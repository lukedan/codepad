// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "window.h"

/// \file
/// Implementation of certain functions of \ref codepad::ui::window_base.

#include <vector>

#include "manager.h"

using namespace std;

namespace codepad::ui {
	void window_base::_on_prerender() {
		get_manager().get_renderer().begin_drawing(*this);
		get_manager().get_renderer().clear(colord(0.0, 0.0, 0.0, 0.0));
		panel::_on_prerender();
	}

	void window_base::_on_postrender() {
		panel::_on_postrender();
		get_manager().get_renderer().end_drawing();
	}

	void window_base::_initialize(str_view_t cls, const element_configuration &metrics) {
		panel::_initialize(cls, metrics);
		_is_focus_scope = true;
		get_manager().get_renderer()._new_window(*this);
	}

	void window_base::_dispose() {
		// here we call _on_removing_element to ensure that the focus has been properly updated
		get_manager().get_scheduler()._on_removing_element(*this);
		get_manager().get_renderer()._delete_window(*this);
		panel::_dispose();
	}

	void window_base::_on_size_changed(size_changed_info &p) {
		get_manager().get_scheduler().notify_layout_change(*this);
		size_changed(p);
	}

	void window_base::_on_scaling_factor_changed(scaling_factor_changed_info &p) {
		invalidate_visual();
		scaling_factor_changed(p);
	}

	void window_base::_on_key_down(key_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_key_down(p);
		} else {
			panel::_on_key_down(p);
		}
	}

	void window_base::_on_key_up(key_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_key_up(p);
		} else {
			panel::_on_key_up(p);
		}
	}

	void window_base::_on_keyboard_text(text_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_keyboard_text(p);
		} else {
			panel::_on_keyboard_text(p);
		}
	}

	void window_base::_on_composition(composition_info &p) {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_composition(p);
		} else {
			panel::_on_composition(p);
		}
	}

	void window_base::_on_composition_finished() {
		element *focus = get_manager().get_scheduler().get_focused_element();
		if (focus && focus != this) {
			focus->_on_composition_finished();
		} else {
			panel::_on_composition_finished();
		}
	}
}
