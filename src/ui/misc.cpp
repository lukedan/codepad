// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "misc.h"

/// \file
/// Implementation of miscellaneous functionalities.

#include "manager.h"

namespace codepad::ui {
	drag_deadzone::drag_deadzone() : drag_deadzone(manager::get_drag_deadzone_radius()) {
	}

	void drag_deadzone::start(const mouse_position &mouse, element &parent) {
		if (window_base *wnd = parent.get_window()) { // start only if the element's in a window
			wnd->set_mouse_capture(parent);
			_start = mouse.get(*wnd);
			_deadzone = true;
		}
	}

	bool drag_deadzone::update(const mouse_position &mouse, element &parent) {
		if (window_base *wnd = parent.get_window()) {
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
		if (window_base *wnd = parent.get_window()) {
			if (wnd->get_mouse_capture()) {
				wnd->release_mouse_capture();
			}
		}
		_deadzone = false;
	}
}
