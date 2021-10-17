// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/scrollbar.h"

/// \file
/// Implementation of the scrollbar.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	void scrollbar::set_params(double tot, double vis) {
		_total_range = std::max(tot, 0.0);
		_visible_range = std::clamp(vis, 0.0, _total_range);
		// _on_target_value_changed is NOT called here; it only starts smooth scrolling which we don't need here
		_target_value = _clamp_value(get_target_value());
		_smooth_begin_pos = _clamp_value(_smooth_begin_pos);
		// update smooth scrolling using the new parameters
		// if no smooth scrolling has ever been initiated, _smooth_begin will be epoch which still works corretly
		_update_smooth_scrolling(
			std::chrono::duration<double>(scheduler::clock_t::now() - _smooth_begin).count()
		);
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
		_drag_button_extended = btnlen < get_minimum_drag_button_length() && _visible_range < _total_range;
		if (_drag_button_extended) {
			btnlen = std::min(get_minimum_drag_button_length(), 0.5 * totsize);
			double percentage = get_actual_value() / (_total_range - _visible_range);
			mid1 = min + (totsize - btnlen) * percentage;
			mid2 = mid1 + btnlen;
		} else {
			double ratio = totsize / _total_range;
			mid1 = min + ratio * get_actual_value();
			mid2 = mid1 + ratio * _visible_range;
		}
		if (_total_range == 0.0) { // attempt to get rid of NaNs
			mid1 = min;
			mid2 = max;
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

	void scrollbar::_on_drag_button_moved(double newmin) {
		double range, draglen;
		rectd client = get_client_region();
		if (get_orientation() == orientation::vertical) {
			range = client.height();
			draglen = _drag->get_layout().height();
		} else {
			range = client.width();
			draglen = _drag->get_layout().width();
		}
		double
			diff = newmin,
			totsz = range;
		if (_drag_button_extended) {
			set_values_immediate((get_total_range() - get_visible_range()) * diff / (totsz - draglen));
		} else {
			set_values_immediate(get_total_range() * diff / totsz);
		}
	}

	property_info scrollbar::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"scrollbar")) {
			if (path.front().property == u8"orientation") {
				return property_info::make_getter_setter_property_info<scrollbar, orientation, element>(
					[](const scrollbar &bar) {
						return bar.get_orientation();
					},
					[](scrollbar &bar, orientation ori) {
						bar.set_orientation(ori);
					},
					u8"scrollbar.orientation"
				);
			}
			if (path.front().property == u8"smooth_scroll_duration") {
				return property_info::find_member_pointer_property_info<&scrollbar::_smooth_duration, element>(path);
			}
			if (path.front().property == u8"smoothing") {
				return property_info::find_member_pointer_property_info_managed<
					&scrollbar::_smoothing_transition, element
				>(path, get_manager());
			}
		}
		return panel::_find_property_path(path);
	}

	bool scrollbar::_handle_reference(std::u8string_view role, element *elem) {
		if (role == get_drag_button_name()) {
			if (_reference_cast_to(_drag, elem)) {
				_drag->mouse_down += [this](mouse_button_info &info) {
					if (info.button == _drag->get_trigger_button()) {
						if (get_orientation() == orientation::vertical) {
							_drag_button_mouse_offset =
								info.position.get(*this).y + get_layout().ymin - _drag->get_layout().ymin;
						} else {
							_drag_button_mouse_offset =
								info.position.get(*this).x + get_layout().xmin - _drag->get_layout().xmin;
						}
					}
				};
				_drag->mouse_move += [this](mouse_move_info &info) {
					if (_drag->is_trigger_button_pressed()) {
						_on_drag_button_moved((
							get_orientation() == orientation::vertical ?
							info.new_position.get(*this).y :
							info.new_position.get(*this).x
						) - _drag_button_mouse_offset);
					}
				};
			}
			return true;
		}
		if (role == get_page_up_button_name()) {
			if (_reference_cast_to(_pgup, elem)) {
				_pgup->set_trigger_type(button::trigger_type::mouse_down);
				_pgup->click += [this]() {
					set_target_value(get_target_value() - get_visible_range());
				};
			}
			return true;
		}
		if (role == get_page_down_button_name()) {
			if (_reference_cast_to(_pgdn, elem)) {
				_pgdn->set_trigger_type(button::trigger_type::mouse_down);
				_pgdn->click += [this]() {
					set_target_value(get_target_value() + get_visible_range());
				};
			}
			return true;
		}
		return panel::_handle_reference(role, elem);
	}

	void scrollbar::_update_smooth_scrolling(double time) {
		if (time >= _smooth_duration) {
			_update_actual_value(get_target_value());
		} else {
			// position: 0.0 for start, 1.0 for end
			double progress = time / _smooth_duration, position = 1.0;
			auto &smoothing = get_smoothing();
			if (smoothing) {
				position = smoothing(progress);
			}
			_update_actual_value(_smooth_begin_pos + (get_target_value() - _smooth_begin_pos) * position);
		}
	}

	void scrollbar::_initiate_smooth_scrolling() {
		if (get_smoothing()) {
			auto now = scheduler::clock_t::now();

			if (!_smooth_update_token.empty()) {
				// if there's already another smooth scrolling operation in progress, first update it and use the
				// updated values as the initial position of the new smooth scrolling operation
				// this greatly reduces the choppiness with consecutive scroll operations
				_update_smooth_scrolling(std::chrono::duration<double>(now - _smooth_begin).count());

				get_manager().get_scheduler().cancel_synchronous_task(_smooth_update_token);
			}

			_smooth_begin = now;
			_smooth_begin_pos = get_actual_value();
			_smooth_update_token = get_manager().get_scheduler().register_synchronous_task(
				scheduler::clock_t::now(), this, [this](element*) -> std::optional<scheduler::clock_t::time_point> {
					auto now = scheduler::clock_t::now();
					double time = std::chrono::duration<double>(now - _smooth_begin).count();
					bool ended = time >= _smooth_duration;
					// this is done before updating the value so that new tasks are cancelled correctly even if some
					// handler of actual_value_changed changes the target value again
					if (ended) {
						_smooth_update_token = scheduler::sync_task_token();
					}
					_update_smooth_scrolling(time);
					if (ended) {
						return std::nullopt;
					}
					return now;
				}
			);
		} else {
			_update_actual_value(get_target_value());
		}
	}
}
