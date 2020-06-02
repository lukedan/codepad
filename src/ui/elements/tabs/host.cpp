// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/tabs/host.h"

/// \file
/// Implementation of certain functions related to tab hosts.

#include "codepad/ui/elements/tabs/manager.h"

namespace codepad::ui::tabs {
	class_arrangements::notify_mapping drag_destination_selector::_get_child_notify_mapping() {
		auto mapping = panel::_get_child_notify_mapping();
		mapping.emplace(get_split_left_indicator_name(), _name_cast(_split_left));
		mapping.emplace(get_split_right_indicator_name(), _name_cast(_split_right));
		mapping.emplace(get_split_up_indicator_name(), _name_cast(_split_up));
		mapping.emplace(get_split_down_indicator_name(), _name_cast(_split_down));
		mapping.emplace(get_combine_indicator_name(), _name_cast(_combine));
		return mapping;
	}

	void drag_destination_selector::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		set_zindex(zindex::overlay);

		_setup_indicator(*_split_left, drag_split_type::split_left);
		_setup_indicator(*_split_right, drag_split_type::split_right);
		_setup_indicator(*_split_up, drag_split_type::split_top);
		_setup_indicator(*_split_down, drag_split_type::split_bottom);
		_setup_indicator(*_combine, drag_split_type::combine);
	}

	void drag_destination_selector::_setup_indicator(element &elem, drag_split_type type) {
		elem.mouse_enter += [this, type]() {
			_dest = type;
		};
		elem.mouse_leave += [this]() {
			_dest = drag_split_type::new_window;
		};
	}


	void host::add_tab(tab &t) {
		_child_set_logical_parent(t, this);
		_child_set_logical_parent(*t._btn, this);
		_tab_contents_region->children().add(t);
		_tab_buttons_region->children().add(*t._btn);

		t.set_visibility(visibility::none);
		if (get_tab_count() == 1) {
			switch_tab(&t);
		}
	}

	void host::switch_tab(tab *t) {
		assert_true_logical(t == nullptr || t->logical_parent() == this, "the tab doesn't belong to this host");
		if (_active_tab) {
			_active_tab->set_visibility(visibility::none);
			_active_tab->_btn->set_zindex(0); // TODO a bit hacky
			_active_tab->_on_unselected();
		}
		_active_tab = t;
		if (_active_tab) {
			_active_tab->set_visibility(visibility::full);
			_active_tab->_btn->set_zindex(1);
			_active_tab->_on_selected();
		}
	}

	void host::_set_drag_dest_selector(drag_destination_selector *sel) {
		if (_dsel == sel) {
			return;
		}
		if (_dsel) {
			_children.remove(*_dsel);
		}
		_dsel = sel;
		if (_dsel) {
			_children.add(*_dsel);
		}
	}

	void host::_on_tab_removing(tab &t) {
		if (&t == _active_tab) { // change active tab
			if (_tab_contents_region->children().size() == 1) {
				switch_tab(nullptr);
			} else {
				auto it = _tab_contents_region->children().items().begin();
				for (; it != _tab_contents_region->children().items().end() && *it != &t; ++it) {
				}
				assert_true_logical(
					it != _tab_contents_region->children().items().end(),
					"removed tab in incorrect host"
				);
				auto original = it;
				if (++it == _tab_contents_region->children().items().end()) {
					it = --original;
				}
				switch_tab(dynamic_cast<tab*>(*it));
			}
		}
	}

	void host::_on_tab_removed(tab &t) {
		_tab_buttons_region->children().remove(*t._btn);
		get_tab_manager()._on_tab_detached(*this, t);
	}

	class_arrangements::notify_mapping host::_get_child_notify_mapping() {
		auto mapping = panel::_get_child_notify_mapping();
		mapping.emplace(get_tab_buttons_region_name(), _name_cast(_tab_buttons_region));
		mapping.emplace(get_tab_contents_region_name(), _name_cast(_tab_contents_region));
		return mapping;
	}

	void host::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_tab_contents_region->children().changing += [this](element_collection::change_info &p) {
			if (p.change_type == element_collection::change_info::type::remove) {
				tab *t = dynamic_cast<tab*>(&p.subject);
				assert_true_logical(t != nullptr, "corrupted element tree");
				_on_tab_removing(*t);
			}
		};
		_tab_contents_region->children().changed += [this](element_collection::change_info &p) {
			if (p.change_type == element_collection::change_info::type::remove) {
				tab *t = dynamic_cast<tab*>(&p.subject);
				assert_true_logical(t != nullptr, "corrupted element tree");
				_on_tab_removed(*t);
			}
		};
	}
}
