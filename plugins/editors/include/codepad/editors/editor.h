// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic editor used to encapsulate different types of content regions.

#include <algorithm>

#include <codepad/core/settings.h>
#include <codepad/ui/panel.h>
#include <codepad/ui/manager.h>
#include <codepad/ui/elements/scrollbar.h>

#include "buffer.h"
#include "caret_set.h"
#include "decoration.h"

namespace codepad::editors {
	/// The base class of content regions.
	class contents_region_base : public ui::element {
	public:
		/// Returns the amount of space to scroll for a `tick'.
		virtual double get_vertical_scroll_delta() const = 0;
		/// Returns the amount of space to scroll for a `tick'. By default this simply returns the result of
		/// \ref get_vertical_scroll_delta().
		virtual double get_horizontal_scroll_delta() const {
			return get_vertical_scroll_delta();
		}
		/// Returns the horizontal viewport range.
		virtual double get_horizontal_scroll_range() const = 0;
		/// Returns the vertical viewport range.
		virtual double get_vertical_scroll_range() const = 0;

		/// Called when text is being typed into this contents region.
		virtual void on_text_input(std::u8string_view) = 0;

		/// Returns whether the contents_region is currently in insert mode.
		bool is_insert_mode() const {
			return _insert;
		}
		/// Sets whether the contents_region is currently in insert mode.
		virtual void set_insert_mode(bool v) {
			if (v != _insert) {
				_insert = v;
				edit_mode_changed.invoke();
			}
		}
		/// Toggles insert mode.
		void toggle_insert_mode() {
			set_insert_mode(!is_insert_mode());
		}

		/// Returns the associated \ref buffer. This function returns a reference to a \p std::shared_ptr so that new
		/// views into the buffer can be opened.
		[[nodiscard]] virtual buffer &get_buffer() const = 0;

		/// Returns a reference to the \ref selection_renderer used for this contents region.
		std::shared_ptr<decoration_renderer> &code_selection_renderer() {
			return _selection_renderer;
		}
		/// \overload
		const std::shared_ptr<decoration_renderer> &code_selection_renderer() const {
			return _selection_renderer;
		}

		info_event<>
			/// Invoked when the visual of the contents has changed, e.g., when it is modified, when the document
			/// that's being edited is changed, or when the font has been changed, etc.
			content_visual_changed,
			/// Invoked when the input mode changes from insert to overwrite or the other way around.
			edit_mode_changed;
	protected:
		ui::visuals _caret_visuals; ///< The visuals of carets.
		std::shared_ptr<decoration_renderer> _selection_renderer; ///< The \ref decoration_renderer.
		bool _insert = true; ///< Indicates whether the contents_region is in `insert' mode.

