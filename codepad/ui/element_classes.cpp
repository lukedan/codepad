// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "element_classes.h"

/// \file
/// Implementation of certain functions about element classes.

#include "manager.h"

namespace codepad::ui {
	element_state_id ui_config_parser::_parse_state_id(const json::value_t &val, bool configonly) {
		element_state_id id = normal_element_state_id;
		if (val.IsString()) {
			id = manager::get().get_state_info(json::get_as_string(val)).id;
		} else if (val.IsArray()) {
			for (auto j = val.Begin(); j != val.End(); ++j) {
				if (j->IsString()) {
					element_state_info st = manager::get().get_state_info(json::get_as_string(*j));
					if (configonly && st.type != element_state_type::configuration) {
						logger::get().log_warning(CP_HERE, "non-config state bit encountered in config-only state");
					} else {
						id |= st.id;
					}
				}
			}
		} else {
			logger::get().log_warning(CP_HERE, "invalid state ID format");
		}
		return id;
	}
}
