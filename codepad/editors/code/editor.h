// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of codepad::editors::code::editor and codepad::editors::code::component.

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../../ui/element.h"
#include "../../ui/panel.h"
#include "../../ui/common_elements.h"
#include "../../ui/manager.h"
#include "../../ui/draw.h"

namespace codepad::editors::code {
	class editor;
	class contents_region;

	/// An element that contains an \ref contents_region and other components, and forwards all keyboard events to the
	/// \ref contents_region.
	///
	/// \todo Add horizontal view.
	class editor : public ui::panel_base {
	public:
		/// Sets the vertical position of the view.
		void set_vertical_position(double p) {
			_vscroll->set_value(p);
		}
		/// Returns the current vertical position.
		double get_vertical_position() const {
			return _vscroll->get_value();
		}

		/// Scrolls the view region so that the given point is visible.
		void make_point_visible(vec2d v) {
			_vscroll->make_point_visible(v.y);
		}

		/// Returns a pointer the \ref contents_region associated with this \ref editor.
		contents_region *get_contents_region() const {
			return _contents;
		}

		/// Adds a component before the given element. For example, if \p before is \ref get_contents_region(), then the
		/// component will be inserted before the contents_region.
		void insert_component_before(ui::element *before, ui::element &comp) {
			_child_set_logical_parent(comp, this);
			_components->children().insert_before(before, comp);
		}
		/// Removes the given component.
		void remove_component(ui::element &e) {
			_components->children().remove(e);
		}

		/// Returns the region in which layout calculation for all components is performed.
		rectd get_components_region() const {
			rectd client = get_client_region();
			client.xmax = _vscroll->get_layout().xmin;
			return client;
		}

		info_event<>
			/// Invoked when the vertical position or visible range is changed.
			vertical_viewport_changed,
			/// Invoked when the horizontal position or visible range is changed.
			horizontal_viewport_changed;

		/// Returns the default class of all elements of type \ref editor.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("code_editor");
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
		/// Token used to listen to \ref contents_region::editing_visual_changed, to adjust the parameters of \ref _vscroll
		/// when the height of the document has changed.
		info_event<>::token _vis_changed_tok;

		ui::scrollbar *_vscroll = nullptr; ///< The vertical \ref ui::scrollbar.
		ui::panel *_components = nullptr; ///< The panel that contains all components and the \ref contents_region.
		contents_region *_contents = nullptr; ///< The \ref contents_region.

		/// Calculates and sets the parameters of \ref _vscroll.
		void _reset_scrollbars();

		/// Scrolls the viewport of the \ref contents_region.
		void _on_mouse_scroll(ui::mouse_scroll_info&) override;
		/// Forwards the key down event to the \ref contents_region.
		void _on_key_down(ui::key_info&) override;
		/// Forwards the key up event to the \ref contents_region.
		void _on_key_up(ui::key_info&) override;
		/// Forwards the text event to the \ref contents_region.
		void _on_keyboard_text(ui::text_info&) override;

		/// Calls \ref contents_region::_on_codebox_got_focus.
		void _on_got_focus() override;
		/// Calls \ref contents_region::_on_codebox_lost_focus.
		void _on_lost_focus() override;

		/// Initializes \ref _vscroll and \ref _contents.
		void _initialize(str_view_t, const ui::element_metrics&) override;
		/// Removes and disposes all components, and unregisters from \ref contents_region::editing_visual_changed.
		/// Components are removed in advance in order to avoid problems that may occur when
		/// \ref ui::element::_on_removing_from_parent is called.
		void _dispose() override;
	};

	/// Utility functions for elements designed to work as components of \ref editor.
	namespace component_helper {
		/// Returns the \ref editor that the given \ref ui::element is in.
		inline editor *get_box(const ui::element &elem) {
			return dynamic_cast<editor*>(elem.logical_parent());
		}
		/// Returns the \ref contents_region that's in the same \ref editor as the given component.
		contents_region *get_contents_region(const ui::element&);
		/// Returns both the \ref editor and the \ref contents_region. If the returned \ref contents_region is not \p nullptr, then
		/// the returned \ref editor won't be \p nullptr.
		std::pair<editor*, contents_region*> get_core_components(const ui::element&);
	}
}
