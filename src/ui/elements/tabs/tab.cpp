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


	void tab::deselect() {
		if (_selected) {
			assert_true_logical(
				get_host() == nullptr || get_host()->get_active_tab() != this, "cannot deselect active tab"
			);
			_selected = false;
			_on_deselected();
		}
	}

	void tab::_initialize() {
		panel::_initialize();

		_is_focus_scope = true;

		_btn = get_manager().create_element<tab_button>();
		_btn->click += [this](tab_button::click_info &info) {
			if (info.button_info.modifiers == modifier_keys::shift) {
				// select all tabs from the active tab to this one
				if (get_host()->get_active_tab() != this) {
					auto &tabs = get_host()->get_tabs().items();
					for (auto it = tabs.begin(); it != tabs.end(); ++it) {
						if (*it == this) {
							for (; *it != get_host()->get_active_tab(); ++it) {
								if (auto *t = checked_dynamic_cast<tab>(*it)) {
									t->select();
								}
							}
							break;
						} else if (*it == get_host()->get_active_tab()) {
							for (; *it != this; ++it) {
								if (auto *t = checked_dynamic_cast<tab>(*it)) {
									t->select();
								}
							}
						}
						// otherwise continue looking
					}
					get_host()->activate_tab_keep_selection_and_focus(*this);
				}
			} else if (info.button_info.modifiers == modifier_keys::control) {
				if (get_host()->get_active_tab() != this) {
					if (is_selected()) {
						deselect();
						get_manager().get_scheduler().set_focused_element(get_host());
					} else {
						get_host()->activate_tab_keep_selection_and_focus(*this);
					}
				}
			} else {
				if (get_host()->get_active_tab() != this) {
					get_host()->activate_tab_and_focus(*this);
				}
			}
			info.button_info.mark_focus_set();
		};
		_btn->request_close += [this]() {
			request_close();
		};
		_btn->start_drag += [this](tab_button::drag_start_info &p) {
			vec2d window_pos = p.reference + _btn->get_layout().xmin_ymin();
			vec2d reference_pos = window_pos - get_host()->get_tab_buttons_region().get_layout().xmin_ymin();
			get_tab_manager().start_dragging_selected_tabs(
				*get_host(), reference_pos, get_layout().translated(-window_pos)
			);
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
