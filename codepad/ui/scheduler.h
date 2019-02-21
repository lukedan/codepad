// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to schedule the updating and rendering of elements.

#include <set>
#include <list>
#include <deque>
#include <chrono>
#include <functional>

#include "element.h"
#include "panel.h"
#include "window.h"

namespace codepad::ui {
	/// Schedules the updating and rendering of all elements. There should at most be one active object of this type
	/// per thread.
	class scheduler {
		friend window_base;
		friend element_collection;
	public:
		constexpr static std::chrono::duration<double>
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline{0.01},
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline{0.04};

		/// Specifies if an operation should be blocking (synchronous) or non-blocking (asynchronous).
		enum class wait_type {
			blocking, ///< This operation is synchronous.
			non_blocking ///< This operation is asynchronous.
		};
		/// Stores a task that can be executed every update.
		struct update_task {
			/// A token that through whcih the associated \ref update_task can be scheduled.
			struct token {
				friend scheduler;
			public:
				/// Default constructor.
				token() = default;
			protected:
				using _iterator_t = std::list<update_task>::iterator; ///< Type of the iterator.

				/// Constructs this token with the corresponding iterator.
				explicit token(_iterator_t it) : _it(it) {
				}
				_iterator_t _it; ///< Iterator to the task node.
			};

			/// Default constructor.
			update_task() = default;
			/// Initializes this task with the corresponding function.
			explicit update_task(std::function<void()> t) : task(std::move(t)) {
			}

