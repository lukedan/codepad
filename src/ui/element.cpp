// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/element.h"

/// \file
/// Implementation of certain methods related to ui::element.

#include "codepad/os/misc.h"
#include "codepad/ui/window.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

namespace codepad::ui {
	vec2d mouse_position::get(element &e) const {
		if (e._cached_mouse_position_timestamp != _global_timestamp) {
			vec2d fresh_pos;
			if (e.parent() == nullptr) {
				window *wnd = dynamic_cast<window*>(&e);
				assert_true_usage(wnd, "cannot get mouse position for isolated element");
				assert_true_logical(
					_active_window && _active_window != wnd,
					"there must be one window with fresh mouse position"
				);

				fresh_pos = get(*_active_window);
				// transform from "panel" window to window itself
				fresh_pos = _active_window->get_visual_parameters().transform.transform_point(
					fresh_pos, _active_window->get_layout().size()
				);
				// transform to the current window
				fresh_pos = wnd->screen_to_client(_active_window->client_to_screen(fresh_pos));
				// transform from window itself to "panel" window
				fresh_pos = wnd->get_visual_parameters().transform.inverse_transform_point(
					fresh_pos - wnd->get_layout().xmin_ymin(), wnd->get_layout().size()
				);
			} else { // TODO non-recursive version?
				fresh_pos = e.get_visual_parameters().transform.inverse_transform_point(
					get(*e.parent()) - (e.get_layout().xmin_ymin() - e.parent()->get_layout().xmin_ymin()),
					e.get_layout().size()
				);
			}
			e._cached_mouse_position = fresh_pos;
			e._cached_mouse_position_timestamp = _global_timestamp;
		}
		return e._cached_mouse_position;
	}


	size_allocation element::get_layout_width() const {
		size_allocation_type type = get_width_allocation();
		if (type == size_allocation_type::automatic) {
			return size_allocation::pixels(get_desired_size().x);
		}
		return size_allocation(_layout_params.size.x, type == size_allocation_type::fixed);
	}

	size_allocation element::get_layout_height() const {
		size_allocation_type type = get_height_allocation();
		if (type == size_allocation_type::automatic) {
			return size_allocation::pixels(get_desired_size().y);
		}
		return size_allocation(_layout_params.size.y, type == size_allocation_type::fixed);
	}

	void element::set_zindex(int v) {
		if (_parent) {
			_parent->_children.set_zindex(*this, v);
		} else {
			_zindex = v;
		}
	}

	window *element::get_window() {
		for (element *cur = this; cur; cur = cur->parent()) {
			if (auto *wnd = cur->_get_as_window()) {
				return wnd;
			}
		}
		return nullptr;
	}

	void element::invalidate_visual() {
		get_manager().get_scheduler().invalidate_visual(*this);
	}

	void element::invalidate_layout() {
		get_manager().get_scheduler().invalidate_layout(*this);
	}

