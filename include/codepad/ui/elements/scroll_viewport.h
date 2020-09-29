// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of the scroll viewport.

#include "../panel.h"
#include "scrollbar.h"

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

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"scroll_viewport";
		}
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

	/// A panel containing a \ref scroll_viewport and optionally one or two scrollbars. It's possible and valid to
	/// scroll outside of the virtual size of the \ref scroll_viewport when no scrollbar is present on a specific
	/// orientation, but when a scrollbar is present it will limit the offset range.
	class scroll_view : public panel {
	public:
		/// Returns the underlying \ref scroll_viewport.
		const scroll_viewport *get_viewport() const {
			return _viewport;
		}

		/// Returns \ref _hori_scroll. This may be \p nullptr.
		const scrollbar *get_horizontal_scrollbar() const {
			return _hori_scroll;
		}
		/// Returns \ref _vert_scroll. This may be \p nullptr.
		const scrollbar *get_vertical_scrollbar() const {
			return _vert_scroll;
		}

		/// Adjusts the offset of \ref _viewport or scrollbar values to make sure that the given region is visible.
		void make_region_visible(rectd range) {
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
	protected:
		scroll_viewport *_viewport = nullptr; ///< The \ref scroll_viewport.
		scrollbar
			*_hori_scroll = nullptr, ///< The horizontal scrollbar.
			*_vert_scroll = nullptr; ///< The vertical scrollbar.

		/// Returns the mapping that contains notifications for \ref _viewport, \ref _hori_scroll, and
		/// \ref _vert_scroll.
		class_arrangements::notify_mapping _get_child_notify_mapping() override {
			auto mapping = panel::_get_child_notify_mapping();
			mapping.emplace(get_viewport_name(), _name_cast(_viewport));
			mapping.emplace(get_horizontal_scrollbar_name(), _name_cast(_hori_scroll));
			mapping.emplace(get_vertical_scrollbar_name(), _name_cast(_vert_scroll));
			return mapping;
		}

		/// Handles mouse scroll events.
		void _on_mouse_scroll(mouse_scroll_info &p) override {
			panel::_on_mouse_scroll(p);

			if (_hori_scroll) {
				_hori_scroll->handle_scroll_event(p);
			}
			if (_vert_scroll) {
				_vert_scroll->handle_scroll_event(p);
			}
		}

		/// Updates the parameters of all scrollbars.
		void _update_scrollbar_params() {
			if (_hori_scroll) {
				_hori_scroll->set_params(_viewport->get_virtual_panel_width(), _viewport->get_layout().width());
			}
			if (_vert_scroll) {
				_vert_scroll->set_params(_viewport->get_virtual_panel_height(), _viewport->get_layout().height());
			}
		}

		/// Registers handlers for \ref scroll_viewport::virtual_panel_size_changed, as well as for offset changes
		/// of \ref _hori_scroll and \ref _vert_scroll.
		void _initialize(std::u8string_view cls) override {
			panel::_initialize(cls);

			_viewport->virtual_panel_size_changed += [this]() {
				_update_scrollbar_params();
			};
			_viewport->layout_changed += [this]() {
				_update_scrollbar_params();
			};

			if (_hori_scroll) {
				_hori_scroll->actual_value_changed += [this](scrollbar::value_changed_info &info) {
					_viewport->set_scroll_offset(vec2d(
						_hori_scroll->get_actual_value(), _viewport->get_scroll_offset().y
					));
				};
			}

			if (_vert_scroll) {
				_vert_scroll->actual_value_changed += [this](scrollbar::value_changed_info &info) {
					_viewport->set_scroll_offset(vec2d(
						_viewport->get_scroll_offset().x, _vert_scroll->get_actual_value()
					));
				};
			}
		}
	};
}
