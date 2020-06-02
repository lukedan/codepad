// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/button.h"

/// \file
/// Implementation of buttons.

#include "codepad/ui/window.h"

namespace codepad::ui {
	void button::_on_mouse_down(mouse_button_info &p) {
		_on_update_mouse_pos(p.position.get(*this));
		if (_hit_test_for_child(p.position) == nullptr) {
			if (p.button == _trigbtn) {
				_trigbtn_down = true;
				get_window()->set_mouse_capture(*this); // capture the mouse
				if (_trigtype == trigger_type::mouse_down) {
					_on_click();
				}
			}
		}
		panel::_on_mouse_down(p);
	}

	void button::_on_mouse_up(mouse_button_info &p) {
		_on_update_mouse_pos(p.position.get(*this));
		if (
			_trigbtn_down && p.button == _trigbtn &&
			_hit_test_for_child(p.position) == nullptr
			) {
			_trigbtn_down = false;
			get_window()->release_mouse_capture();
			if (is_mouse_over() && _trigtype == trigger_type::mouse_up) {
				_on_click();
			}
		}
		panel::_on_mouse_up(p);
	}

	void button::_on_update_mouse_pos(vec2d pos) {
		if (_allow_cancel) {
			bool over = hit_test(pos);
			if (over != is_mouse_over()) {
				if (over) {
					_on_mouse_enter();
				} else {
					_on_mouse_leave();
				}
			}
		}
	}
}
