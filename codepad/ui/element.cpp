// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "element.h"

/// \file
/// Implementation of certain methods related to ui::element.

#include "../os/misc.h"
#include "window.h"
#include "panel.h"
#include "manager.h"

using namespace codepad::os;

namespace codepad::ui {
	vec2d mouse_position::get(element &e) const { // TODO non-recursive version?
		if (e._cached_mouse_position_timestamp != _timestamp) {
			assert_true_logical(e.parent() != nullptr, "invalid call to get mouse position");
			vec2d parentpos = get(*e.parent());
			e._cached_mouse_position = e._params.visual_parameters.transform.inverse_transform_point(
				parentpos - (e.get_layout().xmin_ymin() - e.parent()->get_layout().xmin_ymin()),
				e.get_layout().size()
			);
			e._cached_mouse_position_timestamp = _timestamp;
		}
		return e._cached_mouse_position;
	}


	void element::invalidate_visual() {
		get_manager().get_scheduler().invalidate_visual(*this);
	}

	void element::invalidate_layout() {
		get_manager().get_scheduler().invalidate_layout(*this);
	}

	void element::_on_desired_size_changed(bool width, bool height) {
		if (
			(width && get_width_allocation() == size_allocation_type::automatic) ||
			(height && get_height_allocation() == size_allocation_type::automatic)
			) { // the change may actually affect its layout
			if (_parent != nullptr) {
				_parent->_on_child_desired_size_changed(*this, width, height);
			}
		}
	}

	void element::_on_mouse_down(mouse_button_info & p) {
		if (p.button == mouse_button::primary) {
			if (is_visible(visibility::focus) && !p.focus_set()) {
				p.mark_focus_set();
				get_manager().get_scheduler().set_focused_element(this);
			}
		}
		mouse_down.invoke(p);
	}

	void element::_on_prerender() {
		vec2d offset = get_layout().xmin_ymin();
		if (parent()) {
			offset -= parent()->get_layout().xmin_ymin();
		}
		get_manager().get_renderer().push_matrix_mult(
			matd3x3::translate(offset) * _params.visual_parameters.transform.get_matrix(get_layout().size())
		);
		/*get_manager().get_renderer().push_clip(_layout.fit_grid_enlarge<int>());*/ // TODO clips
	}

	void element::_custom_render() const {
		/*if (is_mouse_over()) {
			get_manager().get_renderer().draw_rectangle(
				rectd(0.0, get_layout().width(), 0.0, get_layout().height()),
				generic_brush_parameters(brush_parameters::solid_color(colord(1.0, 0.0, 0.0, 0.2))),
				generic_pen_parameters(generic_brush_parameters(brush_parameters::none()))
			);
		}*/
		vec2d unit = get_layout().size();
		for (const generic_visual_geometry &g : _params.visual_parameters.geometries) {
			g.draw(unit, get_manager().get_renderer());
		}
	}

	void element::_on_postrender() {
		/*get_manager().get_renderer().pop_clip();*/
		get_manager().get_renderer().pop_matrix();
	}

	void element::_on_render() {
		if (is_visible(visibility::visual)) {
			_on_prerender();
			_custom_render();
			_on_postrender();
		}
	}

	void element::_initialize(str_view_t cls, const element_configuration & config) {
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = true;
#endif
		_params = config.default_parameters;
		for (auto &p : config.event_triggers) {
			_register_event(p.first, [this, storyboard = &p.second]() {
				for (auto &ani : storyboard->animations) {
					get_manager().get_scheduler().start_animation(ani.start_for(*this), this);
				}
			});
		}
		for (auto &attr : config.additional_attributes) {
			_set_attribute(attr.first, attr.second);
		}
		_hotkeys = get_manager().get_class_hotkeys().get(cls);
	}

	void element::_dispose() {
		if (_parent) {
			_parent->_children.remove(*this);
		}
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = false;
#endif
	}

	void element::set_zindex(int v) {
		if (_parent) {
			_parent->_children.set_zindex(*this, v);
		} else {
			_zindex = v;
		}
	}

	window_base *element::get_window() {
		element *cur = this;
		while (cur->_parent != nullptr) {
			cur = cur->_parent;
		}
		return dynamic_cast<window_base*>(cur);
	}


	void decoration::set_layout(rectd r) {
		_layout = r;
		if (_wnd) {
			_wnd->invalidate_visual();
		}
	}

	decoration::~decoration() {
		if (_wnd) {
			_wnd->_on_decoration_destroyed(*this);
		}
	}

	void decoration::set_class(const str_t &cls) {
		_class = cls;
		// TODO
	}
}
