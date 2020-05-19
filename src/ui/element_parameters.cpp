// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "element_parameters.h"

/// \file
/// Implementation of classes and structs used to define the layout and visual parameters of a
/// \ref codepad::ui::element.

namespace codepad::ui {
	namespace transforms {
		matd3x3 collection::get_matrix(vec2d unit) const {
			matd3x3 res = matd3x3::identity();
			for (const generic &g : components) {
				res = g.get_matrix(unit) * res;
			}
			return res;
		}

		vec2d collection::transform_point(vec2d pt, vec2d unit) const {
			for (const generic &g : components) {
				pt = g.transform_point(pt, unit);
			}
			return pt;
		}

		vec2d collection::inverse_transform_point(vec2d pt, vec2d unit) const {
			for (auto it = components.rbegin(); it != components.rend(); ++it) {
				pt = it->inverse_transform_point(pt, unit);
			}
			return pt;
		}
	}


	element_configuration::event_identifier element_configuration::event_identifier::parse_from_string(
		std::u8string_view s
	) {
		if (auto it = s.begin(); it != s.end()) {
			for (codepoint cp; it != s.end(); ) {
				auto begin = it;
				if (encodings::utf8::next_codepoint(it, s.end(), cp)) {
					if (cp == U'.') { // separator, only consider the first one
						return event_identifier(std::u8string(s.begin(), begin), std::u8string(it, s.end()));
					}
				}
			}
		}
		return event_identifier(std::u8string(s));
	}
}