			std::function<void()> task; ///< The function to be executed.
			bool needs_update = false; ///< Marks if \ref task needs to be executed next update.
		};

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
		/// \ref element::_on_layout_changed() has not been called.
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
			performance_monitor mon(CP_STRLIT("layout"), relayout_time_redline);
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
			performance_monitor mon("render", render_time_redline);
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
			_reset_update_estimate();
		}
		/// Schedules the given element's \ref metrics_configuration to be updated.
		void schedule_metrics_config_update(element &elem) {
			if (!elem._config.metrics_config.get_state().all_stationary) {
				_metricscfg_update.emplace(&elem);
				_reset_update_estimate();
			}
		}
		/// Schedules the given element to be updated next frame.
		void schedule_element_update(element &e) {
			_upd.insert(&e);
		}
		/// Registers a task to be executed periodically and on demand.
		update_task::token register_update_task(std::function<void()> f) {
			_regular_tasks.emplace_back(std::move(f));
			return update_task::token(--_regular_tasks.end());
		}
		/// Unregisters a \ref update_task. This function should *not* be called during the execution of an
		/// \ref update_task. Instead, add a temporary update task to do it, and it'll immediately be executed.
		void unregister_update_task(update_task::token tok) {
			if (tok._it->needs_update) {
				--_active_update_tasks;
			}
			_regular_tasks.erase(tok._it);
		}
		/// Schedules the \ref update_task represented by the given token to be executed once next update. The task
		/// is executed once per update even if this function is called multiple times between two updates.
		void schedule_update_task(update_task::token tok) {
			if (!tok._it->needs_update) {
				tok._it->needs_update = true;
				++_active_update_tasks;
			}
		}
		/// Schedules the given \p std::function to be executed once next update. These are executed immediately
		/// after regular update tasks.
		void schedule_temporary_update_task(std::function<void()> f) {
			_temp_tasks.emplace_back(std::move(f));
		}
		/// Executes temporary and non-temporary update tasks.
		void update_tasks() {
			// non-temporary
			if (_active_update_tasks > 0) {
				std::vector<std::function<void()>*> execs;
				execs.reserve(_active_update_tasks); // should be just right
				for (auto &task : _regular_tasks) {
					if (task.needs_update) {
						execs.emplace_back(&task.task);
						task.needs_update = false;
					}
				}
				assert_true_logical(execs.size() == _active_update_tasks, "wrong number of update tasks");
				_active_update_tasks = 0;
				// clean slate now
				// TODO maybe add indicator and check in unregister_update_task?
				for (auto *f : execs) {
					(*f)();
				}
			}

			// temporary
			std::vector<std::function<void()>> lst;
			std::swap(lst, _temp_tasks);
			for (auto &func : lst) {
				func();
			}
		}
		/// Updates all elements that are scheduled to be updated by \ref schedule_visual_config_update(),
		/// \ref schedule_metrics_config_update(), and \ref schedule_element_update().
		void update_scheduled_elements() {
			performance_monitor mon("update_misc");

			auto aninow = animation_clock_t::now();

			if (aninow >= _next_update) { // only update when necessary
				animation_duration_t wait_time = animation_duration_t::max();

				// from schedule_visual_config_update()
				if (!_visualcfg_update.empty()) {
					std::set<element*> oldset;
					swap(oldset, _visualcfg_update);
					for (element *e : oldset) {
						animation_update_info info(aninow);
						e->invalidate_visual();
						e->_on_update_visual_configurations(info);
						if (!info.is_stationary()) {
							wait_time = std::min(wait_time, info.get_wait_time());
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
						wait_time = std::min(wait_time, e->_config.metrics_config.update(aninow));
						if (!e->_config.metrics_config.get_state().all_stationary) {
							schedule_metrics_config_update(*e);
						}
					}
				}

				_next_update = aninow + wait_time;
				if (wait_time > _update_wait_threshold) { // set timer
					// the update may have taken some time
					_set_timer(_next_update - std::chrono::high_resolution_clock::now());
				}
			}

			auto nnow = std::chrono::high_resolution_clock::now();
			_upd_dt = std::chrono::duration<double>(nnow - _last_update).count();
			_last_update = nnow;

			// from schedule_element_update()
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
			performance_monitor mon("dispose_elements");
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

		/// Returns \ref _update_wait_threshold.
		std::chrono::high_resolution_clock::duration get_update_waiting_threshold() const {
			return _update_wait_threshold;
		}
		/// Sets the minimum time to wait instead of 
		void set_update_waiting_threshold(std::chrono::high_resolution_clock::duration d) {
			_update_wait_threshold = d;
		}

		/// Simply calls \ref update_invalid_layout and \ref update_invalid_visuals.
		void update_layout_and_visual() {
			update_invalid_layout();
			update_invalid_visuals();
		}
		/// Calls \ref update_tasks(), \ref dispose_marked_elements, \ref update_scheduled_elements,
		/// and \ref update_layout_and_visual.
		void update() {
			performance_monitor mon(CP_STRLIT("Update UI"));
			update_tasks();
			dispose_marked_elements();
			update_scheduled_elements();
			update_layout_and_visual();
		}

		/// Returns whether \ref update() needs to be called right now.
		bool needs_update() const {
			return
				_active_update_tasks > 0 || !_temp_tasks.empty() || // tasks
				!_del.empty() || // element disposal
				( // element config update
					!(_visualcfg_update.empty() && _metricscfg_update.empty()) &&
					_next_update <= std::chrono::high_resolution_clock::now() + _update_wait_threshold
					) ||
				!_upd.empty() || // element update
				!_children_layout_scheduled.empty() || !_layout_notify.empty() || // layout
				!_dirty.empty(); // visual
		}

		/// If any internal update is necessary, calls \ref update(), then calls \ref _idle_system() with
		/// \ref wait_type::non_blocking until no more messages can be processed. Otherwise, waits and handles a
		/// single message from the system by calling \ref _idle_system() with \ref wait_type::blocking.
		void idle_loop_body() {
			if (needs_update()) {
				update();
				while (_idle_system(wait_type::non_blocking)) {
				}
			} else {
				_idle_system(wait_type::blocking);
				_last_update = std::chrono::high_resolution_clock::now();
			}
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

		std::list<update_task> _regular_tasks; ///< The list of registered update tasks.
		std::vector<std::function<void()>> _temp_tasks; ///< The list of temporary tasks.

		std::chrono::high_resolution_clock::time_point
			/// The time point when elements were last updated.
			_last_update,
			/// The time point of the next time when updating will be necessary.
			_next_update;
		/// If the next update is more than this amount of time away, then set the timer and yield control to reduce
		/// resource consumption.
		std::chrono::high_resolution_clock::duration _update_wait_threshold{0};

		double _upd_dt = 0.0; ///< The duration since elements were last updated.
		element *_focus = nullptr; ///< Pointer to the currently focused \ref element.
		size_t _active_update_tasks = 0; ///< The number of active update tasks.
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

		/// Forces all configurations to update right now.
		void _reset_update_estimate() {
			_next_update = std::chrono::high_resolution_clock::now();
		}

		// platform-dependent functions
		/// Handles one message from the system message queue. This function is platform-dependent.
		///
		/// \return Whether a message has been handled.
		bool _idle_system(wait_type);
		/// Sets a timer that will be activated after the given amount of time. When the timer is expired, this
		/// program will regain control and thus can update stuff. If this is called when a timer has previously
		/// been set, it may or may not be cancelled. This function is platform-dependent.
		void _set_timer(std::chrono::high_resolution_clock::duration);
	};
}
