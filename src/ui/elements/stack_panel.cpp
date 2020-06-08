// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/stack_panel.h"

/// \file
/// Implementation of stack panels.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.h"

namespace codepad::ui {
	const property_mapping &stack_panel::get_properties() const {
		return get_properties_static();
	}

	const property_mapping &stack_panel::get_properties_static() {
		static property_mapping mapping;
		if (mapping.empty()) {
			mapping = panel::get_properties_static();
			mapping.emplace(u8"orientation", std::make_shared<getter_setter_property<stack_panel, orientation>>(
				u8"orientation",
				[](stack_panel &p) {
					return p.get_orientation();
				},
				[](stack_panel &p, orientation ori) {
					p.set_orientation(ori);
				}
			));
		}

		return mapping;
	}
}
