// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "visual.h"

/// \file
/// Implementation of certain methods related to the visuals of objects.

#include "element_classes.h"
#include "element.h"
#include "manager.h"
#include "draw.h"

using namespace std;
using namespace codepad::os;

namespace codepad::ui {
	rectd visual_layer::get_center_rect(const state &s, rectd rgn) const {
		panel_base::layout_on_direction(
			(rect_anchor & anchor::left) != anchor::none,
			width_alloc == size_allocation_type::fixed,
			(rect_anchor & anchor::right) != anchor::none,
			rgn.xmin, rgn.xmax,
			s.margin.current_value.left,
			width_alloc == size_allocation_type::automatic ? 1.0 : s.size.current_value.x,
			s.margin.current_value.right
		);
		panel_base::layout_on_direction(
			(rect_anchor & anchor::top) != anchor::none,
			height_alloc == size_allocation_type::fixed,
			(rect_anchor & anchor::bottom) != anchor::none,
			rgn.ymin, rgn.ymax,
			s.margin.current_value.top,
			height_alloc == size_allocation_type::automatic ? 1.0 : s.size.current_value.y,
			s.margin.current_value.bottom
		);
		return rgn;
	}

	void visual_layer::render(renderer_base &r, rectd rgn, const state &s) const {
		texture empty, *tex = &empty;
		if (s.texture.current_value) {
			tex = s.texture.current_value.get();
		}
		switch (layer_type) {
		case type::solid:
			{
				rectd cln = get_center_rect(s, rgn);
				r.draw_quad(*tex, cln, rectd(0.0, 1.0, 0.0, 1.0), s.color.current_value);
			}
			break;
		case type::grid:
			{
				size_t w = tex->get_width(), h = tex->get_height();
				rectd
					outer = rgn, inner = get_center_rect(s, outer),
					texr(
						s.margin.current_value.left / static_cast<double>(w),
						1.0 - s.margin.current_value.right / static_cast<double>(w),
						s.margin.current_value.top / static_cast<double>(h),
						1.0 - s.margin.current_value.bottom / static_cast<double>(h)
					);
				colord curc = s.color.current_value;
				render_batch rb;
				rb.reserve(18);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, outer.ymin, inner.ymin),
					rectd(0.0, texr.xmin, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, outer.ymin, inner.ymin),
					rectd(texr.xmin, texr.xmax, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, outer.ymin, inner.ymin),
					rectd(texr.xmax, 1.0, 0.0, texr.ymin), curc
				);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, inner.ymin, inner.ymax),
					rectd(0.0, texr.xmin, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, inner.ymin, inner.ymax),
					rectd(texr.xmin, texr.xmax, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, inner.ymin, inner.ymax),
					rectd(texr.xmax, 1.0, texr.ymin, texr.ymax), curc
				);
				rb.add_quad(
					rectd(outer.xmin, inner.xmin, inner.ymax, outer.ymax),
					rectd(0.0, texr.xmin, texr.ymax, 1.0), curc
				);
				rb.add_quad(
					rectd(inner.xmin, inner.xmax, inner.ymax, outer.ymax),
					rectd(texr.xmin, texr.xmax, texr.ymax, 1.0), curc
				);
				rb.add_quad(
					rectd(inner.xmax, outer.xmax, inner.ymax, outer.ymax),
					rectd(texr.xmax, 1.0, texr.ymax, 1.0), curc
				);
				rb.draw(r, *tex);
			}
			break;
		}
	}


	visual_state::state::state(const visual_state &st, animation_time_point_t now) {
		layer_states.reserve(st.layers.size());
		for (auto &i : st.layers) {
			layer_states.emplace_back(i, now);
		}
	}
	visual_state::state::state(const state &old, animation_time_point_t now) {
		layer_states.reserve(old.layer_states.size());
		for (auto j = old.layer_states.begin(); j != old.layer_states.end(); ++j) {
			layer_states.emplace_back(*j, now);
		}
	}


	animation_duration_t visual_state::update(state &s, animation_time_point_t now) const {
		s.all_stationary = true;
		auto res = animation_duration_t::max();
		auto j = s.layer_states.begin();
		for (auto i = layers.begin(); i != layers.end(); ++i, ++j) {
			assert_true_usage(j != s.layer_states.end(), "invalid layer state data");
			res = std::min(res, i->update(*j, now));
			if (!j->all_stationary) {
				s.all_stationary = false;
			}
		}
		return res;
	}
}
