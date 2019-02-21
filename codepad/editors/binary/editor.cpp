// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "editor.h"

/// \file
/// Implementation of certain methods of \ref codepad::editors::binary::editor.

#include "contents_region.h"

using namespace codepad::ui;

namespace codepad::editors::binary {
	void editor::_initialize(str_view_t cls, const element_metrics &metrics) {
		panel_base::_initialize(cls, metrics);

		get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
			{get_vertical_scrollbar_role(), _role_cast(_vert_scroll)},
			{get_horizontal_scrollbar_role(), _role_cast(_hori_scroll)},
			// TODO component area
			{get_contents_region_role(), _role_cast(_contents)}
			});

		_vert_scroll->value_changed += [this](value_update_info<double>&) {
			vertical_viewport_changed.invoke();
			invalidate_visual();
		};
		_hori_scroll->value_changed += [this](value_update_info<double>&) {
			horizontal_viewport_changed.invoke();
			invalidate_visual();
		};

		_contents->layout_changed += [this]() {
			vertical_viewport_changed.invoke();
			horizontal_viewport_changed.invoke();
			_reset_scrollbars();
		};
		_visual_changed_tok = (_contents->content_visual_changed += [this]() {
			_reset_scrollbars();
		});
	}

	void editor::_dispose() {
		_contents->content_visual_changed -= _visual_changed_tok;

		panel_base::_dispose();
	}

	void editor::_on_mouse_scroll(mouse_scroll_info &p) {
		_vert_scroll->set_value(_vert_scroll->get_value() - _contents->get_scroll_delta() * p.offset);
		// TODO horizontal scrolling?
		p.mark_handled();
	}

	void editor::_reset_scrollbars() {
		_vert_scroll->set_params(_contents->get_vertical_scroll_range(), _contents->get_layout().height());
		double wrange = _contents->get_layout().width();
		_hori_scroll->set_params(std::max(_contents->get_horizontal_scroll_range(), wrange), wrange);
	}
}
