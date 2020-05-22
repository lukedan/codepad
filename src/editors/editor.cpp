// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "editor.h"

/// \file
/// Implementation of the editor.

namespace codepad::editors {
	const ui::property_mapping &contents_region_base::get_properties() const {
		return get_properties_static();
	}

	const ui::property_mapping &contents_region_base::get_properties_static() {
		static ui::property_mapping mapping;
		if (mapping.empty()) {
			mapping = ui::element::get_properties_static();
			mapping.emplace(
				u8"caret_visuals",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_caret_visuals>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
			mapping.emplace(
				u8"selection_brush",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_selection_brush>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
			mapping.emplace(
				u8"selection_pen",
				std::make_shared<ui::member_pointer_property<&contents_region_base::_selection_pen>>(
					[](contents_region_base &c) {
						c.invalidate_visual();
					}
					)
			);
		}

		return mapping;
	}
}
