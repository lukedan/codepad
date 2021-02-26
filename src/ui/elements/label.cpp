// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/label.h"

/// \file
/// Implementation of labels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	void label::_custom_render() const {
		element::_custom_render();

		rectd client = get_client_region();
		vec2d offset = client.xmin_ymin() - get_layout().xmin_ymin();
		get_manager().get_renderer().draw_formatted_text(*_formatted_text, offset);
	}

	property_info label::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"label")) {
			if (path.front().property == u8"text_color") {
				return property_info::find_member_pointer_property_info<&label::_text_color, element>(
					path, property_info::make_typed_modification_callback<element, label>([](label &lbl) {
						lbl._on_text_color_changed();
					})
				);
			}
			if (path.front().property == u8"font") {
				return property_info::find_member_pointer_property_info<&label::_font, element>(
					path, property_info::make_typed_modification_callback<element, label>([](label &lbl) {
						constexpr std::size_t size_t_max = std::numeric_limits<std::size_t>::max();
						lbl._formatted_text->set_font_family(lbl.get_font_parameters().family, 0, size_t_max);
						lbl._formatted_text->set_font_size(lbl.get_font_parameters().size, 0, size_t_max);
						lbl._formatted_text->set_font_style(lbl.get_font_parameters().style, 0, size_t_max);
						lbl._formatted_text->set_font_weight(lbl.get_font_parameters().weight, 0, size_t_max);
						lbl._formatted_text->set_font_stretch(lbl.get_font_parameters().stretch, 0, size_t_max);
						lbl._on_text_layout_changed();
					})
				);
			}
			if (path.front().property == u8"horizontal_alignment") {
				return property_info::make_getter_setter_property_info<label, horizontal_text_alignment, element>(
					[](const label &lbl) {
						return lbl.get_horizontal_alignment();
					},
					[](label &lbl, horizontal_text_alignment align) {
						lbl.set_horizontal_alignment(align);
					}
				);
			}
			if (path.front().property == u8"vertical_alignment") {
				return property_info::make_getter_setter_property_info<label, vertical_text_alignment, element>(
					[](const label &lbl) {
						return lbl.get_vertical_alignment();
					},
					[](label &lbl, vertical_text_alignment align) {
						lbl.set_vertical_alignment(align);
					}
				);
			}
			if (path.front().property == u8"text") {
				return property_info::make_getter_setter_property_info<label, std::u8string, element>(
					[](const label &lbl) {
						return lbl.get_text();
					},
					[](label &lbl, std::u8string text) {
						lbl.set_text(std::move(text));
					}
				);
			}
			if (path.front().property == u8"wrapping") {
				return property_info::make_getter_setter_property_info<label, wrapping_mode, element>(
					[](const label &lbl) {
						return lbl.get_wrapping_mode();
					},
					[](label &lbl, wrapping_mode wrap) {
						lbl.set_wrapping_mode(wrap);
					}
				);
			}
			if (path.front().property == u8"wrapping_width_mode") {
				return property_info::make_getter_setter_property_info<label, wrapping_width_mode, element>(
					[](const label &lbl) {
						return lbl.get_wrapping_width_mode();
					},
					[](label &lbl, wrapping_width_mode wrap) {
						lbl.set_wrapping_width_mode(wrap);
					}
				);
			}
			if (path.front().property == u8"wrapping_width") {
				return property_info::make_getter_setter_property_info<label, double, element>(
					[](const label &lbl) {
						return lbl.get_custom_wrapping_width();
					},
					[](label &lbl, double width) {
						lbl.set_custom_wrapping_width(width);
					}
				);
			}
		}
		return element::_find_property_path(path);
	}
}
