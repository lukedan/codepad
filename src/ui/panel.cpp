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


	cursor panel::get_current_display_cursor() const {
		if (_children_cursor != cursor::not_specified) {
			return _children_cursor;
		}
		return element::get_current_display_cursor();
	}

	size_allocation panel::get_desired_width() const {
		double maxw = 0.0;
		for (const element *e : _children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = _get_horizontal_absolute_span(*e)) {
					maxw = std::max(maxw, span.value());
				}
			}
		}
		return size_allocation::pixels(maxw + get_padding().width());
	}

	size_allocation panel::get_desired_height() const {
		double maxh = 0.0;
		for (const element *e : _children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = _get_vertical_absolute_span(*e)) {
					maxh = std::max(maxh, span.value());
				}
			}
		}
		return size_allocation::pixels(maxh + get_padding().height());
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
				double midpos = 0.5 * (clientmin + clientmax + marginmin - marginmax);
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

	void panel::layout_child_horizontal(element &child, double xmin, double xmax) {
		anchor anc = child.get_anchor();
		thickness margin = child.get_margin();
		auto wprop = child.get_layout_width();
		child._layout.xmin = xmin;
		child._layout.xmax = xmax;
		layout_on_direction(
			child._layout.xmin, child._layout.xmax,
			(anc & anchor::left) != anchor::none, wprop.is_pixels, (anc & anchor::right) != anchor::none,
			margin.left, wprop.value, margin.right
		);
	}

	void panel::layout_child_vertical(element &child, double ymin, double ymax) {
		anchor anc = child.get_anchor();
		thickness margin = child.get_margin();
		auto hprop = child.get_layout_height();
		child._layout.ymin = ymin;
		child._layout.ymax = ymax;
		layout_on_direction(
			child._layout.ymin, child._layout.ymax,
			(anc & anchor::top) != anchor::none, hprop.is_pixels, (anc & anchor::bottom) != anchor::none,
			margin.top, hprop.value, margin.bottom
		);
	}

	void panel::_invalidate_children_layout() {
		get_manager().get_scheduler().invalidate_children_layout(*this);
	}

	void panel::_on_child_desired_size_changed(element &child, bool width, bool height) {
		bool
			width_changed = width && get_width_allocation() == size_allocation_type::automatic,
			height_changed = height && get_height_allocation() == size_allocation_type::automatic;
		if (width_changed || height_changed) { // actually affects something
			_on_desired_size_changed(width_changed, height_changed);
		} else {
			child.invalidate_layout();
		}
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
		return nullptr;
	}

	void panel::_initialize(std::u8string_view cls) {
		element::_initialize(cls);

		get_manager().get_class_arrangements().get_or_default(cls).construct_children(
			*this, _get_child_notify_mapping()
		);
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

	/// Implementation of \ref panel::_get_horizontal_absolute_span() and \ref panel::_get_vertical_absolute_span().
	template <
		anchor AnchorBefore, double thickness::*MarginBefore,
		size_allocation(element::*Size)() const,
		anchor AnchorAfter, double thickness::*MarginAfter
	> std::optional<double> _get_absolute_span(const element &e) {
		bool has_value = false;
		double cur = 0.0;
		thickness margin = e.get_margin();
		anchor anc = e.get_anchor();
		if ((anc & AnchorBefore) != anchor::none) {
			has_value = true;
			cur += margin.*MarginBefore;
		}
		auto size = (e.*Size)();
		if (size.is_pixels) {
			has_value = true;
			cur += size.value;
		}
		if ((anc & AnchorAfter) != anchor::none) {
			has_value = true;
			cur += margin.*MarginAfter;
		}
		if (has_value) {
			return cur;
		}
		return std::nullopt;
	}
	std::optional<double> panel::_get_horizontal_absolute_span(const element &e) {
		return _get_absolute_span<
			anchor::left, &thickness::left,
			&element::get_layout_width,
			anchor::right, &thickness::right
		>(e);
	}

	std::optional<double> panel::_get_vertical_absolute_span(const element &e) {
		return _get_absolute_span<
			anchor::top, &thickness::top,
			&element::get_layout_height,
			anchor::bottom, &thickness::bottom
		>(e);
	}

	/// Implementation of \ref panel::_get_max_horizontal_absolute_span() and
	/// \ref panel::_get_max_vertical_absolute_span()
	template <std::optional<double>(*GetSpan)(const element&)> std::optional<double> _get_max_absolute_span(
		const element_collection &children
	) {
		bool has_value = false;
		double val = 0.0;
		for (element *e : children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = GetSpan(*e)) {
					has_value = true;
					val = std::max(val, span.value());
				}
			}
		}
		if (has_value) {
			return val;
		}
		return std::nullopt;
	}
	std::optional<double> panel::_get_max_horizontal_absolute_span(const element_collection &children) {
		return _get_max_absolute_span<_get_horizontal_absolute_span>(children);
	}

	std::optional<double> panel::_get_max_vertical_absolute_span(const element_collection &children) {
		return _get_max_absolute_span<_get_vertical_absolute_span>(children);
	}

	/// Implementation of \ref panel::_get_total_horizontal_absolute_span() and
	/// \ref panel::_get_total_vertical_absolute_span()
	template <std::optional<double>(*GetSpan)(const element&)> std::optional<double> _get_total_absolute_span(
		const element_collection &children
	) {
		bool has_value = false;
		double val = 0.0;
		for (element *e : children.items()) {
			if (e->is_visible(visibility::layout)) {
				if (auto span = GetSpan(*e)) {
					has_value = true;
					val += span.value();
				}
			}
		}
		if (has_value) {
			return val;
		}
		return std::nullopt;
	}
	std::optional<double> panel::_get_total_horizontal_absolute_span(const element_collection &children) {
		return _get_total_absolute_span<_get_horizontal_absolute_span>(children);
	}

	std::optional<double> panel::_get_total_vertical_absolute_span(const element_collection &children) {
		return _get_total_absolute_span<_get_vertical_absolute_span>(children);
	}
}
