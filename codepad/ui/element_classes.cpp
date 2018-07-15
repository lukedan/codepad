#include "element_classes.h"

/// \file
/// Implementation of certain functions about element classes.

#include "manager.h"

namespace codepad::ui {
	element_state_id visual_json_parser::_parse_vid(const json::value_t &val) {
		element_state_id id = normal_element_state_id;
		if (val.IsString()) {
			id = manager::get().get_or_register_state_id(json::get_as_string(val));
		} else if (val.IsArray()) {
			for (auto j = val.Begin(); j != val.End(); ++j) {
				if (j->IsString()) {
					id |= manager::get().get_or_register_state_id(json::get_as_string(*j));
				}
			}
		}
		return id;
	}
}
