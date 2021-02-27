// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/scroll_viewport.h"

/// \file
/// Implementation of \ref codepad::ui::scroll_viewport.

namespace codepad::ui {
	void scroll_view::make_region_visible(rectd range) {
		// although scrollbar::make_range_visible_axis does this correction, we need to do it earlier here because
		// otherwise padding width/height will be added to the wrong component
		range = range.made_positive_swap();
		thickness padding = _viewport->get_padding();
		range.xmax += padding.width();
		range.ymax += padding.height();

		if (_hori_scroll) {
			_hori_scroll->make_range_visible(range.xmin, range.xmax);
		} else {
			if (auto newpos = scrollbar::make_range_visible_axis(
				range.xmin, range.xmax, _viewport->get_scroll_offset().x, _viewport->get_layout().width()
			)) {
				_viewport->set_scroll_offset(vec2d(newpos.value(), _viewport->get_scroll_offset().y));
			}
		}

		if (_vert_scroll) {
			_vert_scroll->make_range_visible(range.ymin, range.ymax);
		} else {
			if (auto newpos = scrollbar::make_range_visible_axis(
				range.ymin, range.ymax, _viewport->get_scroll_offset().y, _viewport->get_layout().height()
			)) {
				_viewport->set_scroll_offset(vec2d(_viewport->get_scroll_offset().x, newpos.value()));
			}
		}
	}

	void scroll_view::_on_mouse_scroll(mouse_scroll_info &p) {
		panel::_on_mouse_scroll(p);

		if (_hori_scroll) {
			_hori_scroll->handle_scroll_event(p, _hori_scroll_delta);
		}
		if (_vert_scroll) {
			_vert_scroll->handle_scroll_event(p, _vert_scroll_delta);
		}
	}

	property_info scroll_view::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"scroll_view")) {
			if (path.front().property == u8"horizontal_delta") {
				return property_info::find_member_pointer_property_info<
					&scroll_view::_hori_scroll_delta, element
				>(path);
			}
			if (path.front().property == u8"vertical_delta") {
				return property_info::find_member_pointer_property_info<
					&scroll_view::_vert_scroll_delta, element
				>(path);
			}
		}
		return panel::_find_property_path(path);
	}

	void scroll_view::_initialize(std::u8string_view cls) {
		panel::_initialize(cls);

		_viewport->virtual_panel_size_changed += [this]() {
			_update_scrollbar_params();
		};
		_viewport->layout_changed += [this]() {
			_update_scrollbar_params();
		};

		if (_hori_scroll) {
			_hori_scroll->actual_value_changed += [this](scrollbar::value_changed_info&) {
				_viewport->set_scroll_offset(vec2d(
					_hori_scroll->get_actual_value(), _viewport->get_scroll_offset().y
				));
			};
		}

		if (_vert_scroll) {
			_vert_scroll->actual_value_changed += [this](scrollbar::value_changed_info&) {
				_viewport->set_scroll_offset(vec2d(
					_viewport->get_scroll_offset().x, _vert_scroll->get_actual_value()
				));
			};
		}
	}
}
