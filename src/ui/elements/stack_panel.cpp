// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/stack_panel.h"

/// \file
/// Implementation of stack panels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	size_allocation stack_panel::get_desired_width() const {
		std::optional<double> value =
			get_orientation() == orientation::vertical ?
			_get_max_horizontal_absolute_desired_span(_children) :
			_get_total_horizontal_absolute_desired_span(_children);
		return size_allocation::pixels(value.value_or(0.0) + get_padding().width());
	}

	size_allocation stack_panel::get_desired_height() const {
		std::optional<double> value =
			get_orientation() == orientation::vertical ?
			_get_total_vertical_absolute_desired_span(_children) :
			_get_max_vertical_absolute_desired_span(_children);
		return size_allocation::pixels(value.value_or(0.0) + get_padding().height());
	}

	property_info stack_panel::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"stack_panel")) {
			if (path.front().property == u8"orientation") {
				return property_info::make_getter_setter_property_info<stack_panel, orientation, element>(
					[](const stack_panel &p) {
						return p.get_orientation();
					},
					[](stack_panel &p, orientation ori) {
						p.set_orientation(ori);
					},
					u8"stack.orientation"
				);
			}
		}
		return panel::_find_property_path(path);
	}
}
