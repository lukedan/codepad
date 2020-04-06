// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "panel.h"

/// \file
/// Implementation of certain methods of codepad::ui::element_collection.

#include "manager.h"
#include "scheduler.h"
#include "window.h"

using namespace codepad::os;

namespace codepad::ui {
	element_collection::~element_collection() {
		assert_true_logical(_children.empty(), "clear() not called in panel::_dispose()");
	}

	void element_collection::insert_before(element *before, element &target) {
		assert_true_usage(target._parent == nullptr, "the element is already a child of another panel");
		_f._on_child_adding(target, before);
		changing.invoke_noret(change_info::type::add, target, before);

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

		_f._on_child_added(target, before);
		changed.invoke_noret(change_info::type::add, target, before);
		target._on_added_to_parent();
	}

	void element_collection::set_zindex(element &elem, int newz) {
		_f._on_child_zindex_changing(elem);
		changing.invoke_noret(change_info::type::set_zindex, elem, nullptr);
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
		changed.invoke_noret(change_info::type::set_zindex, elem, nullptr);
	}

	void element_collection::move_before(element &elem, element *before) {
		_f._on_child_order_changing(elem, before);
		changing.invoke_noret(change_info::type::set_order, elem, before);
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
		_f._on_child_order_changed(elem, before);
		changed.invoke_noret(change_info::type::set_order, elem, before);
	}

	void element_collection::remove(element &elem) {
		assert_true_logical(elem._parent == &_f, "corrupted element tree");
		_f.get_manager().get_scheduler()._on_removing_element(elem);
		elem._on_removing_from_parent();
		_f._on_child_removing(elem);
		changing.invoke_noret(change_info::type::remove, elem, nullptr);
		elem._logical_parent = nullptr;
		elem._parent = nullptr;
		_children.erase(find(_children.begin(), _children.end(), &elem));
		_zorder.erase(find(_zorder.begin(), _zorder.end(), &elem));
		_f._on_child_removed(elem);
		changed.invoke_noret(change_info::type::remove, elem, nullptr);
	}

	void element_collection::clear() {
		while (!_children.empty()) {
			remove(**_children.begin());
		}
	}


	void panel::layout_on_direction(
		double &clientmin, double &clientmax,
		bool anchormin, bool pixelsize, bool anchormax,
		double marginmin, double size, double marginmax
	) {
		double totalspace = clientmax - clientmin, totalprop = 0.0;
		if (anchormax) {
			totalspace -= marginmax;
		} else {
			totalprop += marginmax;
		}
		if (pixelsize) {
			totalspace -= size;
		} else {
			totalprop += size;
		}
		if (anchormin) {
			totalspace -= marginmin;
		} else {
			totalprop += marginmin;
		}
		double propmult = totalspace / totalprop;
		// size in pixels are prioritized so that zero-size proportion parts are ignored when possible
		if (anchormin && anchormax) {
			if (pixelsize) {
				double midpos = 0.5 * (clientmin + clientmax);
				clientmin = midpos - 0.5 * size;
				clientmax = midpos + 0.5 * size;
			} else {
				clientmin += marginmin;
				clientmax -= marginmax;
			}
		} else if (anchormin) {
			clientmin += marginmin;
			clientmax = clientmin + (pixelsize ? size : size * propmult);
		} else if (anchormax) {
			clientmax -= marginmax;
			clientmin = clientmax - (pixelsize ? size : size * propmult);
		} else {
			clientmin += marginmin * propmult;
			clientmax -= marginmax * propmult;
		}
	}

	void panel::_invalidate_children_layout() {
		get_manager().get_scheduler().invalidate_children_layout(*this);
	}

	void panel::_on_mouse_down(mouse_button_info &p) {
		if (element *mouseover = _hit_test_for_child(p.position)) {
			mouseover->_on_mouse_down(p);
		}
		mouse_down.invoke(p);
		if (p.button == mouse_button::primary) {
			if (is_visible(visibility::focus) && !p.focus_set()) {
				p.mark_focus_set();
				get_manager().get_scheduler().set_focused_element(this);
			}
		}
	}

	element *panel::_hit_test_for_child(const mouse_position &p) const {
		for (element *elem : _children.z_ordered()) {
			if (elem->is_visible(visibility::interact)) {
				if (elem->hit_test(p.get(*elem))) {
					return elem;
				}
			}
		}
		return nullptr;
	}

	void panel::_dispose() {
		if (_dispose_children) {
			for (auto i : _children.items()) {
				get_manager().get_scheduler().mark_for_disposal(*i);
			}
		}
		_children.clear();
		element::_dispose();
	}
}