		/// Handles the registration of \p mode_changed_insert and \p mode_changed_overwrite events.
		bool _register_edit_mode_changed_event(std::u8string_view, std::function<void()>&);
		/// Additionally calls \ref _register_edit_mode_changed_event().
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_register_edit_mode_changed_event(name, callback) ||
				element::_register_event(name, std::move(callback));
		}
		/// Handles the \p caret_visuals property.
		ui::property_info _find_property_path(const ui::property_path::component_list&) const override;
	};

	/// The editor region that contains a contents region and other elements.
	///
	/// \todo Currently many components, including contents regions, are not designed to be able to be moved between
	///       different editors, mainly because they do not unbind from events they listen to when they're disposed
	///       of. If no such uses are desired then users must take caution, otherwise this needs to be fixed.
	class editor : public ui::panel {
	public:
		/// Sets the target vertical position of this \ref editor.
		void set_target_vertical_position(double p) {
			_vert_scroll->set_target_value(p);
		}
		/// Changes the vertical position of this \ref editor immediately.
		void set_vertical_position_immediate(double p) {
			_vert_scroll->set_values_immediate(p);
		}
		/// Returns the vertical position of this \ref editor.
		double get_vertical_position() const {
			return _vert_scroll->get_actual_value();
		}

		/// Sets the horizontal position of this \ref editor.
		void set_target_horizontal_position(double p) {
			_hori_scroll->set_target_value(p);
		}
		/// Changes the horizontal position of this \ref editor immediately.
		void set_horizontal_position_immediate(double p) {
			_hori_scroll->set_values_immediate(p);
		}
		/// Returns the horizontal position of this \ref editor.
		double get_horizontal_position() const {
			return _hori_scroll->get_actual_value();
		}

		/// Returns the combined results of \ref get_vertical_position() and \ref get_horizontal_position().
		vec2d get_position() const {
			return vec2d(get_horizontal_position(), get_vertical_position());
		}
		/// A combination of \ref set_target_horizontal_position() and \ref set_target_vertical_position().
		void set_target_position(vec2d pos) {
			set_target_horizontal_position(pos.x);
			set_target_vertical_position(pos.y);
		}
		/// A combination of \ref set_horizontal_position_immediate() and \ref set_vertical_position_immediate().
		void set_position_immediate(vec2d pos) {
			set_horizontal_position_immediate(pos.x);
			set_vertical_position_immediate(pos.y);
		}

		/// Adjusts horizontal and vertical positions so that the given region is visible.
		void make_region_visible(rectd rgn) {
			_hori_scroll->make_range_visible(rgn.xmin, rgn.xmax);
			_vert_scroll->make_range_visible(rgn.ymin, rgn.ymax);
		}

		/// Returns the associated \ref contents_region_base.
		contents_region_base *get_contents_region() const {
			return _contents;
		}

		info_event<>
			/// Event invoked when the vertical position or viewport size has changed.
			vertical_viewport_changed,
			/// Event invoked when the horizontal position or viewport size has changed.
			horizontal_viewport_changed;

		/// Retrieves the setting entry that determines the font size.
		static settings::retriever_parser<double> &get_font_size_setting(settings&);
		/// Retrieves the setting entry that determines the font family.
		static settings::retriever_parser<std::u8string> &get_font_family_setting(settings&);
		/// Retrieves the setting entry that determines the list of interaction modes used in code editors.
		static settings::retriever_parser<std::vector<std::u8string>> &get_interaction_modes_setting(settings&);

		/// Returns the \ref editor that's the logical parent of the given \ref ui::element.
		inline static editor *get_encapsulating(const ui::element &e) {
			return dynamic_cast<editor*>(e.logical_parent());
		}

		/// Returns the default class of all elements of type \ref editor.
		inline static std::u8string_view get_default_class() {
			return u8"editor";
		}

		/// Returns the name identifier of the vertical scrollbar.
		inline static std::u8string_view get_vertical_scrollbar_name() {
			return u8"vertical_scrollbar";
		}
		/// Returns the name identifier of the horizontal scrollbar.
		inline static std::u8string_view get_horizontal_scrollbar_name() {
			return u8"horizontal_scrollbar";
		}
		/// Returns the name identifier of the contents_region.
		inline static std::u8string_view get_contents_region_name() {
			return u8"contents_region";
		}
	protected:
		ui::scrollbar
			*_vert_scroll = nullptr, ///< The vertical \ref ui::scrollbar.
			*_hori_scroll = nullptr; ///< The horizontal \ref ui::scrollbar.
		contents_region_base *_contents = nullptr; ///< The associated \ref contents_region_base.

		/// Resets the parameters of \ref _vert_scroll and \ref _hori_scroll.
		void _reset_scrollbars() {
			_vert_scroll->set_params(_contents->get_vertical_scroll_range(), _contents->get_layout().height());
			double wrange = _contents->get_layout().width();
			_hori_scroll->set_params(std::max(_contents->get_horizontal_scroll_range(), wrange), wrange);
		}

		/// Scrolls the viewport of the \ref editor.
		void _on_mouse_scroll(ui::mouse_scroll_info &info) override {
			_vert_scroll->handle_scroll_event(info, _contents->get_vertical_scroll_delta());
			_hori_scroll->handle_scroll_event(info, _contents->get_horizontal_scroll_delta());
		}
		/// Invokes \ref contents_region_base::on_text_input().
		void _on_keyboard_text(ui::text_info &info) override {
			_contents->on_text_input(info.content);
		}

		/// Adds \ref _vert_scroll, \ref _hori_scroll, and \ref _contents to the mapping.
		ui::class_arrangements::notify_mapping _get_child_notify_mapping() override {
			auto mapping = panel::_get_child_notify_mapping();
			mapping.emplace(get_vertical_scrollbar_name(), _name_cast(_vert_scroll));
			mapping.emplace(get_horizontal_scrollbar_name(), _name_cast(_hori_scroll));
			mapping.emplace(get_contents_region_name(), _name_cast(_contents));
			return mapping;
		}

		/// Initializes \ref _hori_scroll, \ref _vert_scroll and \ref _contents.
		void _initialize(std::u8string_view) override;
	};
}
