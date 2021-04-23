// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Element used to limit the size of its contents.

#include "codepad/ui/panel.h"

namespace codepad::ui {
	/// A panel that limits the size of its children.
	class size_limiter : public panel {
	public:
		/// Clamps the desired size of all children to the target range, between \ref _min_size and \ref _max_size.
		/// If no child has a fixed width value, the minimum width property will be returned.
		size_allocation get_desired_width() const override {
			double width = _min_size.x;
			for (element *child : _children.items()) {
				if (child->is_visible(visibility::layout)) {
					if (auto span = _get_horizontal_absolute_desired_span(*child)) {
						width = std::max(width, std::min(_max_size.x, span.value()));
					}
				}
			}
			return size_allocation::pixels(width);
		}
		/// Clamps the desired size of all children to the target range, between \ref _min_size and \ref _max_size.
		/// If no child has a fixed height value, the minimum height property will be returned.
		size_allocation get_desired_height() const override {
			double height = _min_size.y;
			for (element *child : _children.items()) {
				if (child->is_visible(visibility::layout)) {
					if (auto span = _get_vertical_absolute_desired_span(*child)) {
						height = std::max(height, std::min(_max_size.y, span.value()));
					}
				}
			}
			return size_allocation::pixels(height);
		}


		/// Similar to \ref panel::layout_on_direction(), except this function takes into account the size limits.
		static void layout_on_direction(
			double &clientmin, double &clientmax,
			bool anchormin, bool pixelsize, bool anchormax,
			double marginmin, double size, double marginmax,
			double minsize, double maxsize
		);
		/// Similar to \ref panel::layout_child_horizontal(), except this function takes into account the size
		/// limits.
		void layout_child_horizontal(element&, double xmin, double xmax) const;
		/// Similar to \ref panel::layout_child_vertical(), except this function takes into account the size limits.
		void layout_child_vertical(element&, double ymin, double ymax) const;
		/// Similar to \ref panel::layout_child(), except this function takes into account the size limits.
		void layout_child(element &child, rectd client) const {
			layout_child_horizontal(child, client.xmin, client.xmax);
			layout_child_vertical(child, client.ymin, client.ymax);
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"size_limiter";
		}
	protected:
		vec2d _min_size{ 0.0, 0.0 }; ///< Minimum size of elements.
		/// Maximum size of elements.
		vec2d _max_size{ std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };

		/// Updates the layout of all children using \ref layout_child().
		void _on_update_children_layout() override {
			rectd client = get_client_region();
			for (element *elem : _children.items()) {
				layout_child(*elem, client);
			}
		}

		/// Handles the \p minimum_size and \p maximum_size properties.
		property_info _find_property_path(const property_path::component_list &path) const override {
			if (path.front().is_type_or_empty(u8"size_limiter")) {
				if (path.front().property == u8"minimum_size") {
					return property_info::find_member_pointer_property_info<&size_limiter::_min_size, element>(
						path, property_info::make_typed_modification_callback<element, size_limiter>(
							[](size_limiter &elem) {
								elem._on_desired_size_changed(true, true);
								elem._invalidate_children_layout();
							}
						)
					);
				}
				if (path.front().property == u8"maximum_size") {
					return property_info::find_member_pointer_property_info<&size_limiter::_max_size, element>(
						path, property_info::make_typed_modification_callback<element, size_limiter>(
							[](size_limiter &elem) {
								elem._on_desired_size_changed(true, true);
								elem._invalidate_children_layout();
							}
						)
					);
				}
			}
			return panel::_find_property_path(path);
		}
	};
}
