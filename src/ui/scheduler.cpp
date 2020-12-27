// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/scheduler.h"

/// \file
/// Implementation of the scheduler.

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/window.h"

namespace codepad::ui {
	void scheduler::invalidate_layout(element &e) {
		// TODO maybe optimize for panels
		if (e.parent() != nullptr) {
			invalidate_children_layout(*e.parent());
		}
	}

	void scheduler::update_invalid_layout() {
		if (_children_layout_scheduled.empty() && _layout_notify.empty()) {
			return;
		}

		// list of elements to be notified
		std::deque<element*> notify(_layout_notify.begin(), _layout_notify.end());
		_layout_notify.clear();
		// gather the list of elements with invalidated layout
		std::set<panel*> childrenupdate;
		std::swap(childrenupdate, _children_layout_scheduled);

		_update_layout(childrenupdate, std::move(notify));
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
		_update_layout(update, std::deque<element*>());
	}

	void scheduler::update_invalid_visuals() {
		if (_dirty.empty()) {
			return;
		}
		performance_monitor mon(u8"render", render_time_redline);
		// gather the list of windows to render
		std::set<window_base*> ss;
		for (auto i : _dirty) {
			window_base *wnd = i->get_window();
			if (!wnd) {
				wnd = dynamic_cast<window_base*>(i);
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

	void scheduler::update_tasks() {
		clock_t::time_point execute_before = clock_t::now();
		while (!_task_queue.empty() && execute_before >= _task_queue.top().info->scheduled) {
			const auto &ref = _task_queue.top();
			if (auto repeat = ref.info->task(ref.info->iter->first)) { // change scheduled time
				ref.info->scheduled = repeat.value();
				_task_queue.on_key_decreased(0);
			} else { // erase the task
				_cancel_task(&(*ref.info));
			}
		}
	}

	void scheduler::set_focused_element(element *elem) {
#ifdef CP_CHECK_LOGICAL_ERRORS
		static bool _in = false;

		assert_true_logical(!_in, "recursive calls to set_focused_element");
		_in = true;
#endif

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
			logger::get().log_debug(CP_HERE) <<
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
				if (auto it = _tasks.find(elem); it != _tasks.end()) {
					for (const auto &info : it->second) {
						_task_queue.erase(info.queue_index);
					}
					_tasks.erase(it);
				}
				// remove it from other lists
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
			// if updating is necessary, first perform this update, then process pending messages
			update();
			// limit the maximum number of messages processed at once
			for (
				std::size_t i = 0;
				i < maximum_messages_per_update && _main_iteration_system(wait_type::non_blocking);
				++i
			) {
			}
		} else {
			_set_timer(_next_update() - clock_t::now()); // set up the timer
			_main_iteration_system(wait_type::blocking); // wait for the next event or the timer
		}
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
		if (window_base *wnd = e.get_window()) {
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

	void scheduler::_update_layout(const std::set<panel*> &update, std::deque<element*> notify) {
		performance_monitor mon(u8"layout", relayout_time_redline);
		assert_true_logical(!_layouting, "update_invalid_layout() cannot be called recursively");
		_layouting = true;

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
}
