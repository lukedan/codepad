// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "window.h"

/// \file
/// Implementation of certain functions of \ref codepad::ui::window_base.

#include <vector>

#include "manager.h"

using namespace std;

namespace codepad::ui {
	void window_base::register_decoration(decoration &dec) {
		assert_true_usage(dec._wnd == nullptr, "the decoration has already been registered to another window");
		dec._wnd = this;
		_decos.push_back(&dec);
		dec._tok = --_decos.end();
		// TODO start animations
	}

	void window_base::_on_render() {
		get_manager().get_renderer().begin_drawing(*this);
		get_manager().get_renderer().clear(colord(0.0, 0.0, 0.0, 0.0));
		panel::_on_render();
		get_manager().get_renderer().end_drawing();
	}

	void window_base::_initialize(str_view_t cls, const element_configuration &metrics) {
		panel::_initialize(cls, metrics);
		_is_focus_scope = true;
		get_manager().get_renderer()._new_window(*this);
	}

	void window_base::_dispose() {
		// not removed from parent, but still technically removed
		get_manager().get_scheduler()._on_removing_element(*this);
		// special care taken
		auto i = _decos.begin(), j = i;
		for (; i != _decos.end(); i = j) {
			++j;
			delete *i;
		}
		get_manager().get_renderer()._delete_window(*this);
		panel::_dispose();
	}

	void window_base::_on_size_changed(size_changed_info &p) {
		get_manager().get_scheduler().notify_layout_change(*this);
		size_changed(p);
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

	void window_base::_custom_render() const {
		panel::_custom_render();
		for (decoration *dec : _decos) {
			/*dec->_vis_config.render(get_manager().get_renderer(), dec->_layout);*/
		}
	}
}
