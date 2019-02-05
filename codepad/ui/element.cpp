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

	void element::_on_mouse_down(mouse_button_info &p) {
		if (p.button == mouse_button::primary) {
			if (_can_focus && !p.focus_set()) {
				p.mark_focus_set();
				get_manager().get_scheduler().set_focused_element(this);
			}
			set_state_bits(get_manager().get_predefined_states().mouse_down, true);
		}
		mouse_down.invoke(p);
	}

	void element::_on_mouse_enter() {
		set_state_bits(get_manager().get_predefined_states().mouse_over, true);
		mouse_enter.invoke();
	}

	void element::_on_mouse_leave() {
		set_state_bits(get_manager().get_predefined_states().mouse_over, false);
		mouse_leave.invoke();
	}

	void element::_on_got_focus() {
		set_state_bits(get_manager().get_predefined_states().focused, true);
		for (element *e = this; e; e = e->parent()) {
			e->set_state_bits(get_manager().get_predefined_states().child_focused, true);
		}
		got_focus.invoke();
	}

	void element::_on_lost_focus() {
		set_state_bits(get_manager().get_predefined_states().focused, false);
		for (element *e = this; e; e = e->parent()) {
			e->set_state_bits(get_manager().get_predefined_states().child_focused, false);
		}
		lost_focus.invoke();
	}

	void element::_on_mouse_up(mouse_button_info &p) {
		if (p.button == mouse_button::primary) {
			set_state_bits(get_manager().get_predefined_states().mouse_down, false);
		}
		mouse_up.invoke(p);
	}

	void element::_on_update_visual_configurations(animation_update_info &info) {
		info.update_configuration(_config.visual_config);
	}

	void element::_on_prerender() {
		get_manager().get_renderer().push_clip(_layout.fit_grid_enlarge<int>());
	}

	void element::_on_render() {
		if (is_render_visible()) {
			_on_prerender();
			_config.visual_config.render(get_manager().get_renderer(), get_layout());
			_custom_render();
			_on_postrender();
		}
	}

	void element::_on_postrender() {
		get_manager().get_renderer().pop_clip();
	}

	void element::_on_state_changed(value_update_info<element_state_id>&) {
		get_manager().get_scheduler().schedule_visual_config_update(*this);
		get_manager().get_scheduler().schedule_metrics_config_update(*this);
	}

	void element::_initialize(const str_t &cls, const element_metrics &metrics) {
#ifdef CP_CHECK_USAGE_ERRORS
		_initialized = true;
#endif
		_config.visual_config = visual_configuration(
			get_manager().get_class_visuals().get_or_default(cls), _state
		);
		_config.metrics_config = metrics_configuration(metrics, _state);
		_config.hotkey_config = get_manager().get_class_hotkeys().try_get(cls);
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

	bool element::is_mouse_over() const {
		return (_state & get_manager().get_predefined_states().mouse_over) != 0;
	}

	bool element::is_render_visible() const {
		return (_state & get_manager().get_predefined_states().render_invisible) == 0;
	}

	void element::set_render_visibility(bool val) {
		set_state_bits(get_manager().get_predefined_states().render_invisible, !val);
	}

	bool element::is_hittest_visible() const {
		return (_state & get_manager().get_predefined_states().hittest_invisible) == 0;
	}

	void element::set_hittest_visibility(bool val) {
		set_state_bits(get_manager().get_predefined_states().hittest_invisible, !val);
	}

	bool element::is_layout_visible() const {
		return (_state & get_manager().get_predefined_states().layout_invisible) == 0;
	}

	void element::set_layout_visibility(bool val) {
		set_state_bits(get_manager().get_predefined_states().layout_invisible, !val);
	}

	bool element::is_focused() const {
		return (_state & get_manager().get_predefined_states().focused) != 0;
	}

	bool element::is_vertical() const {
		return (_state & get_manager().get_predefined_states().vertical) != 0;
	}

	void element::set_is_vertical(bool v) {
		set_state_bits(get_manager().get_predefined_states().vertical, v);
	}


	void decoration::set_layout(rectd r) {
		_layout = r;
		if (_wnd) {
			_wnd->invalidate_visual();
		}
	}

	void decoration::_on_state_invalidated() {
		if (_wnd) {
			_manager.get_scheduler().schedule_visual_config_update(*_wnd);
		}
	}

	decoration::~decoration() {
		if (_wnd) {
			_wnd->_on_decoration_destroyed(*this);
		}
	}

	void decoration::set_class(const str_t &cls) {
		_class = cls;
		_vis_config = visual_configuration(
			_manager.get_class_visuals().get_or_default(_class), _state
		);
		_on_state_invalidated();
	}
}
