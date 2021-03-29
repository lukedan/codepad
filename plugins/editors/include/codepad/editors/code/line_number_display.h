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
		[[nodiscard]] const text_theme &get_text_theme() const {
			return _theme;
		}
		/// Sets \ref _theme and invokes \ref _update_font() and \ref _on_font_changed().
		void set_text_theme(const text_theme &spec) {
			_theme = spec;
			_update_font();
			_on_font_changed();
		}

		/// Returns the role for \ref _contents_region.
		inline static std::u8string_view get_contents_region_role() {
			return u8"contents_region";
		}
		/// Returns the default class of elements of type \ref line_number_display.
		inline static std::u8string_view get_default_class() {
			return u8"line_number_display";
		}
	protected:
		std::shared_ptr<ui::font> _font; ///< The font used to render all line numbers.
		text_theme _theme; ///< Font color and style for the line numbers.
		contents_region *_contents_region = nullptr; ///< The \ref contents_region associated with this display.

		/// Handles the \p text_theme attribute.
		ui::property_info _find_property_path(const ui::property_path::component_list&) const override;
		/// Handles \ref _contents_region, registers for events, and calls \ref _update_font().
		bool _handle_reference(std::u8string_view, element*) override;

		/// Updates \ref _font when \ref _theme or the font family has changed.
		void _update_font() {
			_font = _contents_region->get_font_families()[0]->get_matching_font(
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
