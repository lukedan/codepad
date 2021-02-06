// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/label.h"

/// \file
/// Implementation of labels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	const property_mapping &label::get_properties() const {
		return get_properties_static();
	}

	const property_mapping &label::get_properties_static() {
		static property_mapping mapping;
		if (mapping.empty()) {
			mapping = element::get_properties_static();
			mapping.emplace(u8"text_color", std::make_shared<member_pointer_property<&label::_text_color>>(
				[](label &lbl) {
					lbl._on_text_color_changed();
				}
			));
			mapping.emplace(u8"font", std::make_shared<member_pointer_property<&label::_font>>(
				[](label &lbl) {
					constexpr std::size_t size_t_max = std::numeric_limits<std::size_t>::max();
					lbl._formatted_text->set_font_family(lbl.get_font_parameters().family, 0, size_t_max);
					lbl._formatted_text->set_font_size(lbl.get_font_parameters().size, 0, size_t_max);
					lbl._formatted_text->set_font_style(lbl.get_font_parameters().style, 0, size_t_max);
					lbl._formatted_text->set_font_weight(lbl.get_font_parameters().weight, 0, size_t_max);
					lbl._formatted_text->set_font_stretch(lbl.get_font_parameters().stretch, 0, size_t_max);
					lbl._on_text_layout_changed();
				}
			));

			mapping.emplace(
				u8"horizontal_alignment",
				std::make_shared<getter_setter_property<label, horizontal_text_alignment>>(
					u8"horizontal_alignment",
					[](label &lbl) {
						return lbl.get_horizontal_alignment();
					},
					[](label &lbl, horizontal_text_alignment align) {
						lbl.set_horizontal_alignment(align);
					}
						)
			);
			mapping.emplace(
				u8"vertical_alignment",
				std::make_shared<getter_setter_property<label, vertical_text_alignment>>(
					u8"vertical_alignment",
					[](label &lbl) {
						return lbl.get_vertical_alignment();
					},
					[](label &lbl, vertical_text_alignment align) {
						lbl.set_vertical_alignment(align);
					}
						)
			);

			mapping.emplace(u8"text", std::make_shared<getter_setter_property<label, std::u8string>>(
				u8"text",
				[](label &lbl) {
					return lbl.get_text();
				},
				[](label &lbl, std::u8string text) {
					lbl.set_text(std::move(text));
				}
				));

			mapping.emplace(u8"wrapping", std::make_shared<getter_setter_property<label, wrapping_mode>>(
				u8"wrapping",
				[](label &lbl) {
					return lbl.get_wrapping_mode();
				},
				[](label &lbl, wrapping_mode mode) {
					lbl.set_wrapping_mode(mode);
				}
				));
			mapping.emplace(
				u8"wrapping_width_mode",
				std::make_shared<getter_setter_property<label, wrapping_width_mode>>(
					u8"wrapping_width_mode",
					[](label &lbl) {
						return lbl.get_wrapping_width_mode();
					},
					[](label &lbl, wrapping_width_mode mode) {
						lbl.set_wrapping_width_mode(mode);
					}
				)
			);
			mapping.emplace(
				u8"wrapping_width",
				std::make_shared<getter_setter_property<label, double>>(
					u8"wrapping_width",
					[](label &lbl) {
						return lbl.get_custom_wrapping_width();
					},
					[](label &lbl, double width) {
						lbl.set_custom_wrapping_width(width);
					}
				)
			);
		}

		return mapping;
	}

	void label::_custom_render() const {
		element::_custom_render();

		rectd client = get_client_region();
		vec2d offset = client.xmin_ymin() - get_layout().xmin_ymin();
		get_manager().get_renderer().draw_formatted_text(*_formatted_text, offset);
	}
}
