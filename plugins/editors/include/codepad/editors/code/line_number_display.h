// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// An editor component that displays line numbers.

#include "contents_region.h"

namespace codepad::editors::code {
	/// Displays a the line number for each line.
	class line_number_display : public ui::element {
	public:
		/// Returns the width of the longest line number.
		ui::size_allocation get_desired_width() const override;

		/// Returns \ref _theme.
		[[nodiscard]] const text_theme_specification &get_text_theme() const {
			return _theme;
		}
		/// Sets \ref _theme and invokes \ref _update_font() and \ref _on_font_changed().
		void set_text_theme(const text_theme_specification &spec) {
			_theme = spec;
			_update_font();
			_on_font_changed();
		}

		/// Returns the default class of elements of type \ref line_number_display.
		inline static std::u8string_view get_default_class() {
			return u8"line_number_display";
		}
	protected:
		std::shared_ptr<ui::font> _font; ///< The font used to render all line numbers.
		text_theme_specification _theme; ///< Font color and style for the line numbers.
		bool _events_registered = false; ///< Indicates whether the events has been registered.

		/// Registers events if a \ref contents_region can be found.
		void _register_handlers();
		/// Invokes \ref _register_handlers().
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
			_register_handlers();
		}
		/// Calls \ref _register_handlers(), and \ref _update_font() if necessary.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			_register_handlers();
			if (!_font) {
				_update_font();
			}
		}

		/// Handles the \p text_theme attribute.
		ui::property_info _find_property_path(const ui::property_path::component_list&) const override;

		/// Updates \ref _font when \ref _theme or the font family has changed.
		void _update_font() {
			if (auto *contents = component_helper::get_contents_region(*this)) {
				_update_font(*contents);
			}
		}
		/// Updates \ref _font using the given \ref contents_region.
		void _update_font(contents_region &contents) {
			_font = contents.get_font_families()[0]->get_matching_font(
				get_text_theme().style, get_text_theme().weight, ui::font_stretch::normal
			);
		}
		/// This should be called when the font family or \ref _theme has been changed.
		void _on_font_changed() {
			_font = nullptr;
			_update_font();
			_on_desired_size_changed(true, false);
			invalidate_visual();
		}

		/// Renders all visible line numbers.
		void _custom_render() const override;
	};
}
