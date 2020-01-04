// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional built-in components of the binary editor.

#include "contents_region.h"

namespace codepad::editors::binary {
	/// Used to display the offset for each line in the binary editor.
	class primary_offset_display : public ui::element {
	public:
		/// Returns the desired with.
		ui::size_allocation get_desired_width() const override {
			if (auto [edt, rgn] = component_helper::get_core_components(*this); rgn) {
				std::size_t chars = _get_label_length(rgn->get_buffer()->length());
				double maxw = rgn->get_font()->get_maximum_character_width_em(
					reinterpret_cast<const codepoint*>(U"0123456789ABCDEF")
				) * rgn->get_font_size();
				return ui::size_allocation(get_padding().width() + chars * maxw, true);
			}
			return ui::size_allocation(0, true);
		}

		/// Returns the default class of elements of type \ref primary_offset_display.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("primary_offset_display");
		}
	protected:
		bool _events_registered = false; ///< Marks if event handlers have been registered.

		/// Registers \ref _vis_change_tok if a \ref contents_region can be found.
		void _register_handlers() {
			if (auto [edt, rgn] = component_helper::get_core_components(*this); rgn) {
				_events_registered = true;
				rgn->content_modified += [this]() {
					// when the content is modified, it is possible that the number of digits is changed
					_on_desired_size_changed(true, false);
				};
				edt->vertical_viewport_changed += [this]() {
					get_manager().get_scheduler().invalidate_visual(*this);
				};
			}
		}
		/// Calls \ref _register_handlers().
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
			_register_handlers();
		}
		/// Calls \ref _register_handlers() if necessary.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			if (!_events_registered) {
				_register_handlers();
			}
		}

		/// Returns the label length that corresponds to the given buffer size.
		inline static std::size_t _get_label_length(std::size_t len) {
			return 1 + high_bit_index(std::max<std::size_t>(len, 1)) / 4;
		}

		/// Returns the hexadecimal representation of the given number, padded to the given length with zeros.
		inline static str_t _to_hex(std::size_t v, std::size_t len) {
			str_t res(len, '0');
			for (auto x = res.rbegin(); v != 0 && x != res.rend(); ++x, v >>= 4) {
				std::size_t digit = v & 0xF;
				if (digit < 10) {
					*x = static_cast<char>(digit + '0'); // TODO use char8_t
				} else {
					*x = static_cast<char>(digit + 'A' - 10);
				}
			}
			return res;
		}
		/// Renders the offsets.
		void _custom_render() const override {
			element::_custom_render();
			if (auto [edt, rgn] = component_helper::get_core_components(*this); rgn) {
				// position of the first line relative to the window
				double
					top = edt->get_vertical_position() - rgn->get_padding().top,
					left = get_client_region().xmin,
					right = get_layout().width() - get_padding().right;
				std::size_t
					firstline = static_cast<std::size_t>(
						std::max(0.0, top / rgn->get_line_height())
						),
					chars = _get_label_length(rgn->get_buffer()->length()),
					offset = firstline * rgn->get_bytes_per_row();
				auto &renderer = get_manager().get_renderer();

				{
					ui::pixel_snapped_render_target buffer(
						renderer,
						rectd::from_corners(vec2d(), get_layout().size()),
						get_window()->get_scaling_factor()
					);

					for (
						double ypos = firstline * rgn->get_line_height() - top;
						ypos < get_layout().height() && offset < rgn->get_buffer()->length();
						ypos += rgn->get_line_height(), offset += rgn->get_bytes_per_row()
						) {
						auto text = renderer.create_plain_text(
							_to_hex(offset, chars), *rgn->get_font(), rgn->get_font_size()
						);
						// TODO custom color
						renderer.draw_plain_text(*text, vec2d(right - text->get_width(), ypos), colord());
					}
				}
			}
		}
	};
}
