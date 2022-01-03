// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A panel that allows the user to explicitly override the layout of each element.

#include "codepad/ui/panel.h"

namespace codepad::ui {
	/// A panel where an overriden layout is set for each child.
	class overriden_layout_panel : public panel {
	public:
		/// Sets the overriden layout of the given child.
		void set_child_layout(element &e, rectd layout) {
			std::any_cast<_child_data&>(_child_get_parent_data(e)).layout = layout;
			_invalidate_children_layout();
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"overriden_layout_panel";
		}
	protected:
		/// Child data.
		struct _child_data {
			rectd layout; ///< Overriden layout.
		};

		/// Initializes the child data of the element.
		void _on_child_added(element &e, element *before) override {
			_child_get_parent_data(e).emplace<_child_data>();
			panel::_on_child_added(e, before);
		}
		/// Resets the child's data.
		void _on_child_removing(element &e) override {
			panel::_on_child_removing(e);
			_child_get_parent_data(e).reset();
		}

		/// Returns the minimum size that fits the overriden sizes of all children.
		vec2d _compute_desired_size_impl(vec2d) override {
			vec2d result;
			for (element *e : children().items()) {
				auto &layout = std::any_cast<_child_data&>(_child_get_parent_data(*e));
				vec2d p = layout.layout.xmax_ymax();
				result.x = std::max(result.x, p.x);
				result.y = std::max(result.y, p.y);
			}
			return result;
		}
		/// Sets the layout of all children.
		void _on_update_children_layout() override {
			vec2d offset = get_client_region().xmin_ymin();
			for (element *e : children().items()) {
				rectd layout = std::any_cast<_child_data&>(_child_get_parent_data(*e)).layout;
				_child_set_layout(*e, layout.translated(offset));
			}
		}
	};
}
