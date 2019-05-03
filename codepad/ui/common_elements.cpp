// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "common_elements.h"

/// \file
/// Implementation of certain methods of commonly used elements.

namespace codepad::ui {
	scrollbar &scrollbar_drag_button::_get_bar() const {
		return *dynamic_cast<scrollbar*>(logical_parent());
	}

	void scrollbar_drag_button::_on_mouse_down(mouse_button_info &p) {
		if (p.button == get_trigger_button()) {
			scrollbar &b = _get_bar();
			if (b.get_orientation() == orientation::vertical) {
				_doffset = p.position.get(*this).y;
			} else {
				_doffset = p.position.get(*this).x;
			}
		}
		button::_on_mouse_down(p);
	}

	void scrollbar_drag_button::_on_mouse_move(mouse_move_info &p) {
		if (is_trigger_button_pressed()) {
			scrollbar &b = _get_bar();
			b._on_drag_button_moved((
				b.get_orientation() == orientation::vertical ?
				p.new_position.get(b).y :
				p.new_position.get(b).x
				) - _doffset);
		}
		button::_on_mouse_move(p);
	}
}
