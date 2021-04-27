// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/size_limiter.h"

/// \file
/// Implementation of \ref codepad::ui::size_limiter.

namespace codepad::ui {
	void size_limiter::layout_on_direction(
		double &clientmin, double &clientmax,
		size_allocation margin_min, size_allocation size, size_allocation margin_max,
		double minsize, double maxsize
	) {
		double clamped_size = size.value;
		if (!size.is_pixels) {
			double totalpixels = 0.0, totalprop = 0.0;
			margin_min.accumulate_to(totalpixels, totalprop);
			size.accumulate_to(totalpixels, totalprop);
			margin_max.accumulate_to(totalpixels, totalprop);
			clamped_size = ((clientmax - clientmin) - totalpixels) * size.value / totalprop;
		}
		clamped_size = std::clamp(clamped_size, minsize, maxsize);
		panel::layout_on_direction(
			clientmin, clientmax, margin_min, size_allocation::pixels(clamped_size), margin_max
		);
	}

	void size_limiter::layout_child_horizontal(element &child, double xmin, double xmax) const {
		double layout_xmin = xmin;
		double layout_xmax = xmax;
		layout_on_direction(
			layout_xmin, layout_xmax,
			child.get_margin_left(), child.get_layout_width(), child.get_margin_right(),
			_min_size.x, _max_size.x
		);
		_child_set_horizontal_layout(child, layout_xmin, layout_xmax);
	}

	void size_limiter::layout_child_vertical(element &child, double ymin, double ymax) const {
		double layout_ymin = ymin;
		double layout_ymax = ymax;
		layout_on_direction(
			layout_ymin, layout_ymax,
			child.get_margin_top(), child.get_layout_height(), child.get_margin_bottom(),
			_min_size.y, _max_size.y
		);
		_child_set_vertical_layout(child, layout_ymin, layout_ymax);
	}
}
