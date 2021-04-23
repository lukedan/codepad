// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/size_limiter.h"

/// \file
/// Implementation of \ref codepad::ui::size_limiter.

namespace codepad::ui {
	void size_limiter::layout_on_direction(
		double &clientmin, double &clientmax,
		bool anchormin, bool pixelsize, bool anchormax,
		double marginmin, double size, double marginmax,
		double minsize, double maxsize
	) {
		double clamped_size = size;
		if (!pixelsize) {
			double totalspace = clientmax - clientmin, totalprop = 0.0;
			if (anchormax) {
				totalspace -= marginmax;
			} else {
				totalprop += marginmax;
			}
			if (pixelsize) {
				totalspace -= size;
			} else {
				totalprop += size;
			}
			if (anchormin) {
				totalspace -= marginmin;
			} else {
				totalprop += marginmin;
			}
			clamped_size = totalspace * size / totalprop;
		}
		clamped_size = std::clamp(clamped_size, minsize, maxsize);
		panel::layout_on_direction(
			clientmin, clientmax, anchormin, true, anchormax, marginmin, clamped_size, marginmax
		);
	}

	void size_limiter::layout_child_horizontal(element &child, double xmin, double xmax) const {
		anchor anc = child.get_anchor();
		thickness margin = child.get_margin();
		auto width = child.get_layout_width();
		double layout_xmin = xmin;
		double layout_xmax = xmax;
		layout_on_direction(
			layout_xmin, layout_xmax,
			(anc & anchor::left) != anchor::none, width.is_pixels, (anc & anchor::right) != anchor::none,
			margin.left, width.value, margin.right, _min_size.x, _max_size.x
		);
		_child_set_horizontal_layout(child, layout_xmin, layout_xmax);
	}

	void size_limiter::layout_child_vertical(element &child, double ymin, double ymax) const {
		anchor anc = child.get_anchor();
		thickness margin = child.get_margin();
		auto height = child.get_layout_height();
		double layout_ymin = ymin;
		double layout_ymax = ymax;
		layout_on_direction(
			layout_ymin, layout_ymax,
			(anc & anchor::top) != anchor::none, height.is_pixels, (anc & anchor::bottom) != anchor::none,
			margin.top, height.value, margin.bottom, _min_size.y, _max_size.y
		);
		_child_set_vertical_layout(child, layout_ymin, layout_ymax);
	}
}
