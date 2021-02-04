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
			return size_allocation::pixels(_cached_fmt->get_layout().width() + get_padding().width());
		}
		/// Returns the combined height of the text and the padding in pixels.
		size_allocation get_desired_height() const override {
			_check_cache_format();
			return size_allocation::pixels(_cached_fmt->get_layout().height() + get_padding().height());
		}

		/// Returns the text.
		const std::u8string &get_text() const {
			return _text;
		}
		/// Sets the text.
		void set_text(std::u8string t) {
			_text = std::move(t);
			_on_text_changed();
		}
		/// Returns \ref _cached_fmt.
		const formatted_text &get_formatted_text() const {
			_check_cache_format();
			return *_cached_fmt;
		}

		/// Returns the color of the text.
		[[nodiscard]] colord get_text_color() const {
			return _text_color;
		}
		/// Sets the color of the text.
		void set_text_color(colord c) {
			_text_color = c;
			_on_text_color_changed();
		}

		/// Returns the font parameters.
		[[nodiscard]] const font_parameters get_font_parameters() const {
			return _font;
		}
		/// Sets the font parameters.
		void set_font_parameters(font_parameters params) {
			_font = std::move(params);
			_on_text_layout_changed();
		}

		/// Returns \ref _halign.
		[[nodiscard]] horizontal_text_alignment get_horizontal_alignment() const {
			return _halign;
		}
		/// Sets \ref _halign and invokes \ref _on_text_layout_changed().
		void set_horizontal_alignment(horizontal_text_alignment align) {
			_halign = align;
			_on_text_layout_changed();
		}

		/// Returns the wrapping mode currently applied to this \ref label.
		[[nodiscard]] wrapping_mode get_wrapping_mode() const {
			return _wrap;
		}
		/// Sets the wrapping mode and invokes \ref _on_text_layout_changed().
		void set_wrapping_mode(wrapping_mode w) {
			if (_wrap != w) {
				_wrap = w;
				_on_text_layout_changed();
			}
		}

		/// Returns \ref _valign.
		[[nodiscard]] vertical_text_alignment get_vertical_alignment() const {
			return _valign;
		}
		/// Sets \ref _valign and invokes \ref _on_text_layout_changed().
		void set_vertical_alignment(vertical_text_alignment align) {
			_valign = align;
			_on_text_layout_changed();
		}

		/// Returns the list of properties.
		const property_mapping &get_properties() const override;

		/// Adds the \p text_color, \p font, \p horizontal_alignment, \p vertical_alignment, and \p text properties.
		static const property_mapping &get_properties_static();
		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"label";
		}
	protected:
		std::u8string _text; ///< The text.
		colord _text_color; ///< The color of the text.
		font_parameters _font; ///< The font.
		mutable std::shared_ptr<formatted_text> _cached_fmt; ///< The cached formatted text.
		horizontal_text_alignment _halign = horizontal_text_alignment::front; ///< Horizontal text alignment.
		vertical_text_alignment _valign = vertical_text_alignment::top; ///< Vertical text alignment.
		wrapping_mode _wrap = wrapping_mode::none; ///< Wrapping mode for this label.

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
			invalidate_visual();
		}
		/// Called when the text has been changed. Invokes \ref _on_text_layout_changed().
		virtual void _on_text_changed() {
			_on_text_layout_changed();
		}

		/// Updates the layout size of the cached \ref formatted_text.
		void _on_layout_changed() override {
			element::_on_layout_changed();
			if (_cached_fmt) {
				_cached_fmt->set_layout_size(get_client_region().size());
			}
		}
	};
}
