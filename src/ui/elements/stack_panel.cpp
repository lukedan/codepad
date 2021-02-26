// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/stack_panel.h"

/// \file
/// Implementation of stack panels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	size_allocation stack_panel::get_desired_width() const {
		double val = 0.0;
		for (element *e : _children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = _get_horizontal_absolute_span(*e)) {
					val =
						get_orientation() == orientation::vertical ?
						std::max(val, span.value()) :
						val + span.value();
				}
			}
		}
		return size_allocation::pixels(val + get_padding().width());
	}

	size_allocation stack_panel::get_desired_height() const {
		double val = 0.0;
		for (element *e : _children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = _get_vertical_absolute_span(*e)) {
					val =
						get_orientation() == orientation::vertical ?
						val + span.value() :
						std::max(val, span.value());
				}
			}
		}
		return size_allocation::pixels(val + get_padding().height());
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
					}
				);
			}
		}
		return panel::_find_property_path(path);
	}
}
