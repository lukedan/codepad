// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/scheduler.h"

/// \file
/// Implementation of the scheduler.

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/window.h"

namespace codepad::ui {
	bool scheduler::callback_token::cancel() {
		auto *sched = std::exchange(_scheduler, nullptr);
		// _callback_timestamp cannot be larger than _timestamp
		if (sched->_callback_timestamp == _timestamp) {
			std::scoped_lock<std::mutex> lock(sched->_callback_mutex);
			if (sched->_callback_timestamp == _timestamp) {
				sched->_callbacks.erase(_iter);
				--sched->_num_callbacks;
				return true;
			}
		}
		return false;
	}


	void scheduler::invalidate_layout(element &e) {
		// TODO maybe optimize for panels
		if (e.parent() != nullptr) {
			invalidate_children_layout(*e.parent());
		}
	}

	void scheduler::update_invalid_layout() {
		if (_layouting) { // prevent re-entrant
			return;
		}

		std::set<window*> desired_size_notify;
		std::swap(desired_size_notify, _desired_size_changed);
		// gather the list of elements with invalidated layout
		std::set<panel*> childrenupdate;
		std::swap(childrenupdate, _children_layout_scheduled);
		// list of elements to be notified
		std::deque<element*> notify(_layout_notify.begin(), _layout_notify.end());
		_layout_notify.clear();

		_update_layout(std::move(desired_size_notify), std::move(childrenupdate), std::move(notify));
	}

	void scheduler::update_element_layout_immediate(element &e) {
		panel *pnl = e.parent();
		if (!pnl) {
			pnl = dynamic_cast<panel*>(&e);
			if (!pnl) {
				// what are we doing here?
				return;
			}
		}

		std::set<panel*> update{ pnl };
		std::set<window*> measure;
		if (auto *wnd = pnl->get_window()) {
			measure.emplace(wnd);
		}
		_update_layout(std::move(measure), std::move(update), {});
	}

	void scheduler::update_invalid_visuals() {
		if (_dirty.empty()) {
			return;
		}
		performance_monitor mon(u8"render", render_time_redline);
		// gather the list of windows to render
		std::set<window*> ss;
		for (auto i : _dirty) {
			window *wnd = i->get_window();
			if (!wnd) {
				wnd = dynamic_cast<window*>(i);
			}
			if (wnd) {
				ss.insert(wnd);
			}
		}
		_dirty.clear();
		for (auto i : ss) {
			i->invalidate_window_visuals();
		}
	}

	void scheduler::update_synchronous_tasks() {
		clock_t::time_point execute_before = clock_t::now();
		while (!_sync_task_queue.empty() && execute_before >= _sync_task_queue.top().info->scheduled) {
			const auto &ref = _sync_task_queue.top();
			if (auto repeat = ref.info->task(ref.info->iter->first)) { // change scheduled time
				ref.info->scheduled = repeat.value();
				_sync_task_queue.on_key_decreased(0);
			} else { // erase the task
				_cancel_sync_task(&(*ref.info));
			}
		}
	}

