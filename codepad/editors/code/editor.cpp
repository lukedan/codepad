// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "editor.h"

/// \file
/// Implementation of certain functions of \ref codepad::editors::code::editor.

#include <vector>

#include "contents_region.h"

using namespace std;

using namespace codepad::ui;

namespace codepad::editors::code {
	/// \todo Have \ref contents_region listen to \ref vertical_viewport_changed and call
	///       \ref contents_region::_update_window_caret_position.
	void editor::_initialize(str_view_t cls, const element_metrics &metrics) {
		panel_base::_initialize(cls, metrics);

		get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
			{get_vertical_scrollbar_role(), _role_cast(_vscroll)},
			/*{get_horizontal_scrollbar_role(), ...},*/ // TODO
			{get_components_panel_role(), _role_cast(_components)},
			{get_contents_region_role(), _role_cast(_contents)}
			});

		_vscroll->value_changed += [this](value_update_info<double>&) {
			_contents->_update_window_caret_position(); // update caret position
			vertical_viewport_changed.invoke();
			invalidate_visual();
		};

		_contents->layout_changed += [this]() {
			vertical_viewport_changed.invoke();
			horizontal_viewport_changed.invoke();
			_reset_scrollbars();
		};
		_vis_changed_tok = (_contents->editing_visual_changed += [this]() {
			_reset_scrollbars();
		});
	}

	void editor::_dispose() {
		_contents->editing_visual_changed -= _vis_changed_tok;
		while (!_components->children().items().empty()) {
			element *e = _components->children().items().front();
			_components->children().remove(*e);
			if (get_dispose_children()) {
				get_manager().get_scheduler().mark_for_disposal(*e);
			}
		}
		panel_base::_dispose();
	}

	void editor::_reset_scrollbars() {
		_vscroll->set_params(_contents->get_vertical_scroll_range(), _contents->get_layout().height());
	}

	void editor::_on_mouse_scroll(mouse_scroll_info &p) {
		_vscroll->set_value(_vscroll->get_value() - _contents->get_scroll_delta() * p.offset);
		p.mark_handled();
	}

	void editor::_on_key_down(key_info &p) {
		_contents->_on_key_down(p);
	}

	void editor::_on_key_up(key_info &p) {
		_contents->_on_key_up(p);
	}

	void editor::_on_keyboard_text(text_info &p) {
		_contents->_on_keyboard_text(p);
	}

	void editor::_on_got_focus() {
		_contents->_on_codebox_got_focus();
		panel_base::_on_got_focus();
	}

	void editor::_on_lost_focus() {
		_contents->_on_codebox_lost_focus();
		panel_base::_on_lost_focus();
	}


	namespace component_helper {
		contents_region *get_contents_region(const ui::element &elem) {
			return get_core_components(elem).second;
		}

		std::pair<editor*, contents_region*> get_core_components(const ui::element &elem) {
			if (editor *box = get_box(elem)) {
				return {box, box->get_contents_region()};
			}
			return {nullptr, nullptr};
		}
	}
}
