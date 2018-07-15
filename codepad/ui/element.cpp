#include "element.h"

/// \file
/// Implementation of certain methods related to ui::element.

#include "../os/window.h"
#include "panel.h"
#include "manager.h"

using namespace codepad::os;

namespace codepad::ui {
	void element::invalidate_layout() {
		manager::get().invalidate_layout(*this);
	}

	void element::revalidate_layout() {
		manager::get().revalidate_layout(*this);
	}

	void element::invalidate_visual() {
		manager::get().invalidate_visual(*this);
	}

	void element::_on_mouse_down(mouse_button_info &p) {
		if (p.button == input::mouse_button::primary) {
			if (_can_focus && !p.focus_set()) {
				p.mark_focus_set();
				get_window()->set_window_focused_element(*this);
			}
			_set_state_bit(manager::get().get_predefined_states().mouse_down, true);
		}
		mouse_down.invoke(p);
	}

	void element::_on_mouse_enter() {
		_set_state_bit(manager::get().get_predefined_states().mouse_over, true);
		mouse_enter.invoke();
	}

	void element::_on_mouse_leave() {
		_set_state_bit(manager::get().get_predefined_states().mouse_over, false);
		mouse_leave.invoke();
	}

	void element::_on_got_focus() {
		_set_state_bit(manager::get().get_predefined_states().focused, true);
		got_focus.invoke();
	}

	void element::_on_lost_focus() {
		_set_state_bit(manager::get().get_predefined_states().focused, false);
		lost_focus.invoke();
	}

	void element::_on_mouse_up(mouse_button_info &p) {
		if (p.button == os::input::mouse_button::primary) {
			_set_state_bit(manager::get().get_predefined_states().mouse_down, false);
		}
		mouse_up.invoke(p);
	}

	void element::_on_render() {
		if (!_rst.test_state_bits(manager::get().get_predefined_states().invisible)) {
			_on_prerender();
			if (_rst.update_and_render(get_layout())) {
				invalidate_visual();
			}
			_custom_render();
			_on_postrender();
		}
	}

	void element::_recalc_horizontal_layout(double xmin, double xmax) {
		auto wprop = get_layout_width();
		_layout.xmin = xmin;
		_layout.xmax = xmax;
		layout_on_direction(
			test_bit_all(_anchor, anchor::left), wprop.second, test_bit_all(_anchor, anchor::right),
			_layout.xmin, _layout.xmax, _margin.left, wprop.first, _margin.right
		);
	}

	void element::_recalc_vertical_layout(double ymin, double ymax) {
		auto hprop = get_layout_height();
		_layout.ymin = ymin;
		_layout.ymax = ymax;
		layout_on_direction(
			test_bit_all(_anchor, anchor::top), hprop.second, test_bit_all(_anchor, anchor::bottom),
			_layout.ymin, _layout.ymax, _margin.top, hprop.first, _margin.bottom
		);
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
		return _rst.test_state_bits(manager::get().get_predefined_states().mouse_over);
	}

	bool element::is_visible() const {
		return !_rst.test_state_bits(manager::get().get_predefined_states().invisible);
	}

	void element::set_visibility(bool val) {
		_set_state_bit(manager::get().get_predefined_states().invisible, !val);
	}

	bool element::is_interactive() const {
		return !_rst.test_state_bits(manager::get().get_predefined_states().ghost);
	}

	void element::set_is_interactive(bool val) {
		_set_state_bit(manager::get().get_predefined_states().ghost, !val);
	}

	bool element::is_focused() const {
		return _rst.test_state_bits(manager::get().get_predefined_states().focused);
	}


	void decoration::_on_visual_changed() {
		if (_wnd) {
			_wnd->invalidate_visual();
		}
	}

	decoration::~decoration() {
		if (_wnd) {
			_wnd->_on_decoration_destroyed(*this);
		}
	}
}
