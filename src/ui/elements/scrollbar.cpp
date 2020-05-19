// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "scrollbar.h"

/// \file
/// Implementation of the scrollbar.

#include "../manager.h"

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


	class_arrangements::notify_mapping scrollbar::_get_child_notify_mapping() {
		auto mapping = panel::_get_child_notify_mapping();
		mapping.emplace(get_drag_button_name(), _name_cast(_drag));
		mapping.emplace(get_page_up_button_name(), _name_cast(_pgup));
		mapping.emplace(get_page_down_button_name(), _name_cast(_pgdn));
		return mapping;
	}

	void scrollbar::_initialize(std::u8string_view cls, const element_configuration &config) {
		panel::_initialize(cls, config);

		_pgup->set_trigger_type(button::trigger_type::mouse_down);
		_pgup->click += [this]() {
			set_value(get_value() - get_visible_range());
		};

		_pgdn->set_trigger_type(button::trigger_type::mouse_down);
		_pgdn->click += [this]() {
			set_value(get_value() + get_visible_range());
		};
	}
}
