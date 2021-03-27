// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/window.h"

/// \file
/// Implementation of certain functions of \ref codepad::ui::window.

#include <vector>

#include "codepad/ui/manager.h"

namespace codepad::ui {
	size_allocation window::get_desired_width() const {
		double maxw = 0.0;
		for (const element *e : _children.items()) {
			if (e->is_visible(visibility::layout) && !e->_get_as_window()) {
				if (auto span = _get_horizontal_absolute_span(*e)) {
					maxw = std::max(maxw, span.value());
				}
			}
		}
		return size_allocation::pixels(maxw + get_padding().width());
	}

	size_allocation window::get_desired_height() const {
		double maxh = 0.0;
		for (const element *e : _children.items()) {
			if (e->is_visible(visibility::layout) && !e->_get_as_window()) {
				if (auto span = _get_vertical_absolute_span(*e)) {
					maxh = std::max(maxh, span.value());
				}
			}
		}
		return size_allocation::pixels(maxh + get_padding().height());
	}

	void window::set_mouse_capture(element &elem) {
		logger::get().log_debug(CP_HERE) <<
			"set mouse capture 0x" << &elem << " <" << demangle(typeid(elem).name()) << ">";
		assert_true_usage(_capture == nullptr, "mouse already captured");
		_capture = &elem;
		// send appropriate mouse leave / enter messages
		// TODO this may cause stack overflow
		if (!_capture->is_mouse_over()) {
			std::vector<element*> enters; // the list of elements where mouse_enter should be sent to
			for (panel *e = elem.parent(); e; e = e->parent()) { // send mouse_leave messages
				if (e->is_mouse_over()) {
					for (element *cur : e->children().z_ordered()) {
						if (cur->is_mouse_over()) {
							// if the mouse is over an element then
							// it must not contain _capture
							cur->_on_mouse_leave();
							// there should only be one element that the mouse is over,
							// but we'll iterate over all of them just because
						}
					}
					break;
					// TODO if the mouse is not in this window then mouse_leave messages will *not* be sent
					//      not sure if anything should be done, or if the system set_capture function will
					//      handle it
				}
				enters.emplace_back(e);
			}
			// send mouse_enter messages
			for (auto it = enters.rbegin(); it != enters.rend(); ++it) {
				(*it)->_on_mouse_enter();
			}
			_capture->_on_mouse_enter();
		}
		// finally, notify the system
		_impl->_set_mouse_capture();
	}

	[[nodiscard]] cursor window::get_current_display_cursor() const {
		if (_capture) {
			return _capture->get_current_display_cursor();
		}
		return panel::get_current_display_cursor();
	}

	mouse_position window::_update_mouse_position(vec2d pos) {
		mouse_position::_active_window = this;
		++mouse_position::_global_timestamp;
		_cached_mouse_position = get_visual_parameters().transform.inverse_transform_point(
			pos - get_layout().xmin_ymin(), get_layout().size()
		);
		_cached_mouse_position_timestamp = mouse_position::_global_timestamp;
		return mouse_position();
	}

	void window::_on_prerender() {
		get_manager().get_renderer().begin_drawing(*this);
		get_manager().get_renderer().clear(colord(0.0, 0.0, 0.0, 0.0));
		panel::_on_prerender();
	}

	void window::_on_postrender() {
		panel::_on_postrender();
		get_manager().get_renderer().end_drawing();
	}

	void window::_initialize() {
		_impl = _create_impl(*this);
		_is_focus_scope = true;

		panel::_initialize();

		get_manager().get_renderer()._new_window(*this);
	}

	void window::_dispose() {
		// here we call _on_removing_element to ensure that the focus has been properly updated
		get_manager().get_scheduler()._on_removing_element(*this);
		panel::_dispose();

		// finalize renderer data after everything else so that events sent out during
		// base panel finalization can be handled
		get_manager().get_renderer()._delete_window(*this);
	}

	void window::_on_size_changed(size_changed_info &p) {
		get_manager().get_scheduler().notify_layout_change(*this);
		size_changed(p);
	}

	void window::_on_window_layout_changed() {
		window_layout_changed();
	}

