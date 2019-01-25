// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codebox.h"

/// \file
/// Implementation of certain functions of \ref codepad::editor::code::codebox.

#include <vector>

#include "editor.h"


using namespace std;

using namespace codepad::ui;

namespace codepad::editor::code {
	/// \todo Have \ref editor listen to \ref vertical_viewport_changed and call
	///       \ref editor::_update_window_caret_position.
	void codebox::_initialize(const str_t &cls, const element_metrics &metrics) {
		panel_base::_initialize(cls, metrics);

		get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
			{get_vertical_scrollbar_role(), _role_cast(_vscroll)},
			/*{get_horizontal_scrollbar_role(), ...},*/ // TODO
			{get_components_panel_role(), _role_cast(_components)},
			{get_editor_role(), _role_cast(_editor)}
			});

		_vscroll->value_changed += [this](value_update_info<double> &info) {
			_editor->_update_window_caret_position(); // update caret position
			vertical_viewport_changed.invoke(info);
			invalidate_visual();
		};

		_mod_tok = (_editor->editing_visual_changed += [this]() {
			_reset_scrollbars();
			});
	}
	void codebox::_dispose() {
		while (_components->children().items().front() != _editor) {
			element *e = _components->children().items().front();
			_components->children().remove(*e);
			if (get_dispose_children()) {
				get_manager().get_scheduler().mark_for_disposal(*e);
			}
		}
		while (_components->children().items().back() != _editor) {
			element *e = _components->children().items().back();
			_components->children().remove(*e);
			if (get_dispose_children()) {
				get_manager().get_scheduler().mark_for_disposal(*e);
			}
		}
		_editor->editing_visual_changed -= _mod_tok;
		panel_base::_dispose();
	}

	void codebox::_reset_scrollbars() {
		_vscroll->set_params(_editor->get_vertical_scroll_range(), _editor->get_layout().height());
	}

	void codebox::_on_mouse_scroll(mouse_scroll_info &p) {
		_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.offset);
		p.mark_handled();
	}

	void codebox::_on_key_down(key_info &p) {
		_editor->_on_key_down(p);
	}

	void codebox::_on_key_up(key_info &p) {
		_editor->_on_key_up(p);
	}

	void codebox::_on_keyboard_text(text_info &p) {
		_editor->_on_keyboard_text(p);
	}

	void codebox::_on_got_focus() {
		_editor->_on_codebox_got_focus();
		panel_base::_on_got_focus();
	}

	void codebox::_on_lost_focus() {
		_editor->_on_codebox_lost_focus();
		panel_base::_on_lost_focus();
	}


	namespace component_helper {
		editor &get_editor(const ui::element &elem) {
			return get_box(elem).get_editor();
		}
	}
}
