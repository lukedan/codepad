// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Labels.

#include "../element.h"

namespace codepad::ui {
	/// A label that displays plain text. Non-focusable by default.
	class label : public element {
	public:
		/// Returns the combined width of the text and the padding in pixels.
		size_allocation get_desired_width() const override {
			_check_cache_format();
			return size_allocation(_cached_fmt->get_layout().width(), true);
		}
		/// Returns the combined height of the text and the padding in pixels.
		size_allocation get_desired_height() const override {
			_check_cache_format();
			return size_allocation(_cached_fmt->get_layout().height(), true);
		}

		/// Returns the text.
		const std::u8string &get_text() const {
			return _text;
		}
		/// Sets the text.
		void set_text(std::u8string t) {
			_text = std::move(t);
			_on_text_layout_changed();
		}

		/// Returns the color of the text.
		colord get_text_color() const {
			return _text_color;
		}
		/// Sets the color of the text.
		void set_text_color(colord c) {
			_text_color = c;
			_on_text_color_changed();
		}

		/// Returns the font parameters.
		const font_parameters get_font_parameters() const {
			return _font;
		}
		/// Sets the font parameters.
		void set_font_parameters(font_parameters params) {
			_font = std::move(params);
			_on_text_layout_changed();
		}

		/// Returns the list of properties.
		const property_mapping &get_properties() const override;

		/// Adds the \p text_color property.
		static const property_mapping &get_properties_static();
		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"label";
		}
	protected:
		std::u8string _text; ///< The text.
		colord _text_color; ///< The color of the text.
		font_parameters _font; ///< The font.
		mutable std::unique_ptr<formatted_text> _cached_fmt; ///< The cached formatted text.

		/// Calls \ref _check_cache_format().
		void _on_prerender() override {
			element::_on_prerender();
			_check_cache_format();
		}
		/// Renders the text.
		void _custom_render() const override;

		/// Ensures that \ref _cached_fmt is valid.
		void _check_cache_format() const;
		/// Called when the color of the text has changed.
		virtual void _on_text_color_changed() {
			if (_cached_fmt) {
				_cached_fmt->set_text_color(_text_color, 0, std::numeric_limits<std::size_t>::max());
			}
			invalidate_visual();
		}
		/// Called when the layout of the text has potentially changed.
		virtual void _on_text_layout_changed() {
			_cached_fmt.reset();
			_on_desired_size_changed(true, true);
		}
	};
}
