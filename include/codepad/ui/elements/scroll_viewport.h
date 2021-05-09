// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of the scroll viewport.

#include "codepad/ui/panel.h"
#include "scrollbar.h"

namespace codepad::ui {
	/// A panel where all its children are laid out according to scroll values and can be moved around. Note that
	/// this element does not contain any scrollbars. A `virtual size' is calculated using the pixel sizes of all
	/// children, which is then used when scrolling. It's valid to scroll outside this virtual area.
	class scroll_viewport : public panel {
	public:
		/// Returns the desired size of all children as if the available size is infinitely large. This size does not
		/// include padding.
		[[nodiscard]] virtual vec2d get_virtual_panel_size() {
			panel_desired_size_accumulator
				hori_accum(std::numeric_limits<double>::infinity(), orientation::horizontal),
				vert_accum(std::numeric_limits<double>::infinity(), orientation::vertical);
			for (element *child : _children.items()) {
				if (child->is_visible(visibility::layout)) {
					hori_accum.accumulate(*child);
					vert_accum.accumulate(*child);
				}
			}
			return vec2d(hori_accum.maximum_size, vert_accum.maximum_size);
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

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"scroll_viewport";
		}
	protected:
		vec2d _scroll_offset; ///< The scrolling offset of all children.

		/// Updates the layout of all elements.
		void _on_update_children_layout() override {
			vec2d client_size = get_client_region().size();
			vec2d virtual_size = get_virtual_panel_size();
			vec2d layout_size = vec2d(
				std::max(client_size.x, virtual_size.x),
				std::max(client_size.y, virtual_size.y)
			);
			vec2d top_left = get_client_region().xmin_ymin() - _scroll_offset;
			rectd virtual_client = rectd::from_corner_and_size(top_left, layout_size);
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
		void _on_child_desired_size_changed(element &child) override {
			panel::_on_child_desired_size_changed(child);
			if (
				child.get_width_allocation() == size_allocation_type::automatic ||
				child.get_height_allocation() == size_allocation_type::automatic
			) {
				_on_virtual_panel_size_changed();
			}
		}

		/// When the padding's changed, the virtual panel size changes as well.
		void _on_layout_parameters_changed() override {
			panel::_on_layout_parameters_changed();
			_on_virtual_panel_size_changed();
		}
	};

	/// A panel containing a \ref scroll_viewport and optionally one or two scrollbars. It's possible and valid to
	/// scroll outside of the virtual size of the \ref scroll_viewport when no scrollbar is present on a specific
	/// orientation, but when a scrollbar is present it will limit the offset range.
	class scroll_view : public panel {
	public:
		/// Returns the underlying \ref scroll_viewport.
		[[nodiscard]] scroll_viewport *get_viewport() {
			return _viewport;
		}
		/// Returns the underlying \ref scroll_viewport.
		[[nodiscard]] const scroll_viewport *get_viewport() const {
			return _viewport;
		}

		/// Returns \ref _hori_scroll. This may be \p nullptr.
		[[nodiscard]] const scrollbar *get_horizontal_scrollbar() const {
			return _hori_scroll;
		}
		/// Returns \ref _vert_scroll. This may be \p nullptr.
		[[nodiscard]] const scrollbar *get_vertical_scrollbar() const {
			return _vert_scroll;
		}

		/// Adjusts the offset of \ref _viewport or scrollbar values to make sure that the given region is visible.
		void make_region_visible(rectd);

		/// Moves the visible range so that it does not fall outside of the virtual size. This function makes
		/// adjustments even if there are no scrollbars in an orientation. If the virtual size is smaller than the
		/// visible range in an orientation, the offset is set to zero.
		void clamp_to_valid_range() {
			if (_hori_scroll && _vert_scroll) {
				return;
			}

			vec2d offset = _viewport->get_scroll_offset();
			vec2d virtual_size = _viewport->get_virtual_panel_size();
			if (_hori_scroll == nullptr) {
				// works even when virtual panel width is smaller
				offset.x = std::max(0.0, std::min(
					virtual_size.x - _viewport->get_client_region().width(), offset.x
				));
			}
			if (_vert_scroll == nullptr) {
				offset.y = std::max(0.0, std::min(
					virtual_size.y - _viewport->get_client_region().height(), offset.y
				));
			}
			_viewport->set_scroll_offset(offset);
		}

		/// Returns the name of \ref _viewport.
		inline static std::u8string_view get_viewport_name() {
			return u8"viewport";
		}
		/// Returns the name of \ref _hori_scroll.
		inline static std::u8string_view get_horizontal_scrollbar_name() {
			return u8"horizontal_scrollbar";
		}
		/// Returns the name of \ref _vert_scroll.
		inline static std::u8string_view get_vertical_scrollbar_name() {
			return u8"vertical_scrollbar";
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"scroll_view";
		}
	protected:
		scroll_viewport *_viewport = nullptr; ///< The \ref scroll_viewport.
		scrollbar
			*_hori_scroll = nullptr, ///< The horizontal scrollbar.
			*_vert_scroll = nullptr; ///< The vertical scrollbar.
		double
			_vert_scroll_delta = 30.0, ///< Vertical distance per scroll `tick'.
			_hori_scroll_delta = 30.0; ///< Horizontal distance per scroll `tick'.

		/// Handles mouse scroll events.
		void _on_mouse_scroll(mouse_scroll_info&) override;

		/// Updates the parameters of all scrollbars.
		void _update_scrollbar_params() {
			vec2d virtual_size = _viewport->get_virtual_panel_size();
			if (_hori_scroll) {
				_hori_scroll->set_params(
					virtual_size.x + _viewport->get_padding().width(),
					_viewport->get_layout().width()
				);
			}
			if (_vert_scroll) {
				_vert_scroll->set_params(
					virtual_size.y + _viewport->get_padding().height(),
					_viewport->get_layout().height()
				);
			}
		}

		/// Handles the \p horizontal_delta and \p vertical_delta properties.
		property_info _find_property_path(const property_path::component_list&) const override;

		/// Handles \ref _viewport, \ref _hori_scroll, and \ref _vert_scroll. Also handles
		/// \ref scroll_viewport::virtual_panel_size_changed, as well as offset changes of \ref _hori_scroll and
		/// \ref _vert_scroll.
		bool _handle_reference(std::u8string_view, element*) override;
	};
}
