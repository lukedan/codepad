// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "tab.h"

/// \file
/// Implementation of certain functions related to tabs.

#include "manager.h"

namespace codepad::ui::tabs {
	host *tab::get_host() const {
		return dynamic_cast<host*>(logical_parent());
	}

	void tab::_initialize(str_view_t cls, const element_configuration &config) {
		panel::_initialize(cls, config);

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
			vec2d diff = p.drag_diff - vec2d(get_layout().xmin, _btn->get_layout().ymin);
			get_tab_manager().start_drag_tab(*this, p.drag_diff, get_layout().translated(-diff));
		};
	}

	void tab::_on_close_requested() {
		// also works without removing first, but this allows the window to check immediately if all tabs are
		// willing to close, and thus should always be performed with the next action.
		get_host()->remove_tab(*this);
		get_manager().get_scheduler().mark_for_disposal(*this);
	}
}
