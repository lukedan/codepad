// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/decoration.h"

/// \file
/// Classes used to store and render text decoration.

namespace codepad::editors {
	namespace decoration_renderers {
		void rounded_renderer::render(ui::renderer_base &rend, const decoration_layout &deco, vec2d client) const {
			if (deco.line_bounds.empty()) {
				return;
			}

			auto &builder = rend.start_path();

			double
				toprx = _half_radius(deco.line_bounds.front().second - deco.line_bounds.front().first),
				ry = _half_radius(deco.line_height);

			builder.move_to(vec2d(deco.line_bounds.front().first, deco.top + ry));
			builder.add_arc(
				vec2d(deco.line_bounds.front().first + toprx, deco.top), vec2d(toprx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(deco.line_bounds.front().second - toprx, deco.top));
			builder.add_arc(
				vec2d(deco.line_bounds.front().second, deco.top + ry), vec2d(toprx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			double y = deco.top + deco.line_height;
			{ // downwards
				auto last = deco.line_bounds.begin(), cur = last;
				for (++cur; cur != deco.line_bounds.end(); last = cur, ++cur, y += deco.line_height) {
					bool right = cur->second > last->second;
					double
						rx = _half_radius(std::abs(last->second - cur->second)),
						posrx = right ? rx : -rx;

					builder.add_segment(vec2d(last->second, y - ry));
					builder.add_arc(
						vec2d(last->second + posrx, y), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->second - posrx, y));
					builder.add_arc(
						vec2d(cur->second, y + ry), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
				}
			}

			double botrx = _half_radius(deco.line_bounds.back().second - deco.line_bounds.back().first);

			builder.add_segment(vec2d(deco.line_bounds.back().second, y - ry));
			builder.add_arc(
				vec2d(deco.line_bounds.back().second - botrx, y), vec2d(botrx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(deco.line_bounds.back().first + botrx, y));
			builder.add_arc(
				vec2d(deco.line_bounds.back().first, y - ry), vec2d(botrx, ry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			{ // upwards
				y -= deco.line_height;
				auto last = deco.line_bounds.rbegin(), cur = last;
				for (++cur; cur != deco.line_bounds.rend(); last = cur, ++cur, y -= deco.line_height) {
					bool right = cur->first > last->first;
					double
						rx = _half_radius(std::abs(cur->first - last->first)),
						posrx = right ? rx : -rx;

					builder.add_segment(vec2d(last->first, y + ry));
					builder.add_arc(
						vec2d(last->first + posrx, y), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->first - posrx, y));
					builder.add_arc(
						vec2d(cur->first, y - ry), vec2d(rx, ry), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
				}
			}

			builder.close();

			rend.end_and_draw_path(brush.get_parameters(client), pen.get_parameters(client));
		}
	}
}
