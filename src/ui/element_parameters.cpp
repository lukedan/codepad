// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/element_parameters.h"

/// \file
/// Implementation of classes and structs used to define the layout and visual parameters of a
/// \ref codepad::ui::element.

namespace codepad::ui {
	namespace transform_parameters {
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
}
