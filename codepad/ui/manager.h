#pragma once

#include <map>
#include <set>
#include <deque>
#include <chrono>

#include "element.h"
#include "../utilities/performance_monitor.h"

namespace codepad {
	namespace ui {
		class manager {
			friend class element;
		public:
			constexpr static double relayout_time_redline = 0.01, render_time_redline = 0.04;

			~manager() {
				dispose_marked_elements();
			}

			static manager &get();

			void invalidate_layout(element &e) {
				if (_layouting) {
					_q.push_back(_layout_info(&e, true));
				} else {
					_targets[&e] = true;
				}
			}
			void revalidate_layout(element &e) {
				if (_layouting) {
					_q.push_back(_layout_info(&e, false));
				} else {
					if (_targets.find(&e) == _targets.end()) {
						_targets.insert(std::make_pair(&e, false));
					}
				}
			}
			void update_invalid_layouts();

			void invalidate_visual(element &e) {
				_dirty.insert(&e);
			}
			void update_invalid_visuals();
			void update_visual_immediate(element&);

			void schedule_update(element &e) {
				_upd.insert(&e);
			}
			void update_scheduled_elements();
			double update_delta_time() const {
				return _upd_dt;
			}

			// may be called on one element multiple times before the element's disposed
			void mark_disposal(element &e) {
				_del.insert(&e);
			}
			void dispose_marked_elements();

			void update_layout_and_visual() {
				update_invalid_layouts();
				update_invalid_visuals();
			}
			void update() {
				monitor_performance mon(CP_STRLIT_U8("Update UI"));
				dispose_marked_elements();
				update_scheduled_elements();
				update_layout_and_visual();
			}

			double get_minimum_rendering_interval() const {
				return _min_render_interval;
			}
			void set_minimum_rendering_interval(double dv) {
				_min_render_interval = dv;
			}

			element *get_focused() const {
				return _focus;
			}
			void set_focus(element*);
		protected:
			struct _layout_info {
				_layout_info() = default;
				_layout_info(element *el, bool v) : elem(el), need_recalc(v) {
				}
				element *elem;
				bool need_recalc;
			};

			// layout
			std::map<element*, bool> _targets;
			std::deque<_layout_info> _q;
			bool _layouting = false;
			// scheduled elements to render
			std::set<element*> _dirty;
			std::chrono::time_point<std::chrono::high_resolution_clock> _lastrender;
			double _min_render_interval = 0.0;
			// scheduled controls to delete
			std::set<element*> _del;
			// scheduled controls to update
			std::set<element*> _upd;
			std::chrono::time_point<std::chrono::high_resolution_clock> _now;
			double _upd_dt = 0.0;
			// focus
			element *_focus = nullptr;
		};
	}
}
