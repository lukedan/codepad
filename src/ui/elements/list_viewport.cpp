// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/list_viewport.h"

/// \file
/// Implementation of virtualized listboxes.

namespace codepad::ui {
	vec2d virtual_list_viewport::get_virtual_panel_size() {
		double stacking_size = get_stacking_virtual_panel_size();
		panel_desired_size_accumulator independent_accum(
			std::numeric_limits<double>::infinity(),
			get_orientation() == orientation::horizontal ? orientation::vertical : orientation::horizontal
		);
		for (element *child : _children.items()) {
			independent_accum.accumulate(*child);
		}
		if (get_orientation() == orientation::horizontal) {
			return vec2d(stacking_size, independent_accum.maximum_size);
		} else {
			return vec2d(independent_accum.maximum_size, stacking_size);
		}
	}

	std::tuple<double, double, double> virtual_list_viewport::_get_item_size() const {
		size_allocation before, size, after;
		auto *first = _children.items().front();
		if (get_orientation() == orientation::horizontal) {
			before = first->get_margin_left();
			size = first->get_layout_width();
			after = first->get_margin_right();
		} else {
			before = first->get_margin_top();
			size = first->get_layout_height();
			after = first->get_margin_bottom();
		}
		return {
			before.is_pixels ? before.value : 0.0,
			size.is_pixels ? size.value : 0.0,
			after.is_pixels ? after.value : 0.0
		};
	}

	void virtual_list_viewport::_initialize_visible_elements(vec2d available_size, bool compute_desired_size) {
		if (!_source || _source->get_item_count() == 0) {
			return;
		}

		panel_desired_size_accumulator accum;
		if (get_orientation() == orientation::horizontal) {
			available_size.x = std::numeric_limits<double>::max();
			accum.available_size = available_size.y;
			accum.orient = orientation::vertical;
		} else {
			available_size.y = std::numeric_limits<double>::max();
			accum.available_size = available_size.x;
			accum.orient = orientation::horizontal;
		}
		// calls element::compute_desired_size using the appropriate available size
		auto do_compute_desired_size = [&](element &e) {
			double independent_size = accum.get_available(e);
			vec2d elem_available = available_size;
			if (get_orientation() == orientation::horizontal) {
				elem_available.y = independent_size;
			} else {
				elem_available.x = independent_size;
			}
			e.compute_desired_size(elem_available);
		};

		if (_children.items().empty()) {
			auto &first_item = _create_item(0);
			_children.add(first_item);
			do_compute_desired_size(first_item);
		}

		// compute visible item indices
		auto [before, size, after] = _get_item_size();
		double start = 0.0, end = 0.0;
		double item_size = before + size + after;
		if (get_orientation() == orientation::horizontal) {
			start = get_scroll_offset().x - get_padding().left;
			end = start + get_layout().width();
		} else {
			start = get_scroll_offset().y - get_padding().top;
			end = start + get_layout().height();
		}
		auto first_item = static_cast<std::size_t>(std::max(start / item_size, 1.0));
		auto past_last_item = std::min(
			static_cast<std::size_t>(std::max(end / item_size, 0.0)) + 1, _source->get_item_count()
		);
		std::size_t past_last_existing = _second_element_index + _children.items().size() - 1;

		// remove elements that are out of view
		while (_second_element_index < first_item && _children.items().size() > 1) { // before
			element &elem = **++_children.items().begin();
			_children.remove(elem);
			get_manager().get_scheduler().mark_for_disposal(elem);
			++_second_element_index;
		}
		while (past_last_existing > _second_element_index && past_last_existing > past_last_item) {
			element &elem = **--_children.items().end();
			_children.remove(elem);
			get_manager().get_scheduler().mark_for_disposal(elem);
			--past_last_existing;
		}
		if (_children.items().size() == 1) {
			// we've removed all elements other than the first one; we should reset the range of existing
			// elements to the visible range, or the next steps may create a lot of unnecessary elements
			_second_element_index = past_last_existing = first_item;
		}

		// create new elements
		element *insert_before = nullptr;
		if (_children.items().size() > 1) {
			insert_before = *++_children.items().begin();
		}
		while (_second_element_index > first_item) { // before
			--_second_element_index;
			auto &new_elem = _create_item(_second_element_index);
			_children.insert_before(insert_before, new_elem);
			if (compute_desired_size) {
				do_compute_desired_size(new_elem);
			}
			insert_before = &new_elem;
		}
		while (past_last_existing < past_last_item) { // after
			auto &new_elem = _create_item(past_last_existing);
			_children.insert_before(nullptr, new_elem);
			if (compute_desired_size) {
				do_compute_desired_size(new_elem);
			}
			++past_last_existing;
		}
	}

