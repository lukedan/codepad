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


	std::size_t get_line_ending_length(line_ending le) {
		switch (le) {
		case line_ending::none:
			return 0;
		case line_ending::n:
			[[fallthrough]];
		case line_ending::r:
			return 1;
		case line_ending::rn:
			return 2;
		}
		return 0;
	}

	std::u32string_view line_ending_to_string(line_ending le) {
		switch (le) {
		case line_ending::r:
			return U"\r";
		case line_ending::n:
			return U"\n";
		case line_ending::rn:
			return U"\r\n";
		default:
			return U"";
		}
	}
}
