// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic editor used to encapsulate different types of content regions.

#include <algorithm>

#include "../ui/panel.h"
#include "../ui/common_elements.h"
#include "caret_set.h"

namespace codepad::editors {
	/// The base class of content regions.
	class contents_region_base : public ui::element {
	public:
		/// Returns the amount of space to scroll for a `tick'.
		virtual double get_horizontal_scroll_delta() const = 0;
		/// Returns the amount of space to scroll for a `tick'.
		virtual double get_vertical_scroll_delta() const = 0;
		/// Returns the horizontal viewport range.
		virtual double get_horizontal_scroll_range() const = 0;
		/// Returns the vertical viewport range.
		virtual double get_vertical_scroll_range() const = 0;

		/// Returns the \ref caret_position that the given position points to. The position is relative to the top
		/// left corner of the \ref ui::element's layout.
		virtual caret_position hit_test_for_caret(vec2d) const = 0;

		/// Adds the given caret to the contents region.
		virtual void add_caret(caret_selection_position) = 0;
		/// Clears all carets from the contents region.
		virtual void clear_carets() = 0;

		/// Called when text is being typed into this contents region.
		virtual void on_text_input(str_view_t) = 0;

		/// Invoked when the visual of the contents has changed, e.g., when it is modified, when the document that's
		/// being edited is changed, or when the font has been changed, etc.
		info_event<> content_visual_changed;
	};

	/// The editor region that contains a contents region and other elements.
	///
	/// \todo Currently many components, including contents regions, are not designed to be able to be moved between
	///       different editors, mainly because they do not unbind from events they listen to when they're disposed
	///       of. If no such uses are desired then users must take caution, otherwise this needs to be fixed.
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
		/// A combination of \ref set_horizontal_position() and \ref set_vertical_position().
		void set_position(vec2d pos) {
			set_horizontal_position(pos.x);
			set_vertical_position(pos.y);
		}

		/// Returns the associated \ref contents_region.
		contents_region_base *get_contents_region() const {
			return _contents;
		}

		info_event<>
			/// Event invoked when the vertical position or viewport size has changed.
			vertical_viewport_changed,
			/// Event invoked when the horizontal position or viewport size has changed.
			horizontal_viewport_changed;

		/// Returns the \ref editor that's the logical parent of the given \ref ui::element.
		inline static editor *get_encapsulating(const ui::element &e) {
			return dynamic_cast<editor*>(e.logical_parent());
		}

		/// Returns the default class of all elements of type \ref editor.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("editor");
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
	protected:
		ui::scrollbar
			*_vert_scroll = nullptr, ///< The vertical \ref ui::scrollbar.
			*_hori_scroll = nullptr; ///< The horizontal \ref ui::scrollbar.
		contents_region_base *_contents = nullptr; ///< The associated \ref contents_region.

		/// Resets the parameters of \ref _vert_scroll and \ref _hori_scroll.
		void _reset_scrollbars() {
			_vert_scroll->set_params(_contents->get_vertical_scroll_range(), _contents->get_layout().height());
			double wrange = _contents->get_layout().width();
			_hori_scroll->set_params(std::max(_contents->get_horizontal_scroll_range(), wrange), wrange);
		}

		/// Scrolls the viewport of the \ref editor.
		void _on_mouse_scroll(ui::mouse_scroll_info &info) override {
			_vert_scroll->set_value(
				_vert_scroll->get_value() - _contents->get_vertical_scroll_delta() * info.delta.y
			);
			_hori_scroll->set_value(
				_hori_scroll->get_value() + _contents->get_horizontal_scroll_delta() * info.delta.x
			);
			info.mark_handled();
		}
		/// Invokes \ref content_region_base::on_text_input().
		void _on_keyboard_text(ui::text_info &info) override {
			_contents->on_text_input(info.content);
		}

		/// Initializes \ref hori_scroll, \ref _vert_scroll and \ref _contents.
		void _initialize(str_view_t cls, const ui::element_configuration &config) override {
			panel_base::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_vertical_scrollbar_role(), _role_cast(_vert_scroll)},
				{get_horizontal_scrollbar_role(), _role_cast(_hori_scroll)},
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
			_contents->content_visual_changed += [this]() {
				_reset_scrollbars();
			};
		}
	};
}
