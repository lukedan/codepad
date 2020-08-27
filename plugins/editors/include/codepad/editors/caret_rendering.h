// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs used to render carets and selection regions.

#include <codepad/core/math.h>
#include <codepad/ui/renderer.h>

namespace codepad::editors {
	/// Basic interface for rendering selected regions.
	class selection_renderer {
	public:
		/// Default virtual destructor.
		virtual ~selection_renderer() = default;

		/// Renders a single selected region.
		virtual void render(
			ui::renderer_base&, const std::vector<rectd>&, const ui::generic_brush&, const ui::generic_pen&
		) const = 0;
	};

	/// Renders selected regions with rounded corners.
	class rounded_selection_renderer : public selection_renderer {
	public:
		/// Renders these regions as a single path with round corners.
		void render(
			ui::renderer_base &rend, const std::vector<rectd> &rects,
			const ui::generic_brush &brush, const ui::generic_pen &pen
		) const override {
			if (rects.empty()) {
				return;
			}

			auto &builder = rend.start_path();

			double
				toprx = _half_radius(rects.front().width()),
				topry = _half_radius(rects.front().height());

			builder.move_to(vec2d(rects.front().xmin, rects.front().ymin + topry));
			builder.add_arc(
				vec2d(rects.front().xmin + toprx, rects.front().ymin), vec2d(toprx, topry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(rects.front().xmax - toprx, rects.front().ymin));
			builder.add_arc(
				vec2d(rects.front().xmax, rects.front().ymin + topry), vec2d(toprx, topry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			{ // downwards
				auto last = rects.begin(), cur = last;
				for (++cur; cur != rects.end(); last = cur, ++cur) {
					bool right = cur->xmax > last->xmax;
					double
						rx = _half_radius(std::abs(last->xmax - cur->xmax)),
						posrx = right ? rx : -rx,
						ry1 = _half_radius(last->height()),
						ry2 = _half_radius(cur->height());

					builder.add_segment(vec2d(last->xmax, last->ymax - ry1));
					builder.add_arc(
						vec2d(last->xmax + posrx, last->ymax), vec2d(rx, ry1), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->xmax - posrx, cur->ymin));
					builder.add_arc(
						vec2d(cur->xmax, cur->ymin + ry2), vec2d(rx, ry2), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
				}
			}

			double
				botrx = _half_radius(rects.back().width()),
				botry = _half_radius(rects.back().height());

			builder.add_segment(vec2d(rects.back().xmax, rects.back().ymax - botry));
			builder.add_arc(
				vec2d(rects.back().xmax - botrx, rects.back().ymax), vec2d(botrx, botry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);
			builder.add_segment(vec2d(rects.back().xmin + botrx, rects.back().ymax));
			builder.add_arc(
				vec2d(rects.back().xmin, rects.back().ymax - botry), vec2d(botrx, botry), 0.0,
				ui::sweep_direction::clockwise, ui::arc_type::minor
			);

			{ // upwards
				auto last = rects.rbegin(), cur = last;
				for (++cur; cur != rects.rend(); last = cur, ++cur) {
					bool right = cur->xmin > last->xmin;
					double
						rx = _half_radius(std::abs(cur->xmin - last->xmin)),
						posrx = right ? rx : -rx,
						ry1 = _half_radius(last->height()),
						ry2 = _half_radius(cur->height());

					builder.add_segment(vec2d(last->xmin, last->ymin + ry1));
					builder.add_arc(
						vec2d(last->xmin + posrx, last->ymin), vec2d(rx, ry1), 0.0,
						right ? ui::sweep_direction::clockwise : ui::sweep_direction::counter_clockwise,
						ui::arc_type::minor
					);
					builder.add_segment(vec2d(cur->xmin - posrx, cur->ymax));
					builder.add_arc(
						vec2d(cur->xmin, cur->ymax - ry2), vec2d(rx, ry2), 0.0,
						right ? ui::sweep_direction::counter_clockwise : ui::sweep_direction::clockwise,
						ui::arc_type::minor
					);
				}
			}

			builder.close();

			rend.end_and_draw_path(brush, pen);
		}

		double maximum_radius = 4.0; ///< The maximum radius of the corners.
	protected:
		/// Returns the smaller value between half the input and \ref maximum_radius.
		double _half_radius(double v) const {
			return std::min(0.5 * v, maximum_radius);
		}
	};
}
