// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/label.h"

/// \file
/// Implementation of labels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.h"

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
					lbl._on_text_layout_changed();
				}
			));
		}

		return mapping;
	}

	void label::_custom_render() const {
		element::_custom_render();

		rectd client = get_client_region();
		vec2d offset = client.xmin_ymin() - get_layout().xmin_ymin();
		get_manager().get_renderer().draw_formatted_text(*_cached_fmt, offset);
	}

	void label::_check_cache_format() const {
		if (!_cached_fmt) {
			rectd client = get_client_region();
			_cached_fmt = get_manager().get_renderer().create_formatted_text(
				get_text(), _font, _text_color, client.size(), wrapping_mode::none,
				horizontal_text_alignment::front, vertical_text_alignment::top
			);
		}
	}
}
