// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of the scroll viewport.

#include "../panel.h"

namespace codepad::ui {
	/// A panel where all its children are laid out according to scroll values and can be moved around. Note that
	/// this element does not contain any scrollbars. A `virtual size' is calculated using the pixel sizes of all
	/// children, which is then used when scrolling. It's valid to scroll outside this virtual area.
	class scroll_viewport : public panel {
	public:
		/// Returns the maximum pixel width of all children.
		double get_virtual_panel_width() const {
			return _get_max_horizontal_absolute_span(_children).value_or(0.0);
		}
		/// Returns the maximum pixel height of all children.
		double get_virtual_panel_height() const {
			return _get_max_vertical_absolute_span(_children).value_or(0.0);
		}

		/// Sets \ref _scroll_offset, and invokes \ref _on_scroll_offset_changed().
		void set_scroll_offset(vec2d offset) {
			_scroll_offset = offset;
			_on_scroll_offset_changed();
		}
		/// Returns \ref _scroll_offset.
		vec2d get_scroll_offset() const {
			return _scroll_offset;
		}

		info_event<> virtual_panel_size_changed; ///< Invoked whenever the size of the virtual panel is changed.
	protected:
		vec2d _scroll_offset; ///< The scrolling offset of all children.

		/// Updates the layout of all elements.
		void _on_update_children_layout() override {
			vec2d
				virtual_size(get_virtual_panel_width(), get_virtual_panel_height()),
				top_left = get_client_region().xmin_ymin() + _scroll_offset;
			rectd virtual_client = rectd::from_corner_and_size(top_left, virtual_size);
			for (element *e : children().items()) {
				panel::layout_child(*e, virtual_client);
			}
		}

		/// Called when \ref _scroll_offset is changed. Calls \ref _invalidate_children_layout().
		void _on_scroll_offset_changed() {
			_invalidate_children_layout();
		}

		/// Called when the size of the virtual panel is potentially changed; invokes
		/// \ref virtual_panel_size_changed.
		void _on_virtual_panel_size_changed() {
			virtual_panel_size_changed.invoke();
		}

		/// Invokes \ref _on_virtual_panel_size_changed().
		void _on_child_layout_parameters_changed(element &e) override {
			panel::_on_child_layout_parameters_changed(e);
			_on_virtual_panel_size_changed();
		}
		/// If the size allocation of a changed orientation is automatic, invokes
		/// \ref _on_virtual_panel_size_changed().
		void _on_child_desired_size_changed(element &child, bool width, bool height) override {
			panel::_on_child_desired_size_changed(child, width, height);
			if (
				(width && child.get_width_allocation() == size_allocation_type::automatic) ||
				(height && child.get_height_allocation() == size_allocation_type::automatic)
			) {
				_on_virtual_panel_size_changed();
			}
		}
	};
}
