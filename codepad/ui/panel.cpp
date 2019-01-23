// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "panel.h"

/// \file
/// Implementation of certain methods of codepad::ui::element_collection.

#include "manager.h"
#include "window.h"

using namespace codepad::os;

namespace codepad::ui {
	element_collection::~element_collection() {
		assert_true_logical(_children.empty(), "clear() not called in panel_base::_dispose()");
	}

	void element_collection::insert_before(element *before, element &target) {
		assert_true_usage(target._parent == nullptr, "the element is already a child of another panel");
		_f._on_child_adding(target);
		changing.invoke_noret(element_collection_change_info::type::add, target);

		target._parent = &_f;
		// find the first item whose z-index is less or equal
		auto zbefore = _zorder.rbegin();
		for (; zbefore != _zorder.rend(); ++zbefore) {
			if ((*zbefore)->get_zindex() >= target.get_zindex()) {
				break;
			}
		}
		auto posbefore = _children.begin();
		if (before == nullptr) {
			// find first element with different z-index
			for (; zbefore != _zorder.rend() && (*zbefore)->get_zindex() <= target.get_zindex(); ++zbefore) {
			}
			posbefore = _children.end();
		} else {
			// find `before'
			for (; posbefore != _children.end() && *posbefore != before; ++posbefore) {
				if (*posbefore == *zbefore) { // correct zbefore along the way
					++zbefore;
				}
			}
			assert_true_logical(posbefore != _children.end(), "`before' is not a member of this collection");
		}
		_zorder.insert(zbefore.base(), &target);
		_children.insert(posbefore, &target);

		_f._on_child_added(target);
		changed.invoke_noret(element_collection_change_info::type::add, target);
		target._on_added_to_parent();
	}

	void element_collection::set_zindex(element &elem, int newz) {
		_f._on_child_zindex_changing(elem);
		changing.invoke_noret(element_collection_change_info::type::set_zindex, elem);
		if (elem._zindex != newz) {
			// remove elem from _zorder
			for (auto it = _zorder.begin(); it != _zorder.end(); ++it) {
				if (*it == &elem) {
					_zorder.erase(it);
					break;
				}
			}
			// find elem in _children, and the first element before elem with z-index equal to newz
			element *insertbefore = nullptr;
			for (auto elemit = _children.begin(); elemit != _children.end() && *elemit != &elem; ++elemit) {
				if ((*elemit)->get_zindex() == elem.get_zindex()) {
					insertbefore = *elemit;
				}
			}
			// find place to insert in _zorder
			auto zbefore = _zorder.begin();
			if (insertbefore == nullptr) { // no object with the same z-index
				// find first element with smaller z-index
				for (; zbefore != _zorder.end() && (*zbefore)->get_zindex() >= newz; ++zbefore) {
				}
			} else {
				// find elemit in _zorder
				for (; zbefore != _zorder.end() && *zbefore != insertbefore; ++zbefore) {
				}
			}
			_zorder.insert(zbefore, &elem);
			elem._zindex = newz;
		}
		_f._on_child_zindex_changed(elem);
		changed.invoke_noret(element_collection_change_info::type::set_zindex, elem);
	}

	void element_collection::move_before(element &elem, element *before) {
		_f._on_child_order_changing(elem);
		changing.invoke_noret(element_collection_change_info::type::set_order, elem);
		// erase from both containers
		_children.erase(std::find(_children.begin(), _children.end(), &elem));
		_zorder.erase(std::find(_zorder.begin(), _zorder.end(), &elem));
		// find new position in _children, and previous element with the same z index
		auto newpos = _children.begin();
		element *zbefore = nullptr;
		if (before != nullptr) {
			for (; newpos != _children.end() && *newpos != before; ++newpos) {
				if ((*newpos)->get_zindex() == elem.get_zindex()) {
					zbefore = *newpos;
				}
			}
		} else {
			newpos = _children.end();
		}
		// insert in _children
		_children.emplace(newpos, &elem);
		// insert in _zorder
		auto zpos = _zorder.begin();
		for (; zpos != _zorder.end() && (*zpos)->get_zindex() > elem.get_zindex(); ++zpos) {
		}
		if (zbefore != nullptr) {
			for (++zpos; zpos != _zorder.end() && *zpos != zbefore; ++zpos) {
			}
		}
		_zorder.emplace(zpos, &elem);
		_f._on_child_order_changed(elem);
		changed.invoke_noret(element_collection_change_info::type::set_order, elem);
	}

	void element_collection::remove(element &elem) {
		assert_true_logical(elem._parent == &_f, "corrupted element tree");
		elem._on_removing_from_parent();
		_f._on_child_removing(elem);
		changing.invoke_noret(element_collection_change_info::type::remove, elem);
		window_base *wnd = _f.get_window();
		if (wnd != nullptr) {
			wnd->_on_removing_window_element(&elem);
		}
		elem._logical_parent = nullptr;
		elem._parent = nullptr;
		_children.erase(find(_children.begin(), _children.end(), &elem));
		_zorder.erase(find(_zorder.begin(), _zorder.end(), &elem));
		_f._on_child_removed(elem);
		changed.invoke_noret(element_collection_change_info::type::remove, elem);
	}

	void element_collection::clear() {
		while (!_children.empty()) {
			remove(**_children.begin());
		}
	}


	void panel_base::_invalidate_children_layout() {
		get_manager().invalidate_children_layout(*this);
	}

	void panel_base::_on_mouse_down(mouse_button_info &p) {
		element *mouseover = _hit_test_for_child(p.position);
		if (mouseover != nullptr) {
			mouseover->_on_mouse_down(p);
		}
		mouse_down.invoke(p);
		if (p.button == mouse_button::primary) {
			if (_can_focus && !p.focus_set()) {
				p.mark_focus_set();
				get_window()->set_window_focused_element(*this);
			}
			if (mouseover == nullptr) {
				set_state_bits(get_manager().get_predefined_states().mouse_down, true);
			}
		}
	}

	element *panel_base::_hit_test_for_child(vec2d p) {
		for (element *elem : _children.z_ordered()) {
			if (elem->is_hittest_visible() && elem->hit_test(p)) {
				return elem;
			}
		}
		return nullptr;
	}

	void panel_base::_dispose() {
		if (_dispose_children) {
			for (auto i : _children.items()) {
				get_manager().mark_disposal(*i);
			}
		}
		_children.clear();
		element::_dispose();
	}


	void stack_panel::_on_state_changed(value_update_info<element_state_id> &p) {
		panel::_on_state_changed(p);
		if (_has_any_state_bit_changed(get_manager().get_predefined_states().vertical, p)) {
			_on_desired_size_changed(true, true);
			_invalidate_children_layout();
		}
	}
}