	void scheduler::set_focused_element(element *elem) {
#ifdef CP_CHECK_LOGICAL_ERRORS
		struct _recursive_check {
			_recursive_check(bool &b) : _value(b) {
				assert_true_logical(!_value, "recursive calls to set_focused_element");
				_value = true;
			}
			~_recursive_check() {
				_value = false;
			}
		protected:
			bool &_value;
		};

		static bool _in = false;
		_recursive_check _check(_in);
#endif

		// first check if a parent window has system keyboard focus, otherwise don't update anything
		if (elem) {
			window *wnd = elem->get_window();
			bool should_update_focus = false;
			while (wnd) {
				if (wnd == _focused_window) {
					should_update_focus = true;
					break;
				}
				wnd = dynamic_cast<window*>(wnd->parent());
			}
			if (!should_update_focus) {
				return;
			}
		}

		element *newfocus = elem;
		// handle nested focus scopes: if a focus scope receives focus, the element inside that had focus should be
		// given focus
		while (true) {
			if (auto scope = dynamic_cast<panel*>(newfocus); scope && scope->is_focus_scope()) {
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
					gps.emplace_back(cur->_hotkeys, cur);
				}
				// update scope focus on root path
				element *scope_focus = newfocus;
				for (panel *scp = newfocus->parent(); scp; scp = scp->parent()) {
					if (scp->is_focus_scope()) {
						scp->_scope_focus = scope_focus;
						scope_focus = scp;
					}
				}
			}
			_hotkeys.reset_groups(gps);
			// change focus
			element *oldfocus = _focus;
			_focus = newfocus;
			// invoke events
			if (oldfocus) {
				oldfocus->_on_lost_focus();
			}
			if (newfocus) {
				newfocus->_on_got_focus();
			}
			logger::get().log_debug() <<
				"focus changed from " << oldfocus << " <" <<
				(oldfocus ? demangle(typeid(*oldfocus).name()) : u8"empty") << "> to " << _focus << " <" <<
				(_focus ? demangle(typeid(*_focus).name()) : u8"empty") << ">";
		}

#ifdef CP_CHECK_LOGICAL_ERRORS
		_in = false;
#endif
	}

	void scheduler::dispose_marked_elements() {
		performance_monitor mon(u8"dispose_elements");
		while (!_to_delete.empty()) { // as long as there are new batches to dispose of
			std::set<element*> batch;
			std::swap(batch, _to_delete);
			// dispose the current batch
			// new batches may be produced during this process
			for (element *elem : batch) {
				elem->destroying();
				elem->_dispose();
#ifdef CP_CHECK_USAGE_ERRORS
				assert_true_usage(!elem->_initialized, "element::_dispose() must be invoked by children classses");
#endif
				// remove the current entry from all lists
				auto *pnl = dynamic_cast<panel*>(elem);
				if (pnl) {
					_children_layout_scheduled.erase(pnl);
				}
				// remove all tasks related to the element
				if (auto it = _sync_tasks.find(elem); it != _sync_tasks.end()) {
					for (const auto &info : it->second) {
						_sync_task_queue.erase(info.queue_index);
					}
					_sync_tasks.erase(it);
				}
				// remove it from other lists
				if (auto *wnd = elem->_get_as_window()) {
					_desired_size_changed.erase(wnd);
				}
				_layout_notify.erase(elem);
				_dirty.erase(elem);
				_to_delete.erase(elem);
				// delete it
				delete elem;
			}
		}
	}

	void scheduler::main_iteration() {
		if (needs_update()) {
			// execute callbacks
			_check_and_execute_callbacks();
			// if updating is necessary, first perform this update, then process pending messages
			update();
			// limit the maximum number of messages processed at once
			for (std::size_t i = 0; i < maximum_messages_per_update; ++i) {
				if (!_impl->handle_event(wait_type::non_blocking)) {
					break;
				}
			}
		} else {
			_impl->set_timer(_earliest_sync_task() - clock_t::now()); // set up the timer
			_impl->handle_event(wait_type::blocking); // wait for the next event or the timer
		}
	}

	void scheduler::wake_up() {
		_impl->wake_up();
	}

	panel *scheduler::_find_focus_scope(element &e) {
		panel *scope = e.parent(); // innermost focus scope
		for (; scope && !scope->is_focus_scope(); scope = scope->parent()) {
		}
		return scope;
	}

	void scheduler::_on_removing_element(element &e) {
		panel *scope = _find_focus_scope(e);
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
				panel *newfocus = e.parent();
				while (
					newfocus && (newfocus->get_visibility() & visibility::focus) == visibility::none &&
					!newfocus->is_focus_scope()
					) {
					newfocus = newfocus->parent();
				}
				set_focused_element(newfocus); // may be nullptr, which is ok
				break;
			}
			// otherwise, only the focus in that scope (which is not in effect) has changed, and nothing needs
			// to be done
		}

		// release mouse capture if necessary
		if (window *wnd = e.get_window()) {
			for (element *c = wnd->get_mouse_capture(); c; c = c->parent()) {
				if (c == &e) { // yes, the captured element is a child of the removed element
					element *capture = wnd->get_mouse_capture();
					wnd->release_mouse_capture();
					// manually call _on_capture_lost() because it's not called elsewhere and this is not the
					// element willingly releasing the capture
					capture->_on_capture_lost();
					break;
				}
			}
		}
	}

	void scheduler::_on_system_focus_changed(window *wnd) {
		_focused_window = wnd;
		set_focused_element(wnd);
	}

	void scheduler::_update_layout(std::set<window*> measure, std::set<panel*> update, std::deque<element*> notify) {
		performance_monitor mon(u8"layout", relayout_time_redline);
		assert_true_logical(!_layouting, "update_invalid_layout() cannot be called recursively");
		_layouting = true;

		for (window *wnd : measure) {
			wnd->compute_desired_size(wnd->get_layout().size());
			// after we have the desired size, update the managed size of the window
			wnd->get_impl()._update_managed_window_size();
			update.emplace(wnd);
		}
		for (panel *pnl : update) {
			pnl->_on_update_children_layout();
			for (element *elem : pnl->_children.items()) {
				notify.emplace_back(elem);
			}
		}
		while (!notify.empty()) {
			element *li = notify.front();
			notify.pop_front();
			li->_on_layout_changed();
			// for panels, since their layout have been changed, their children will also be notified
			if (auto *pnl = dynamic_cast<panel*>(li)) {
				for (element *elem : pnl->_children.items()) {
					notify.emplace_back(elem);
				}
			}
		}
		_layouting = false;
	}

	bool scheduler::_check_and_execute_callbacks() {
		if (_num_callbacks > 0) {
			// take the list of callbacks out and unlock before executing, so that queuing callbacks in them
			// won't cause deadlocks
			std::list<std::function<void()>> callbacks;
			{
				std::scoped_lock<std::mutex> lock(_callback_mutex);
				if (_callbacks.empty()) {
					return false;
				}
				std::swap(callbacks, _callbacks);
				++_callback_timestamp;
				_num_callbacks = 0;
			}
			while (!callbacks.empty()) {
				callbacks.front()();
				callbacks.pop_front();
			}
			return true;
		}
		return false;
	}
}
