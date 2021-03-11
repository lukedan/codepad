// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementations of all JSON parsers in \ref codepad::editors.

#include "code/theme.h"
#include "theme_manager.h"

namespace codepad::ui {
	template <typename Value> std::optional<
		editors::code::text_theme_specification
	> managed_json_parser<editors::code::text_theme_specification>::operator()(const Value &val) const {
		if (auto obj = val.template try_cast<typename Value::object_type>()) {
			editors::code::text_theme_specification result;
			if (auto clr = obj->template parse_optional_member<colord>(
				u8"color", managed_json_parser<colord>(_manager)
			)) {
				result.color = clr.value();
			}
			if (auto style = obj->template parse_optional_member<font_style>(u8"style")) {
				result.style = style.value();
			}
			if (auto weight = obj->template parse_optional_member<font_weight>(u8"weight")) {
				result.weight = weight.value();
			}
			return result;
		} else if (auto clr = val.template try_parse<colord>(managed_json_parser<colord>(_manager))) {
			return editors::code::text_theme_specification(clr.value(), font_style::normal, font_weight::normal);
		}
		val.template log<log_level::error>(CP_HERE) << "invalid text_theme_specification format";
		return std::nullopt;
	}


	template <typename Value> std::optional<
		editors::theme_configuration
	> managed_json_parser<editors::theme_configuration>::operator()(const Value &val) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			editors::theme_configuration result;
			for (auto it = obj->member_begin(); it != obj->member_end(); ++it) {
				if (auto spec = it.value().template parse<editors::code::text_theme_specification>(
					managed_json_parser<editors::code::text_theme_specification>(_manager)
				)) {
					result.add_entry(it.name(), spec.value());
				}
			}
			return result;
		}
		val.template log<log_level::error>(CP_HERE) << "invalid theme_configuration format";
		return std::nullopt;
	}
}
