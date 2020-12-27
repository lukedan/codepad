// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/scroll_viewport.h"

/// \file
/// Implementation of \ref codepad::ui::scroll_viewport.

namespace codepad::ui {
	void scroll_view::make_region_visible(rectd range) {
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

	const property_mapping &scroll_view::get_properties() const {
		return get_properties_static();
	}

	const property_mapping &scroll_view::get_properties_static() {
		static property_mapping mapping;
		if (mapping.empty()) {
			mapping = panel::get_properties_static();

			mapping.emplace(
				u8"horizontal_delta", std::make_shared<member_pointer_property<&scroll_view::_hori_scroll_delta>>()
			);
			mapping.emplace(
				u8"vertical_delta", std::make_shared<member_pointer_property<&scroll_view::_vert_scroll_delta>>()
			);
		}

		return mapping;
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
