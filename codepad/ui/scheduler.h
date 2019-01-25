#pragma once

/// \file
/// Classes used to schedule the updating and rendering of elements.

#include <set>
#include <deque>
#include <chrono>

#include "element.h"
#include "panel.h"
#include "window.h"

namespace codepad::ui {
	/// Schedules the updating and rendering of all elements.
	class scheduler {
		friend window_base;
	public:
		constexpr static double
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline = 0.01,
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline = 0.04;

		/// Invalidates the layout of an element. Its parent will be notified to recalculate its layout.
		void invalidate_layout(element &e) {
			// TODO maybe optimize for panels
			if (e.parent() != nullptr) {
				invalidate_children_layout(*e.parent());
			}
		}
		/// Invalidates the layout of all children of a \ref panel_base.
		void invalidate_children_layout(panel_base &p) {
			_children_layout_scheduled.emplace(&p);
		}
		/// Marks the element for layout validation, meaning that its layout is valid but
		/// \ref element::_on_layout_changed() has not been called. Like \ref invalidate_layout, different operation
		/// will be performed depending on whether layout is in progress.
		void notify_layout_change(element &e) {
			assert_true_logical(!_layouting, "layout notifications are handled automatically");
			_layout_notify.emplace(&e);
		}
		/// Calculates the layout of all elements with invalidated layout.
		/// The calculation is recursive; that is, after a parent's layout has been changed,
		/// all its children are automatically marked for layout calculation.
		void update_invalid_layout() {
			if (_children_layout_scheduled.empty() && _layout_notify.empty()) {
				return;
			}
			performance_monitor mon(CP_HERE, relayout_time_redline);
			assert_true_logical(!_layouting, "update_invalid_layout() cannot be called recursively");
			_layouting = true;
			std::deque<element*> notify(_layout_notify.begin(), _layout_notify.end()); // list of elements to be notified
			_layout_notify.clear();
			// gather the list of elements with invalidated layout
			std::set<panel_base*> childrenupdate;
			swap(childrenupdate, _children_layout_scheduled);
			for (panel_base *pnl : childrenupdate) {
				pnl->_on_update_children_layout();
				for (element *elem : pnl->_children.items()) {
					notify.emplace_back(elem);
				}
			}
			while (!notify.empty()) {
				element *li = notify.front();
				notify.pop_front();
				li->_on_layout_changed();
				auto *pnl = dynamic_cast<panel_base*>(li);
				if (pnl != nullptr) {
					for (element *elem : pnl->_children.items()) {
						notify.emplace_back(elem);
					}
				}
			}
			_layouting = false;
		}

		/// Marks the given element for re-rendering. This will re-render the whole window,
		/// but even if the visual of multiple elements in the window is invalidated,
		/// the window is still rendered once.
		void invalidate_visual(element &e) {
			_dirty.insert(&e);
		}
		/// Re-renders the windows that contain elements whose visuals are invalidated.
		void update_invalid_visuals() {
			if (_dirty.empty()) {
				return;
			}
			performance_monitor mon(CP_HERE, render_time_redline);
			auto now = std::chrono::high_resolution_clock::now();
			double diff = std::chrono::duration<double>(now - _lastrender).count();
			if (diff < _min_render_interval) {
				// don't render too often
				return;
			}
			_lastrender = now;
			// gather the list of windows to render
			std::set<element*> ss;
			for (auto i : _dirty) {
				window_base *wnd = i->get_window();
				if (wnd) {
					ss.insert(wnd);
				}
			}
			_dirty.clear();
			for (auto i : ss) {
				i->_on_render();
			}
		}
		/// Immediately re-render the window containing the given element.
		/*void update_visual_immediate(element&);*/ // TODO necessary?

		/// Schedules the given element's visual configurations to be updated.
		void schedule_visual_config_update(element &elem) {
			_visualcfg_update.emplace(&elem);
		}
		/// Schedules the given element's \ref metrics_configuration to be updated.
		void schedule_metrics_config_update(element &elem) {
			if (!elem._config.metrics_config.get_state().all_stationary) {
				_metricscfg_update.emplace(&elem);
			}
		}
		/// Schedules the given element to be updated next frame.
		///
		/// \todo Remove this.
		void schedule_update(element &e) {
			_upd.insert(&e);
		}
		/// Updates all elements that are scheduled to be updated by \ref schedule_visual_config_update(),
		/// \ref schedule_metrics_config_update(), and \ref schedule_update().
		void update_scheduled_elements() {
			performance_monitor mon(CP_HERE);
			auto nnow = std::chrono::high_resolution_clock::now();
			_upd_dt = std::chrono::duration<double>(nnow - _lastupdate).count();
			_lastupdate = nnow;

			// from schedule_visual_config_update()
			if (!_visualcfg_update.empty()) {
				std::set<element*> oldset;
				swap(oldset, _visualcfg_update);
				for (element *e : oldset) {
					e->invalidate_visual();
					if (!e->_on_update_visual_configurations(_upd_dt)) {
						schedule_visual_config_update(*e);
					}
				}
			}

			// from schedule_metrics_config_update()
			if (!_metricscfg_update.empty()) {
				std::set<element*> oldset;
				swap(oldset, _metricscfg_update);
				for (element *e : oldset) {
					e->invalidate_layout();
					if (!e->_config.metrics_config.update(_upd_dt)) {
						schedule_metrics_config_update(*e);
					}
				}
			}

			// from schedule_update()
			// TODO remove this?
			if (!_upd.empty()) {
				std::set<element*> list; // the new list
				swap(list, _upd);
				for (auto i : list) {
					i->_on_update();
				}
			}
		}
		/// Returns the amount of time that has passed since the last
		/// time \ref update_scheduled_elements has been called, in seconds.
		double update_delta_time() const {
			return _upd_dt;
		}