	vec2d virtual_list_viewport::_compute_desired_size_impl(vec2d available) {
		available -= get_padding().size();

		_initialize_visible_elements(available);

		panel_desired_size_accumulator accum;
		if (get_orientation() == orientation::horizontal) {
			available.x = std::numeric_limits<double>::max();
			accum.available_size = available.y;
			accum.orient = orientation::vertical;
		} else {
			available.y = std::numeric_limits<double>::max();
			accum.available_size = available.x;
			accum.orient = orientation::horizontal;
		}
			
		for (element *e : _children.items()) {
			double independent_size = accum.get_available(*e);
			vec2d elem_available = available;
			if (get_orientation() == orientation::horizontal) {
				elem_available.y = independent_size;
			} else {
				elem_available.x = independent_size;
			}
			e->compute_desired_size(elem_available);
			accum.accumulate(*e);
		}

		vec2d result;
		if (get_orientation() == orientation::horizontal) {
			result.x = get_stacking_virtual_panel_size();
			result.y = accum.maximum_size;
		} else {
			result.x = accum.maximum_size;
			result.y = get_stacking_virtual_panel_size();
		}
		return result + get_padding().size();
	}

	void virtual_list_viewport::_on_update_children_layout() {
		rectd client = get_client_region();
		// here we need to compute new desired size so that we have layout size data for the following layout
		// computation
		_initialize_visible_elements(client.size(), true);
		if (_children.items().empty()) {
			return;
		}

		auto [before, size, after] = _get_item_size();
		double span = before + size + after;
		// layout first element
		auto it = _children.items().begin();
		double virtual_panel_start = 0.0;
		if (get_orientation() == orientation::horizontal) {
			virtual_panel_start = get_layout().xmin + before - get_scroll_offset().x;
			_child_set_horizontal_layout(**it, virtual_panel_start, virtual_panel_start + size);
			panel::layout_child_vertical(**it, client.ymin, client.ymax);
		} else {
			virtual_panel_start = get_layout().ymin + before - get_scroll_offset().y;
			_child_set_vertical_layout(**it, virtual_panel_start, virtual_panel_start + size);
			panel::layout_child_horizontal(**it, client.xmin, client.xmax);
		}
		// layout other elements
		std::size_t index = _second_element_index;
		for (++it; it != _children.items().end(); ++it, ++index) {
			double min_pos = virtual_panel_start + span * index;
			if (get_orientation() == orientation::horizontal) {
				_child_set_horizontal_layout(**it, min_pos, min_pos + size);
				panel::layout_child_vertical(**it, client.ymin, client.ymax);
			} else {
				_child_set_vertical_layout(**it, min_pos, min_pos + size);
				panel::layout_child_horizontal(**it, client.xmin, client.xmax);
			}
		}
	}

	property_info virtual_list_viewport::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"virtual_list_viewport")) {
			if (path.front().property == u8"orientation") {
				return property_info::make_getter_setter_property_info<
					virtual_list_viewport, orientation, element
				>(
					[](const virtual_list_viewport &e) {
						return e.get_orientation();
					},
					[](virtual_list_viewport &e, orientation val) {
						e.set_orientation(val);
					},
					u8"virtual_list_viewport.orientation"
				);
			}
			if (path.front().property == u8"item_class") {
				return property_info::make_getter_setter_property_info<
					virtual_list_viewport, std::u8string, element
				>(
					[](const virtual_list_viewport &e) {
						return e.get_item_class();
					},
					[](virtual_list_viewport &e, std::u8string val) {
						e.set_item_class(val);
					},
					u8"virtual_list_viewport.item_class"
				);
			}
		}
		return scroll_viewport::_find_property_path(path);
	}
}
