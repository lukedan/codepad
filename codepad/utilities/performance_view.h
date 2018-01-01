#pragma once

#include "performance_monitor.h"
#include "../ui/element.h"

namespace codepad {
	namespace performance_view {
		class timeline : public ui::element { // TODO not practical yet
		public:
			constexpr static double width_per_second = 50.0;

			vec2d get_desired_size() const override {
				return vec2d(
					width_per_second * performance_monitor::get().get_log_duration(), 0.0
				) + get_padding().size();
			}

			size_t get_rendered_stack_depth() const {
				return _stkd;
			}
			void set_rendered_stack_depth(size_t v) {
				_stkd = v;
				invalidate_visual();
			}

			inline static str_t get_default_class() {
				return CP_STRLIT("performance_view_timeline");
			}
		protected:
			size_t _stkd = 10;

			void _custom_render() override {
				rectd cln = get_client_region();
				double
					dur = performance_monitor::get().get_log_duration(),
					now = get_uptime().count(), beg = now - dur, stkh = cln.height() / _stkd;
				ui::render_batch batch;
				for (auto &&i : performance_monitor::get().get_recorded_operations()) {
					rectd r(
						cln.xmin + cln.width() * (i.begin_time - beg) / dur,
						cln.xmin + cln.width() * (i.end_time - beg) / dur,
						cln.ymin + stkh * i.stack_depth, 0.0
					);
					r.ymax = r.ymin + stkh;
					batch.add_quad(r, rectd(0.0, 1.0, 0.0, 1.0), colord(0.8, 1.0, 0.8, 1.0));
				}
				batch.draw(os::texture());
			}
		};
	}
}
