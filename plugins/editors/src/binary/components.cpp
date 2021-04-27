// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/binary/components.h"

/// \file
/// Implementation of additional editor components.

namespace codepad::editors::binary {
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
		// position of the first line relative to the window
		double
			top = _contents_region->get_editor().get_vertical_position() - _contents_region->get_padding().top,
			right = get_layout().width() - get_padding().right;
		std::size_t
			firstline = static_cast<std::size_t>(
				std::max(0.0, top / _contents_region->get_line_height())
			),
			chars = _get_label_length(_contents_region->get_buffer().length()),
			offset = firstline * _contents_region->get_bytes_per_row();
		auto &renderer = get_manager().get_renderer();

		{
			renderer.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));

			for (
				double ypos = static_cast<double>(firstline) * _contents_region->get_line_height() - top;
				ypos < get_layout().height() && offset < _contents_region->get_buffer().length();
				ypos += _contents_region->get_line_height(), offset += _contents_region->get_bytes_per_row()
			) {
				auto text = renderer.create_plain_text(
					_to_hex(offset, chars), *_contents_region->get_font(), _contents_region->get_font_size()
				);
				// TODO custom color
				renderer.draw_plain_text(*text, vec2d(right - text->get_width(), ypos), colord());
			}

			renderer.pop_clip();
		}
	}

	bool primary_offset_display::_handle_reference(std::u8string_view role, element *elem) {
		if (role == get_contents_region_role()) {
			if (_reference_cast_to(_contents_region, elem)) {
				_contents_region->content_modified += [this]() {
					// when the content is modified, it is possible that the number of digits is changed
					_on_desired_size_changed();
				};
			}
			return true;
		}
		return element::_handle_reference(role, elem);
	}
}
