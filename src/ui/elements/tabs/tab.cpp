// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/tabs/tab.h"

/// \file
/// Implementation of certain functions related to tabs.

#include "codepad/ui/elements/tabs/manager.h"

namespace codepad::ui::tabs {
	void tab_button::_on_mouse_down(mouse_button_info &p) {
		switch (p.button) {
		case mouse_button::primary:
			if (!_close_btn->is_mouse_over()) {
				_drag_pos = p.position.get(*this);
				_drag.start(p.position, *this);
				click.invoke_noret(p);
			}
			break;
		case mouse_button::tertiary:
			request_close.invoke();
			break;
		}
		panel::_on_mouse_down(p);
	}

	bool tab_button::_handle_reference(std::u8string_view role, element *elem) {
		if (role == get_label_name()) {
			_reference_cast_to(_label, elem);
			return true;
		}
		if (role == get_close_button_name()) {
			if (_reference_cast_to(_close_btn, elem)) {
				_close_btn->click += [this]() {
					request_close.invoke();
				};
			}
			return true;
		}
		return panel::_handle_reference(role, elem);
	}


	void tab::_initialize() {
		panel::_initialize();

		_is_focus_scope = true;

		_btn = get_manager().create_element<tab_button>();
		_btn->click += [this](tab_button::click_info &info) {
			get_host()->activate_tab(*this);
			info.button_info.mark_focus_set();
		};
		_btn->request_close += [this]() {
			request_close();
		};
		_btn->start_drag += [this](tab_button::drag_start_info &p) {
			// place the tab so that it appears as if this tab is the first tab in the given host
			// FIXME this doesn't work when the tab buttons are arranged right-to-left or bottom-to-top.
			vec2d windowpos = p.reference + get_host()->get_tab_buttons_region().get_layout().xmin_ymin();
			get_tab_manager().start_dragging_tab(*this, p.reference, get_layout().translated(-windowpos));
		};
	}

	void tab::_dispose() {
		get_manager().get_scheduler().mark_for_disposal(*_btn);
		panel::_dispose();
	}

	void tab::_on_close() {
		get_manager().get_scheduler().mark_for_disposal(*this);
	}
}
