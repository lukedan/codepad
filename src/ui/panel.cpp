// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/panel.h"

/// \file
/// Implementation of certain methods of codepad::ui::element_collection.

#include "codepad/ui/manager.h"
#include "codepad/ui/scheduler.h"
#include "codepad/ui/window.h"

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


	cursor panel::get_current_display_cursor() const {
		if (_children_cursor != cursor::not_specified) {
			return _children_cursor;
		}
		return element::get_current_display_cursor();
	}

	void panel::layout_on_direction(
		double &clientmin, double &clientmax,
		size_allocation margin_min, size_allocation size, size_allocation margin_max
	) {
		double totalpixels = 0.0, totalprop = 0.0;
		margin_min.accumulate_to(totalpixels, totalprop);
		size.accumulate_to(totalpixels, totalprop);
		margin_max.accumulate_to(totalpixels, totalprop);
		double propmult = ((clientmax - clientmin) - totalpixels) / totalprop;
		// size in pixels are prioritized so that zero-size proportion parts are ignored when possible
		if (margin_min.is_pixels && margin_max.is_pixels) {
			if (size.is_pixels) {
				double midpos = 0.5 * (clientmin + clientmax + margin_min.value - margin_max.value);
				clientmin = midpos - 0.5 * size.value;
				clientmax = midpos + 0.5 * size.value;
			} else {
				clientmin += margin_min.value;
				clientmax -= margin_max.value;
			}
		} else if (margin_min.is_pixels) {
			clientmin += margin_min.value;
			clientmax = clientmin + (size.is_pixels ? size.value : size.value * propmult);
		} else if (margin_max.is_pixels) {
			clientmax -= margin_max.value;
			clientmin = clientmax - (size.is_pixels ? size.value : size.value * propmult);
		} else {
			clientmin += margin_min.value * propmult;
			clientmax -= margin_max.value * propmult;
		}
	}

	void panel::layout_child_horizontal(element &child, double xmin, double xmax) {
		child._layout.xmin = xmin;
		child._layout.xmax = xmax;
		layout_on_direction(
			child._layout.xmin, child._layout.xmax,
			child.get_margin_left(), child.get_layout_width(), child.get_margin_right()
		);
	}

	void panel::layout_child_vertical(element &child, double ymin, double ymax) {
		child._layout.ymin = ymin;
		child._layout.ymax = ymax;
		layout_on_direction(
			child._layout.ymin, child._layout.ymax,
			child.get_margin_top(), child.get_layout_height(), child.get_margin_bottom()
		);
	}

	double panel::get_available_size_for_child(
		double available, size_allocation anchor_min, size_allocation anchor_max,
		size_allocation_type size_type, double size
	) {
		double ext_pixels = 0.0, ext_proportion = 0.0;
		anchor_min.accumulate_to(ext_pixels, ext_proportion);
		anchor_max.accumulate_to(ext_pixels, ext_proportion);
		available -= ext_pixels;
		switch (size_type) {
		case size_allocation_type::automatic:
			return available;
		case size_allocation_type::fixed:
			return size;
		case size_allocation_type::proportion:
			return available * size / (size + ext_proportion);
		}
		return 0.0; // should not happen
	}

	void panel::_invalidate_children_layout() {
		get_manager().get_scheduler().invalidate_children_layout(*this);
	}

	vec2d panel::_compute_desired_size_impl(vec2d available) {
		available -= get_padding().size();
		panel_desired_size_accumulator hori_accum(available.x, orientation::horizontal);
		panel_desired_size_accumulator vert_accum(available.y, orientation::vertical);
		for (element *child : _children.items()) {
			if (child->is_visible(visibility::layout)) {
				vec2d child_available(hori_accum.get_available(*child), vert_accum.get_available(*child));
				child->compute_desired_size(child_available);
				hori_accum.accumulate(*child);
				vert_accum.accumulate(*child);
			}
		}
		return vec2d(hori_accum.maximum_size, vert_accum.maximum_size) + get_padding().size();
	}

	void panel::_on_child_desired_size_changed(element&) {
		_on_desired_size_changed();
	}

	void panel::_on_mouse_down(mouse_button_info &p) {
		if (element *mouseover = _hit_test_for_child(p.position)) {
			mouseover->_on_mouse_down(p);
		}
		element::_on_mouse_down(p);
	}

	void panel::_on_mouse_move(mouse_move_info &p) {
		_children_cursor = cursor::not_specified; // reset cursor
		element *mouseover = _hit_test_for_child(p.new_position);
		for (element *j : _children.z_ordered()) { // the mouse cannot be over any other element
			if (mouseover != j && j->is_mouse_over()) {
				j->_on_mouse_leave();
			}
		}
		if (mouseover) {
			if (!mouseover->is_mouse_over()) { // just moved onto the element
				mouseover->_on_mouse_enter();
			}
			mouseover->_on_mouse_move(p);
			_children_cursor = mouseover->get_current_display_cursor(); // get cursor
		}
		element::_on_mouse_move(p);
	}

	void panel::_on_mouse_hover(mouse_hover_info &p) {
		if (element *mouseover = _hit_test_for_child(p.position)) {
			mouseover->_on_mouse_hover(p);
		}
		element::_on_mouse_hover(p);
	}

	element *panel::_hit_test_for_child(const mouse_position &p) const {
		for (element *elem : _children.z_ordered()) {
			if (elem->is_visible(visibility::interact)) {
				if (elem->hit_test(p.get(*elem))) {
					return elem;
				}
			}
		}
		return _hit_test_fallback;
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