	property_info element::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"element")) {
			if (path.front().property == u8"layout") {
				return property_info::find_member_pointer_property_info<&element::_layout_params, element>(
					path, property_info::make_typed_modification_callback<element>([](element &e) {
						e._on_layout_parameters_changed();
					})
				);
			}
			if (path.front().property == u8"visuals") {
				return property_info::find_member_pointer_property_info_managed<&element::_visual_params, element>(
					path, get_manager(), property_info::make_typed_modification_callback<element>([](element &e) {
						e.invalidate_visual();
					})
				);
			}
			if (path.front().property == u8"cursor") {
				return property_info::find_member_pointer_property_info<&element::_custom_cursor, element>(path);
			}
			if (path.front().property == u8"visibility") {
				return property_info::make_getter_setter_property_info<element, visibility>(
					[](const element &e) {
						return e.get_visibility();
					},
					[](element &e, visibility vis) {
						e.set_visibility(vis);
					},
					u8"element.visibility"
				);
			}
			if (path.front().property == u8"z_index") {
				return property_info::make_getter_setter_property_info<element, int>(
					[](const element &e) {
						return e.get_zindex();
					},
					[](element &e, int val) {
						e.set_zindex(val);
					},
					u8"element.z_index"
				);
			}
			if (path.front().property == u8"clip_to_bounds") {
				return property_info::make_getter_setter_property_info<element, bool>(
					[](const element &e) {
						return e.get_clip_to_bounds();
					},
					[](element &e, bool val) {
						e.set_clip_to_bounds(val);
					},
					u8"element.clip_to_bounds"
				);
			}

		}
		logger::get().log_error() <<
			"invalid property path: " << property_path::to_string(path.begin(), path.end());
		return property_info();
	}

	void element::_start_animation(std::unique_ptr<playing_animation_base> ani) {
		{ // remove animations with the same subject
			auto it = _animations.begin();
			while (it != _animations.end()) {
				if (it->animation->get_accessor().equals(ani->get_accessor())) {
					get_manager().get_scheduler().cancel_synchronous_task(it->task);
					it = _animations.erase(it);
				} else {
					++it;
				}
			}
		}
		// start the animation
		_animations.emplace_back();
		std::list<_animation_info>::iterator it = --_animations.end();
		it->animation = std::move(ani);
		it->task = get_manager().get_scheduler().register_synchronous_task(
			scheduler::clock_t::now(), this,
			[it](element *elem) -> std::optional<scheduler::clock_t::time_point> {
				auto now = scheduler::clock_t::now();
				if (auto next_update = it->animation->update(now)) {
					return now + next_update.value();
				} else {
					// erase entry in _animations
					elem->_animations.erase(it);
					return std::nullopt;
				}
			}
		);
	}

	void element::_on_desired_size_changed() {
		if (_parent != nullptr) {
			_parent->_on_child_desired_size_changed(*this);
		}
	}

	void element::_on_layout_changed() {
		if (
			std::isnan(get_layout().xmin) || std::isnan(get_layout().xmax) ||
			std::isnan(get_layout().ymin) || std::isnan(get_layout().ymax)
		) {
			logger::get().log_warning() <<
				"layout system produced nan on " << demangle(typeid(*this).name());
		}
		invalidate_visual();
		layout_changed.invoke();
	}

	void element::_on_mouse_down(mouse_button_info &p) {
		if (p.button == mouse_button::primary) {
			if (is_visible(visibility::focus) && !p.is_focus_set()) {
				p.mark_focus_set();
				get_manager().get_scheduler().set_focused_element(this);
			}
		}
		mouse_down.invoke(p);
	}

	void element::_on_layout_parameters_changed() {
		if (parent()) {
			parent()->_on_child_layout_parameters_changed(*this);
		}
		invalidate_layout();
	}

	void element::_on_visibility_changed(_visibility_changed_info &p) {
		visibility changed = p.old_value ^ get_visibility();
		if ((changed & visibility::layout) != visibility::none) {
			invalidate_layout();
		} else if ((changed & visibility::visual) != visibility::none) {
			invalidate_visual();
		}
		// TODO handle visibility::focus
	}

	void element::_on_prerender() {
		vec2d offset = get_layout().xmin_ymin();
		if (parent()) {
			offset -= parent()->get_layout().xmin_ymin();
		}
		get_manager().get_renderer().push_matrix_mult(
			matd3x3::translate(offset) * get_visual_parameters().transform.get_matrix(get_layout().size())
		);
		if (_clip_to_bounds) {
			get_manager().get_renderer().push_rectangle_clip(rectd::from_corners(vec2d(), _layout.size()));
		}
	}

	void element::_custom_render() const {
		vec2d unit = get_layout().size();
		for (const generic_visual_geometry &g : get_visual_parameters().geometries) {
			g.draw(unit, get_manager().get_renderer());
		}
		/*get_manager().get_renderer().draw_rectangle(
			rectd::from_corner_and_size(vec2d(), unit),
			ui::generic_brush(),
			ui::generic_pen(ui::generic_brush(ui::brushes::solid_color(colord(1.0, 0.0, 0.0, 1.0))))
		);*/
	}

	void element::_on_postrender() {
		if (_clip_to_bounds) {
			get_manager().get_renderer().pop_clip();
		}
		get_manager().get_renderer().pop_matrix();
	}

	void element::_on_render() {
		if (is_visible(visibility::visual)) {
			_on_prerender();
			_custom_render();
			_on_postrender();
		}
	}

	bool element::_register_event(std::u8string_view name, std::function<void()> callback) {
		return
			_event_helpers::try_register_event(name, u8"mouse_enter", mouse_enter, callback) ||
			_event_helpers::try_register_event(name, u8"mouse_leave", mouse_leave, callback) ||
			_event_helpers::try_register_event(name, u8"got_focus", got_focus, callback) ||
			_event_helpers::try_register_event(name, u8"lost_focus", lost_focus, callback) ||
			_event_helpers::try_register_all_mouse_button_events<true>(name, mouse_down, callback) ||
			_event_helpers::try_register_all_mouse_button_events<false>(name, mouse_up, callback);
	}

	void element::_initialize() {
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = true;
#endif
	}

	void element::_dispose() {
		if (_parent) {
			_parent->_children.remove(*this);
		}
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = false;
#endif
	}
}