		/// Returns the current minimum rendering interval that indicates how long it must be
		/// between two consecutive re-renders.
		double get_minimum_rendering_interval() const {
			return _min_render_interval;
		}
		/// Sets the minimum rendering interval.
		void set_minimum_rendering_interval(double dv) {
			_min_render_interval = dv;
		}

		/// Returns the \ref os::window_base that has the focus.
		window_base *get_focused_window() const {
			return _focus_wnd;
		}
		/// Returns the \ref element that has the focus, or \p nullptr.
		element *get_focused_element() const {
			if (_focus_wnd != nullptr) {
				return _focus_wnd->get_window_focused_element();
			}
			return nullptr;
		}
		/// Sets the currently focused element. When called, this function also interrupts any ongoing composition.
		/// The element must belong to a window. This function should not be called recursively.
		void set_focused_element(element &elem) {
#ifdef CP_CHECK_LOGICAL_ERRORS
			static bool _in = false;

			assert_true_logical(!_in, "recursive calls to set_focused_element");
			_in = true;
#endif
			if (_focus_wnd != nullptr) {
				_focus_wnd->interrupt_input_method();
			}
			window_base *wnd = elem.get_window();
			if (wnd != _focus_wnd) {
				wnd->activate();
			}
			wnd->set_window_focused_element(elem);
#ifdef CP_CHECK_LOGICAL_ERRORS
			_in = false;
#endif
		}

		/// Marks the given element for disposal. The element is only disposed when \ref dispose_marked_elements()
		/// is called. It is safe to call this multiple times before the element's actually disposed.
		void mark_for_disposal(element &e) {
			_del.insert(&e);
		}
		/// Disposes all elements that has been marked for disposal. Other elements that are not marked
		/// previously but are marked for disposal during the process are also disposed.
		void dispose_marked_elements() {
			performance_monitor mon(CP_HERE);
			while (!_del.empty()) { // as long as there are new batches to dispose of
				std::set<element*> batch;
				std::swap(batch, _del);
				// dispose the current batch
				// new batches may be produced during this process
				for (element *elem : batch) {
					elem->_dispose();
#ifdef CP_CHECK_USAGE_ERRORS
					assert_true_usage(!elem->_initialized, "element::_dispose() must be invoked by children classses");
#endif
					// remove the current entry from all lists
					auto *pnl = dynamic_cast<panel_base*>(elem);
					if (pnl) {
						_children_layout_scheduled.erase(pnl);
					}
					_layout_notify.erase(elem);
					_visualcfg_update.erase(elem);
					_metricscfg_update.erase(elem);
					_dirty.erase(elem);
					_del.erase(elem);
					_upd.erase(elem);
					// delete it
					delete elem;
				}
			}
		}

		/// Simply calls \ref update_invalid_layout and \ref update_invalid_visuals.
		void update_layout_and_visual() {
			update_invalid_layout();
			update_invalid_visuals();
		}
		/// Simply calls \ref dispose_marked_elements, \ref update_scheduled_elements,
		/// and \ref update_layout_and_visual.
		void update() {
			performance_monitor mon(CP_STRLIT("Update UI"));
			dispose_marked_elements();
			update_scheduled_elements();
			update_layout_and_visual();
		}
	protected:
		/// Stores the elements whose \ref element::_on_layout_changed() need to be called.
		std::set<element*> _layout_notify;
		/// Stores the panels whose children's layout need computing.
		std::set<panel_base*> _children_layout_scheduled;

		/// Stores the set of elements whose \ref element::_on_update_visual_configurations() are to be called.
		std::set<element*> _visualcfg_update;
		/// Stores the set of elements whose \ref metrics_configuration need updating.
		std::set<element*> _metricscfg_update;

		std::set<element*> _dirty; ///< Stores all elements whose visuals need updating.
		std::set<element*> _del; ///< Stores all elements that are to be disposed of.
		std::set<element*> _upd; ///< Stores all elements that are to be updated.

		/// The time point when elements were last rendered.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastrender;
		/// The time point when elements were last updated.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastupdate;
		double _min_render_interval = 0.0; ///< The minimum interval between consecutive re-renders.
		double _upd_dt = 0.0; ///< The duration since elements were last updated.
		window_base *_focus_wnd = nullptr; ///< Pointer to the currently focused \ref os::window_base.
		bool _layouting = false; ///< Specifies whether layout calculation is underway.

		/// Called when a \ref os::window_base is focused. Sets \ref _focus_wnd accordingly.
		void _on_window_got_focus(window_base &wnd) {
			_focus_wnd = &wnd;
		}
		/// Called when a \ref os::window_base loses the focus. Clears \ref _focus_wnd if necessary.
		void _on_window_lost_focus(window_base &wnd) {
			if (_focus_wnd == &wnd) {
				_focus_wnd = nullptr;
			}
		}
	};
}
