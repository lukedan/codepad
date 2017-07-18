#pragma once

#include <vector>

#include "../utilities/misc.h"
#include "../os/renderer.h"

namespace codepad {
	namespace ui {
		class basic_brush {
		public:
			virtual ~basic_brush() {
			}

			virtual void fill_rect(rectd) const = 0;
		};
		class texture_brush : public basic_brush {
		public:
			texture_brush() = default;
			texture_brush(colord cv) : color(cv) {
			}
			texture_brush(colord cv, os::texture_id tex) : color(cv), texture(tex) {
			}

			void fill_rect(rectd r) const override {
				vec2d v[6] = {
					r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(),
					r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax()
				};
				const vec2d u[6] = {
					vec2d(0.0, 0.0), vec2d(1.0, 0.0), vec2d(0.0, 1.0),
					vec2d(1.0, 0.0), vec2d(1.0, 1.0), vec2d(0.0, 1.0)
				};
				colord c[6] = {color, color, color, color, color, color};
				os::renderer_base::get().draw_triangles(v, u, c, 6, texture);
			}

			colord color;
			os::texture_id texture = 0;
		};

		class basic_pen {
		public:
			virtual ~basic_pen() {
			}

			virtual void draw_lines(const std::vector<vec2d>&) const = 0;
		};
		class pen : public basic_pen {
		public:
			pen() = default;
			pen(colord c) : color(c) {
			}

			void draw_lines(const std::vector<vec2d> &poss) const override {
				std::vector<colord> cs(poss.size(), color);
				os::renderer_base::get().draw_lines(poss.data(), cs.data(), poss.size());
			}

			colord color;
		};
	}
}
