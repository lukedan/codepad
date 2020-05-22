// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "tab.h"

/// \file
/// Implementation of certain functions related to tabs.

#include "manager.h"

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

	class_arrangements::notify_mapping tab_button::_get_child_notify_mapping() {
		auto mapping = panel::_get_child_notify_mapping();
		mapping.emplace(get_label_name(), _name_cast(_label));
		mapping.emplace(get_close_button_name(), _name_cast(_close_btn));
		return mapping;
	}

	void tab_button::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_close_btn->click += [this]() {
			request_close.invoke();
		};
	}


	host *tab::get_host() const {
		return dynamic_cast<host*>(logical_parent());
	}

	void tab::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_is_focus_scope = true;

		_btn = get_manager().create_element<tab_button>();
		_btn->click += [this](tab_button::click_info &info) {
			get_host()->activate_tab(*this);
			info.button_info.mark_focus_set();
		};
		_btn->request_close += [this]() {
			_on_close_requested();
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

	void tab::_on_close_requested() {
		// also works without removing first, but this allows the window to check immediately if all tabs are
		// willing to close, and thus should always be performed with the next action.
		get_host()->remove_tab(*this);
		get_manager().get_scheduler().mark_for_disposal(*this);
	}
}
