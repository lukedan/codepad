// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of \ref codepad::editors::binary::editor.

#include "../../ui/panel.h"
#include "../../ui/common_elements.h"

namespace codepad::editors::binary {
	class contents_region;

	/// The editor region that contains a \ref contents_region and other elements.
	class editor : public ui::panel_base {
	public:
		/// Sets the vertical position of this \ref editor.
		void set_vertical_position(double p) {
			_vert_scroll->set_value(p);
		}
		/// Returns the vertical position of this \ref editor.
		double get_vertical_position() const {
			return _vert_scroll->get_value();
		}
		/// Sets the horizontal position of this \ref editor.
		void set_horizontal_position(double p) {
			_hori_scroll->set_value(p);
		}
		/// Returns the horizontal position of this \ref editor.
		double get_horizontal_position() const {
			return _hori_scroll->get_value();
		}
		/// Returns the combined results of \ref get_vertical_position() and \ref get_horizontal_position().
		vec2d get_position() const {
			return vec2d(get_horizontal_position(), get_vertical_position());
		}

		/// Returns the associated \ref contents_region.
		contents_region *get_contents_region() const {
			return _contents;
		}

		info_event<>
			/// Event invoked when the vertical position or viewport size has changed.
			vertical_viewport_changed,
			/// Event invoked when the horizontal position or viewport size has changed.
			horizontal_viewport_changed;

		/// Returns the default class of all elements of type \ref editor.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("binary_editor");
		}

		/// Returns the role identifier of the vertical scrollbar.
		inline static str_view_t get_vertical_scrollbar_role() {
			return CP_STRLIT("vertical_scrollbar");
		}
		/// Returns the role identifier of the horizontal scrollbar.
		inline static str_view_t get_horizontal_scrollbar_role() {
			return CP_STRLIT("horizontal_scrollbar");
		}
		/// Returns the role identifier of the contents_region.
		inline static str_view_t get_contents_region_role() {
			return CP_STRLIT("contents_region");
		}
		/// Returns the role identifier of the `components' panel.
		inline static str_view_t get_components_panel_role() {
			return CP_STRLIT("components_panel");
		}
	protected:
		ui::scrollbar
			*_vert_scroll = nullptr, ///< The vertical \ref ui::scrollbar.
			*_hori_scroll = nullptr; ///< The horizontal \ref ui::scrollbar.
		contents_region *_contents = nullptr; ///< The associated \ref contents_region.
		info_event<>::token _visual_changed_tok; ///< Used to listen to \ref contents_region::content_visual_changed.

		/// Resets the parameters of \ref _vert_scroll.
		void _reset_scrollbars();

		/// Scrolls the viewport of the \ref editor.
		void _on_mouse_scroll(ui::mouse_scroll_info&) override;

		/// Initializes \ref _vert_scroll and \ref _contents.
		void _initialize(str_view_t, const ui::element_metrics&) override;
		/// Unregisters events from \ref _contents.
		void _dispose() override;
	};

	/// Utility functions for elements designed to work as components of \ref editor.
	namespace component_helper {
		/// Returns the \ref editor that's the logical parent of the given \ref ui::element.
		inline editor *get_editor(const ui::element &e) {
			return dynamic_cast<editor*>(e.logical_parent());
		}
	}
}
