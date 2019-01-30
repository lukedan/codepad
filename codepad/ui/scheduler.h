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
	/// Schedules the updating and rendering of all elements. There can only be one active object of this type per
	/// thread.
	class scheduler {
		friend window_base;
		friend element_collection;
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

		/// Sets the currently focused element. When called, this function also interrupts any ongoing composition.
		/// The element must belong to a window. This function should not be called recursively.
		void set_focused_element(element *elem) {
#ifdef CP_CHECK_LOGICAL_ERRORS
			static bool _in = false;

			assert_true_logical(!_in, "recursive calls to set_focused_element");
			_in = true;
#endif

			element *newfocus = elem;
			while (true) { // handle nested focus scopes
				if (auto scope = dynamic_cast<panel_base*>(newfocus); scope && scope->is_focus_scope()) {
					element *in_scope = scope->get_focused_element_in_scope();
					if (in_scope && in_scope != newfocus) {
						newfocus = in_scope;
						continue;
					}
				}
				break;
			}
			if (newfocus != _focus) { // actually set focused element
				std::vector<element_hotkey_group_data> gps;
				if (newfocus) {
					// update hotkey groups
					for (element *cur = newfocus; cur; cur = cur->parent()) {
						gps.emplace_back(cur->_config.hotkey_config, cur);
					}
					// update scope focus on root path
					element *scope_focus = newfocus;
					for (panel_base *scp = newfocus->parent(); scp; scp = scp->parent()) {
						if (scp->is_focus_scope()) {
							scp->_scope_focus = scope_focus;
							scope_focus = scp;
						}
					}
				}
				_hotkeys.reset_groups(gps);
				// cache & change focus
				element *oldfocus = _focus;
				_focus = newfocus;
				// invoke events
				if (oldfocus) {
					oldfocus->_on_lost_focus();
				}
				if (newfocus) {
					newfocus->_on_got_focus();
				}
				logger::get().log_verbose(
					CP_HERE, "focus changed from ",
					oldfocus, " <", oldfocus ? demangle(typeid(*oldfocus).name()) : "empty", "> to ",
					_focus, " <", _focus ? demangle(typeid(*_focus).name()) : "empty", ">"
				);
			}

#ifdef CP_CHECK_LOGICAL_ERRORS
			_in = false;
#endif
		}
		/// Returns the focused element.
		element *get_focused_element() const {
			return _focus;
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

		/// Returns the \ref hotkey_listener.
		hotkey_listener &get_hotkey_listener() {
			return _hotkeys;
		}
		/// \overload
		const hotkey_listener &get_hotkey_listener() const {
			return _hotkeys;
		}
	protected:
		hotkey_listener _hotkeys; ///< Handles hotkeys.

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
		element *_focus = nullptr; ///< Pointer to the currently focused \ref element.
		bool _layouting = false; ///< Specifies whether layout calculation is underway.

		/// Finds the focus scope that the given \ref element is in. The element itself is not taken into account.
		/// Returns \p nullptr if the element is not in any scope (which should only happen for windows).
		panel_base *_find_focus_scope(element &e) const {
			panel_base *scope = e.parent(); // innermost focus scope
			for (; scope && !scope->is_focus_scope(); scope = scope->parent()) {
			}
			return scope;
		}
		/// Called by \ref element_collection when an element is about to be removed from it. This function updates
		/// the innermost focus scopes as well as the global focus.
		void _on_removing_element(element &e) {
			panel_base *scope = _find_focus_scope(e);
			if (scope) {
				if (element *sfocus = scope->get_focused_element_in_scope()) {
					for (element *f = sfocus; f && f != scope; f = f->parent()) {
						if (f == &e) { // focus is removed from the scope
							scope->_scope_focus = nullptr;
							break;
						}
					}
				}
			}
			// the _scope_focus field is read in set_focus_element() only to find the innermost focused element,
			// which should mean that setting it to nullptr in advance as above is ok

			for (element *gfocus = _focus; gfocus; gfocus = gfocus->parent()) { // check if global focus is removed
				if (gfocus == &e) { // yes it is
					panel_base *newfocus = e.parent();
					for (
						;
						newfocus && !newfocus->get_can_focus() && !newfocus->is_focus_scope();
						newfocus = newfocus->parent()
						) {
					}
					set_focused_element(newfocus); // may be nullptr, which is ok
					break;
				}
				// otherwise, only the focus in that scope (which is not in effect) has changed, and nothing needs
				// to be done
			}
		}
	};
}
