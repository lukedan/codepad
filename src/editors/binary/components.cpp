// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/binary/components.h"

/// \file
/// Implementation of additional editor components.

namespace codepad::editors::binary {
	ui::size_allocation primary_offset_display::get_desired_width() const {
		if (auto [edt, rgn] = component_helper::get_core_components(*this); rgn) {
			std::size_t chars = _get_label_length(rgn->get_buffer()->length());
			double maxw = rgn->get_font()->get_maximum_character_width_em(
				reinterpret_cast<const codepoint*>(U"0123456789ABCDEF")
			) * rgn->get_font_size();
			return ui::size_allocation(get_padding().width() + static_cast<double>(chars) * maxw, true);
		}
		return ui::size_allocation(0, true);
	}

	void primary_offset_display::_register_handlers() {
		if (!_events_registered) {
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
	}

	std::u8string primary_offset_display::_to_hex(std::size_t v, std::size_t len) {
		std::u8string res(len, '0');
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

	void primary_offset_display::_custom_render() const {
		element::_custom_render();
		if (auto [edt, rgn] = component_helper::get_core_components(*this); rgn) {
			// position of the first line relative to the window
			double
				top = edt->get_vertical_position() - rgn->get_padding().top,
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
					double ypos = static_cast<double>(firstline) * rgn->get_line_height() - top;
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
}
