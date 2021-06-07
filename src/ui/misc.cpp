// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/misc.h"

/// \file
/// Implementation of miscellaneous functionalities.

#include "codepad/os/misc.h"
#include "codepad/ui/manager.h"

namespace codepad::ui {
	drag_deadzone::drag_deadzone() : drag_deadzone(os::system_parameters::get_drag_deadzone_radius()) {
	}

	void drag_deadzone::start(const mouse_position &mouse, element &parent) {
		if (window *wnd = parent.get_window()) { // start only if the element's in a window
			wnd->set_mouse_capture(parent);
			_start = mouse.get(*wnd);
			_deadzone = true;
		}
	}

	bool drag_deadzone::update(const mouse_position &mouse, element &parent) {
		if (window *wnd = parent.get_window()) {
			double sqrdiff = (mouse.get(*wnd) - _start).length_sqr();
			if (sqrdiff > radius * radius) { // start dragging
				wnd->release_mouse_capture();
				_deadzone = false;
				return true;
			}
		}
		return false;
	}

	void drag_deadzone::on_cancel(element &parent) {
		assert_true_logical(_deadzone, "please caret check is_active() before calling on_cancel()");
		if (window *wnd = parent.get_window()) {
			if (wnd->get_mouse_capture()) {
				wnd->release_mouse_capture();
			}
		}
		_deadzone = false;
	}


	void caret_selection::move_caret(std::size_t new_caret) {
		if (caret_offset == 0 || caret_offset == selection_length) { // test rule 
			std::size_t
				new_beg = new_caret,
				new_end = caret_offset == 0 ? get_selection_end() : selection_begin;
			if (new_beg > new_end) {
				std::swap(new_beg, new_end);
			}
			selection_begin = new_beg;
			selection_length = new_end - new_beg;
			caret_offset = new_caret - selection_begin;
		} else if (new_caret < selection_begin) { // test half of rule 2
			selection_length = get_selection_end() - new_caret;
			selection_begin = new_caret;
			caret_offset = 0;
		} else if (new_caret > get_selection_end()) { // test other half of rule 2
			selection_length = new_caret - selection_begin;
			caret_offset = selection_length;
		} else {
			caret_offset = new_caret - selection_begin;
		}
	}
}
