// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/tabs/tab.h"

/// \file
/// Implementation of certain functions related to tabs.

#include "codepad/ui/elements/tabs/manager.h"

namespace codepad::ui::tabs {
	void tab_button::_on_mouse_down(mouse_button_info &p) {
		if (_tab) {
			switch (p.button) {
			case mouse_button::primary:
				if (!_close_btn->is_mouse_over()) {
					_drag.start(p.position, *this);
					_handle_activate_on_mouse_up = _tab->is_selected();
					if (!_handle_activate_on_mouse_up) {
						_tab->_on_request_activate(
							(p.modifiers & modifier_keys::shift) != modifier_keys::none,
							(p.modifiers & modifier_keys::control) != modifier_keys::none,
							false
						);
						p.mark_focus_set();
					}
				}
				break;
			case mouse_button::tertiary:
				_tab->request_close();
				break;
			}
		} else {
			logger::get().log_warning() << "standalone tab button will not trigger tab events";
		}
		panel::_on_mouse_down(p);
	}

	void tab_button::_on_mouse_move(mouse_move_info &p) {
		if (_tab) {
			if (_drag.is_active()) {
				if (_drag.update(p.new_position, *this)) {
					if (_tab->get_host()->get_active_tab() != _tab) {
						// activate this tab before starting dragging
						_tab->_on_request_activate(false, true, true);
					}
					_tab->_on_start_drag(_drag.get_starting_position());
					start_drag.construct_info_and_invoke(_drag.get_starting_position());
				}
			}
		} else {
			logger::get().log_warning() << "standalone tab button will not trigger tab events";
		}
		panel::_on_mouse_move(p);
	}

	void tab_button::_on_mouse_up(mouse_button_info &p) {
		if (_tab) {
			if (_drag.is_active()) {
				_drag.on_cancel(*this);
				if (_handle_activate_on_mouse_up) {
					_tab->_on_request_activate(
						(p.modifiers & modifier_keys::shift) != modifier_keys::none,
						(p.modifiers & modifier_keys::control) != modifier_keys::none,
						false
					);
					p.mark_focus_set();
				}
			}
		} else {
			logger::get().log_warning() << "standalone tab button will not trigger tab events";
		}
		panel::_on_mouse_up(p);
	}

	bool tab_button::_handle_reference(std::u8string_view role, element *elem) {
		if (role == get_label_name()) {
			_reference_cast_to(_label, elem);
			return true;
		}
		if (role == get_close_button_name()) {
			if (_reference_cast_to(_close_btn, elem)) {
				_close_btn->click += [this]() {
					if (_tab) {
						_tab->request_close();
					} else {
						logger::get().log_warning() << "standalone tab button will not trigger tab events";
					}
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

	void tab::_on_request_activate(bool range_selection, bool keep_selection, bool before_drag) {
		// chrome-like multi-tab selection logic
		if (before_drag) {
			get_host()->activate_tab_keep_selection_and_focus(*this);
		} else if (range_selection) {
			// range selection: select all tabs from the active tab to this one
			auto &tabs = get_host()->get_tabs().items();
			for (auto it = tabs.begin(); it != tabs.end(); ++it) {
				if (*it == this) {
					for (; *it != get_host()->get_active_tab(); ++it) { // process the range
						if (auto *t = checked_dynamic_cast<tab>(*it)) {
							t->select();
						}
					}
				} else if (*it == get_host()->get_active_tab()) {
					for (; *it != this; ++it) { // process the range
						if (auto *t = checked_dynamic_cast<tab>(*it)) {
							t->select();
						}
					}
				} else {
					if (!keep_selection) {
						if (auto *t = checked_dynamic_cast<tab>(*it)) {
							t->deselect();
						}
					}
				}
			}
			get_host()->activate_tab_keep_selection_and_focus(*this);
		} else if (keep_selection) {
			if (is_selected()) { // deselect this tab
				if (get_host()->get_active_tab() != this) {
					deselect();
					get_manager().get_scheduler().set_focused_element(get_host());
				} else {
					// if there are multiple tabs selected, activate the first one and deselect this one;
					// otherwise do nothing
					for (auto *e : get_host()->get_tabs().items()) {
						if (e != this) {
							auto *t = checked_dynamic_cast<tab>(e);
							if (t && t->is_selected()) {
								get_host()->activate_tab_keep_selection_and_focus(*t);
								deselect();
								break;
							}
						}
					}
				}
			} else {
				get_host()->activate_tab_keep_selection_and_focus(*this);
			}
		} else {
			get_host()->activate_tab_and_focus(*this);
		}
	}

	void tab::_on_start_drag(vec2d mouse_offset) {
		vec2d window_pos = mouse_offset + _btn->get_layout().xmin_ymin();
		vec2d reference_pos = window_pos - get_host()->get_tab_buttons_region().get_layout().xmin_ymin();
		get_tab_manager().start_dragging_selected_tabs(
			*get_host(), reference_pos, get_layout().translated(-window_pos)
		);
	}

	void tab::_initialize() {
		panel::_initialize();

		_is_focus_scope = true;

		_btn = get_manager().create_element<tab_button>();
		_btn->_tab = this;
	}

	void tab::_dispose() {
		get_manager().get_scheduler().mark_for_disposal(*_btn);
		panel::_dispose();
	}

	void tab::_on_close() {
		get_manager().get_scheduler().mark_for_disposal(*this);
	}
}
