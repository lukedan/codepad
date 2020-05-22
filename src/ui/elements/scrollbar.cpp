// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "scrollbar.h"

/// \file
/// Implementation of the scrollbar.

#include "../manager.h"

namespace codepad::ui {
	const property_mapping &scrollbar::get_properties() const {
		return get_properties_static();
	}

	const property_mapping &scrollbar::get_properties_static() {
		static property_mapping mapping;
		if (mapping.empty()) {
			mapping = panel::get_properties_static();
			mapping.emplace(u8"orientation", std::make_shared<getter_setter_property<scrollbar, orientation>>(
				u8"orientation",
				[](scrollbar &bar) {
					return bar.get_orientation();
				},
				[](scrollbar &bar, orientation ori) {
					bar.set_orientation(ori);
				}
			));
		}

		return mapping;
	}

	void scrollbar::_on_update_children_layout() {
		rectd cln = get_client_region();
		double min, max, mid1, mid2;
		if (get_orientation() == orientation::vertical) {
			min = cln.ymin;
			max = cln.ymax;
		} else {
			min = cln.xmin;
			max = cln.xmax;
		}
		double
			totsize = max - min,
			btnlen = totsize * _visible_range / _total_range;
		_drag_button_extended = btnlen < _drag->get_minimum_length();
		if (_drag_button_extended) {
			btnlen = _drag->get_minimum_length();
			double percentage = _value / (_total_range - _visible_range);
			mid1 = min + (totsize - btnlen) * percentage;
			mid2 = mid1 + btnlen;
		} else {
			double ratio = totsize / _total_range;
			mid1 = min + ratio * _value;
			mid2 = mid1 + ratio * _visible_range;
		}
		if (get_orientation() == orientation::vertical) {
			panel::layout_child_horizontal(*_drag, cln.xmin, cln.xmax);
			panel::layout_child_horizontal(*_pgup, cln.xmin, cln.xmax);
			panel::layout_child_horizontal(*_pgdn, cln.xmin, cln.xmax);
			_child_set_vertical_layout(*_drag, mid1, mid2);
			_child_set_vertical_layout(*_pgup, min, mid1);
			_child_set_vertical_layout(*_pgdn, mid2, max);
		} else {
			panel::layout_child_vertical(*_drag, cln.ymin, cln.ymax);
			panel::layout_child_vertical(*_pgup, cln.ymin, cln.ymax);
			panel::layout_child_vertical(*_pgdn, cln.ymin, cln.ymax);
			_child_set_horizontal_layout(*_drag, mid1, mid2);
			_child_set_horizontal_layout(*_pgup, min, mid1);
			_child_set_horizontal_layout(*_pgdn, mid2, max);
		}
	}

	scrollbar &scrollbar_drag_button::_get_bar() const {
		return *dynamic_cast<scrollbar*>(logical_parent());
	}

	void scrollbar_drag_button::_on_mouse_down(mouse_button_info &p) {
		if (p.button == get_trigger_button()) {
			scrollbar &b = _get_bar();
			if (b.get_orientation() == orientation::vertical) {
				_doffset = p.position.get(*this).y;
			} else {
				_doffset = p.position.get(*this).x;
			}
		}
		button::_on_mouse_down(p);
	}

	void scrollbar_drag_button::_on_mouse_move(mouse_move_info &p) {
		if (is_trigger_button_pressed()) {
			scrollbar &b = _get_bar();
			b._on_drag_button_moved((
				b.get_orientation() == orientation::vertical ?
				p.new_position.get(b).y :
				p.new_position.get(b).x
				) - _doffset);
		}
		button::_on_mouse_move(p);
	}


	class_arrangements::notify_mapping scrollbar::_get_child_notify_mapping() {
		auto mapping = panel::_get_child_notify_mapping();
		mapping.emplace(get_drag_button_name(), _name_cast(_drag));
		mapping.emplace(get_page_up_button_name(), _name_cast(_pgup));
		mapping.emplace(get_page_down_button_name(), _name_cast(_pgdn));
		return mapping;
	}

	void scrollbar::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_pgup->set_trigger_type(button::trigger_type::mouse_down);
		_pgup->click += [this]() {
			set_value(get_value() - get_visible_range());
		};

		_pgdn->set_trigger_type(button::trigger_type::mouse_down);
		_pgdn->click += [this]() {
			set_value(get_value() + get_visible_range());
		};
	}
}
