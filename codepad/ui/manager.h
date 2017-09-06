#pragma once

#include <map>
#include <set>
#include <deque>
#include <chrono>

#include "element.h"

namespace codepad {
	namespace ui {
		class manager {
			friend class element;
		public:
			constexpr static double relayout_time_redline = 10.0, render_time_redline = 40.0;

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

			void schedule_update(element &e) {
				_upd.insert(&e);
			}
			void update_scheduled_elements() {
				_lasttp = _now;
				_now = std::chrono::high_resolution_clock::now();
				if (!_upd.empty()) {
					std::set<element*> list;
					std::swap(list, _upd);
					for (auto i = list.begin(); i != list.end(); ++i) {
						(*i)->_on_update();
					}
				}
			}
			double delta_time() const {
				return std::chrono::duration<double>(_now - _lasttp).count();
			}

			// may be called on one element multiple times before the element's disposed
			void mark_disposal(element &e) {
				_del.insert(&e);
			}
			void dispose_marked_elements() {
				while (!_del.empty()) {
					std::set<element*> batch;
					std::swap(batch, _del);
					for (auto i = batch.begin(); i != batch.end(); ++i) {
#ifdef CP_DETECT_LOGICAL_ERRORS
						++control_dispose_rec::get().reg_disposed;
#endif
						(*i)->_dispose();
						_targets.erase(*i);
						_dirty.erase(*i);
						_upd.erase(*i);
						_del.erase(*i);
#ifdef CP_DETECT_USAGE_ERRORS
						assert_true_usage(!(*i)->_initialized, "element::_dispose() must be invoked by children classses");
#endif
						delete *i;
					}
				}
			}

			void update_layout_and_visual() {
				update_invalid_layouts();
				update_invalid_visuals();
			}
			void update() {
				dispose_marked_elements();
				update_scheduled_elements();
				update_layout_and_visual();
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
			bool _layouting = false;
			std::map<element*, bool> _targets;
			std::deque<_layout_info> _q;
			// visual
			std::set<element*> _dirty;
			// focus
			element *_focus = nullptr;
			// scheduled controls to update
			std::chrono::time_point<std::chrono::high_resolution_clock> _lasttp, _now;
			std::set<element*> _upd;
			// scheduled controls to delete
			std::set<element*> _del;
		};

		inline void element::invalidate_layout() {
			manager::get().invalidate_layout(*this);
		}
		inline void element::revalidate_layout() {
			manager::get().revalidate_layout(*this);
		}
		inline void element::invalidate_visual() {
			manager::get().invalidate_visual(*this);
		}
		inline bool element::has_focus() const {
			return manager::get().get_focused() == this;
		}
		inline void element::_on_mouse_down(mouse_button_info &p) {
			mouse_down(p);
			if (_can_focus) {
				p.mark_focus_set();
				manager::get().set_focus(this);
			}
		}
		inline void element::_on_render() {
			if (!_rst.stationary()) {
				_rst.update(manager::get().delta_time());
			}
			if (test_bit_all(_vis, visibility::render_only)) {
				_on_prerender();
				_rst.render(get_layout());
				_custom_render();
				_on_postrender();
			}
			if (!_rst.stationary()) {
				invalidate_visual();
			}
		}
	}
}