	void window::_on_scaling_factor_changed(scaling_factor_changed_info &p) {
		invalidate_visual();
		scaling_factor_changed(p);
	}

	void window::_on_key_down(key_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_key_down(p);
		} else {
			panel::_on_key_down(p);
		}
	}

	void window::_on_key_up(key_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_key_up(p);
		} else {
			panel::_on_key_up(p);
		}
	}

	void window::_on_keyboard_text(text_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_keyboard_text(p);
		} else {
			panel::_on_keyboard_text(p);
		}
	}

	void window::_on_composition(composition_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_composition(p);
		} else {
			panel::_on_composition(p);
		}
	}

	void window::_on_composition_finished() {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_composition_finished();
		} else {
			panel::_on_composition_finished();
		}
	}

	void window::_on_lost_window_capture() {
		if (_capture != nullptr) {
			_capture->_on_capture_lost();
			_capture = nullptr;
		}
	}

	void window::_on_mouse_enter() {
		if (_capture != nullptr) { // TODO technically this won't happen as the window has already captured the mouse
			_capture->_on_mouse_enter();
			element::_on_mouse_enter();
		} else {
			panel::_on_mouse_enter();
		}
	}

	void window::_on_mouse_leave() {
		if (_capture != nullptr) { // TODO technically this won't happen
			_capture->_on_mouse_leave();
			element::_on_mouse_leave();
		} else {
			panel::_on_mouse_leave();
		}
	}

	void window::_on_mouse_move(mouse_move_info &p) {
		if (_capture != nullptr) {
			_capture->_on_mouse_move(p);
			element::_on_mouse_move(p);
		} else {
			panel::_on_mouse_move(p);
		}
	}
	
	void window::_on_mouse_down(mouse_button_info &p) {
		if (_capture != nullptr) {
			_capture->_on_mouse_down(p);
			mouse_down(p);
		} else {
			panel::_on_mouse_down(p);
		}
	}

	void window::_on_mouse_up(mouse_button_info &p) {
		if (_capture != nullptr) {
			_capture->_on_mouse_up(p);
			element::_on_mouse_up(p);
		} else {
			panel::_on_mouse_up(p);
		}
	}

	void window::_on_mouse_scroll(mouse_scroll_info &p) {
		if (_capture != nullptr) {
			for (element *e = _capture; e != this; e = e->parent()) {
				assert_true_logical(e, "corrupted element tree");
				e->_on_mouse_scroll(p);
			}
			element::_on_mouse_scroll(p);
		} else {
			panel::_on_mouse_scroll(p);
		}
	}

	element *window::_hit_test_for_child(const mouse_position &pos) const {
		for (element *elem : _children.z_ordered()) {
			if (!elem->_get_as_window() && elem->is_visible(visibility::interact)) {
				if (elem->hit_test(pos.get(*elem))) {
					return elem;
				}
			}
		}
		return nullptr;
	}

	void window::_on_update_children_layout() {
		rectd client = get_client_region();
		for (auto *elem : _children.items()) {
			if (!elem->_get_as_window()) {
				layout_child(*elem, client);
			}
		}
	}

	property_info window::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"window")) {
			if (path.front().property == u8"width_size_policy") {
				return property_info::make_getter_setter_property_info<window, size_policy, element>(
					[](const window &wnd) {
						return wnd.get_width_size_policy();
					},
					[](window &wnd, size_policy p) {
						wnd.set_width_size_policy(p);
					},
					u8"width_size_policy"
				);
			}
			if (path.front().property == u8"height_size_policy") {
				return property_info::make_getter_setter_property_info<window, size_policy, element>(
					[](const window &wnd) {
						return wnd.get_height_size_policy();
					},
					[](window &wnd, size_policy p) {
						wnd.set_height_size_policy(p);
					},
					u8"height_size_policy"
				);
			}
		}
		return panel::_find_property_path(path);
	}

	void window::_on_got_system_focus() {
		get_manager().get_scheduler()._on_system_focus_changed(this);
	}

	void window::_on_lost_system_focus() {
		get_manager().get_scheduler()._on_system_focus_changed(nullptr);
	}
}
