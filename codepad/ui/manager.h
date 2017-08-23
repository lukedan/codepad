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

			inline static manager &get() {
				return _sman;
			}

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
				_now = std::chrono::high_resolution_clock::now();
#ifdef CP_DETECT_USAGE_ERRORS
				_updating = true;
#endif
				if (!_upd.empty()) {
					std::set<element*> list;
					std::swap(list, _upd);
					for (auto i = list.begin(); i != list.end(); ++i) {
						(*i)->_on_update();
					}
				}
#ifdef CP_DETECT_USAGE_ERRORS
				_updating = false;
#endif
				_lasttp = _now;
			}
			std::chrono::duration<double> delta_time() const {
#ifdef CP_DETECT_USAGE_ERRORS
				assert_true_usage(_updating, "can only get delta time while updating");
#endif
				return _now - _lasttp;
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
#ifndef NDEBUG
						++_dispose_rec.reg_disposed;
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
#ifdef CP_DETECT_USAGE_ERRORS
			bool _updating = false;
#endif
			// scheduled controls to delete
			std::set<element*> _del;

			static manager _sman;
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
		inline void element::_on_mouse_down(mouse_button_info &p) {
			mouse_down(p);
			if (_can_focus) {
				p.mark_focus_set();
				manager::get().set_focus(this);
			}
		}
		inline bool element::has_focus() const {
			return manager::get().get_focused() == this;
		}
	}
}
