// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Labels.

#include "codepad/ui/element.h"
#include "codepad/ui/manager.h"

namespace codepad::ui {
	/// A label that displays plain text. Non-focusable by default.
	class label : public element {
	public:
		/// Controls how wrapping width is computed for a \ref label.
		enum class wrapping_width_mode : unsigned char {
			client, ///< Wrapping width is the same as the width of the client area.
			custom ///< Wrapping width is a fixed value.
		};

		/// Returns the combined width of the text and the padding in pixels.
		size_allocation get_desired_width() const override {
			return size_allocation::pixels(_formatted_text->get_layout().width() + get_padding().width());
		}
		/// Returns the combined height of the text and the padding in pixels.
		size_allocation get_desired_height() const override {
			return size_allocation::pixels(_formatted_text->get_layout().height() + get_padding().height());
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
		/// Returns \ref _formatted_text.
		const formatted_text &get_formatted_text() const {
			return *_formatted_text;
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

		/// Returns the wrapping mode currently applied to this \ref label.
		[[nodiscard]] wrapping_mode get_wrapping_mode() const {
			return _formatted_text->get_wrapping_mode();
		}
		/// Sets the wrapping mode and invokes \ref _on_text_layout_changed().
		void set_wrapping_mode(wrapping_mode w) {
			_formatted_text->set_wrapping_mode(w);
			_on_text_layout_changed();
		}

		/// Returns \ref _wrap_width_mode.
		[[nodiscard]] wrapping_width_mode get_wrapping_width_mode() const {
			return _wrap_width_mode;
		}
		/// Sets \ref _wrap_width_mode, and invokes \ref _on_text_layout_changed() if necessary.
		void set_wrapping_width_mode(wrapping_width_mode mode) {
			if (_wrap_width_mode != mode) {
				_wrap_width_mode = mode;
				_update_text_layout_size();
				_on_text_layout_changed();
			}
		}

		/// Returns the custom wrapping width used by this \ref label.
		[[nodiscard]] double get_custom_wrapping_width() const {
			return _wrap_width;
		}
		/// Sets custom wrapping width and, if, calls \ref _on_text_layout_changed().
		void set_custom_wrapping_width(double w) {
			_wrap_width = w;
			if (get_wrapping_width_mode() == wrapping_width_mode::custom) {
				_update_text_layout_size();
				_on_text_layout_changed();
			}
		}

		/// Returns \ref _halign.
		[[nodiscard]] horizontal_text_alignment get_horizontal_alignment() const {
			return _formatted_text->get_horizontal_alignment();
		}
		/// Sets \ref _halign and invokes \ref _on_text_layout_changed().
		void set_horizontal_alignment(horizontal_text_alignment align) {
			_formatted_text->set_horizontal_alignment(align);
			_on_text_layout_changed();
		}
		/// Returns \ref _valign.
		[[nodiscard]] vertical_text_alignment get_vertical_alignment() const {
			return _formatted_text->get_vertical_alignment();
		}
		/// Sets \ref _valign and invokes \ref _on_text_layout_changed().
		void set_vertical_alignment(vertical_text_alignment align) {
			_formatted_text->set_vertical_alignment(align);
			_on_text_layout_changed();
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"label";
		}
	protected:
		std::u8string _text; ///< The text.
		colord _text_color; ///< The color of the text.
		font_parameters _font; ///< The font.
		std::shared_ptr<formatted_text> _formatted_text; ///< The formatted text.
		/// Controls how wrapping width is computed.
		wrapping_width_mode _wrap_width_mode = wrapping_width_mode::client;
		/// Wrapping width. Has no effect if \ref _wrap_width_mode is \ref wrapping_width_mode::client.
		double _wrap_width = 0.0;
		/// Caches the client size of this element after the previous layout operation. If the size of the client
		/// changes, we need to call \ref _on_text_layout_changed().
		vec2d _prev_client_size;

		/// Renders the text.
		void _custom_render() const override;

		/// Updates the layout size of \ref _formatted_text.
		void _update_text_layout_size() {
			vec2d size = get_client_region().size();
			if (get_wrapping_width_mode() == wrapping_width_mode::custom) {
				size.x = get_custom_wrapping_width();
			}
			_formatted_text->set_layout_size(size);
		}

		/// Called when the color of the text has changed.
		virtual void _on_text_color_changed() {
			_formatted_text->set_text_color(_text_color, 0, std::numeric_limits<std::size_t>::max());
			invalidate_visual();
		}
		/// Called when the layout (size) of the text has potentially changed. Invokes \ref _on_desired_size_changed() and
		/// \ref invalidate_visual().
		virtual void _on_text_layout_changed() {
			_on_desired_size_changed(true, true);
			invalidate_visual();
		}
		/// Called when the text has been changed. Re-creates \ref _formatted_text, then invokes
		/// \ref _on_text_layout_changed(). Derived classes should always call this before executing other logic.
		virtual void _on_text_changed() {
			// re-create the formatted_text object
			vec2d layout_size = _formatted_text->get_layout_size();
			wrapping_mode wrap = _formatted_text->get_wrapping_mode();
			horizontal_text_alignment halign = _formatted_text->get_horizontal_alignment();
			vertical_text_alignment valign = _formatted_text->get_vertical_alignment();
			_formatted_text.reset();
			_formatted_text = get_manager().get_renderer().create_formatted_text(
				_text, _font, _text_color, layout_size, wrap, halign, valign
			);

			_on_text_layout_changed();
		}

		/// Updates the layout size of the cached \ref formatted_text.
		void _on_layout_changed() override {
			element::_on_layout_changed();

			_update_text_layout_size();
			// if the client size changes, call _on_text_layout_changed() since text layout depends on the client
			// size of this element
			// we're comparing doubles here, but since the layout process and floating point arithmetic are
			// deterministic, this should at most result in a few false positives (false size changes)
			vec2d client_size = get_client_region().size();
			if (client_size.x != _prev_client_size.x || client_size.y != _prev_client_size.y) {
				_on_text_layout_changed();
				_prev_client_size = client_size;
			}
		}

		/// Handles the \p text_color, \p font, \p horizontal_alignment, \p vertical_alignment, \p text, \p wrapping,
		/// \p wrapping_width_mode, and \p wrapping_width properties.
		property_info _find_property_path(const property_path::component_list&) const override;

		/// Creates an empty \ref formatted_text.
		void _initialize() override {
			element::_initialize();

			_formatted_text = get_manager().get_renderer().create_formatted_text(
				u8"", _font, _text_color, vec2d(),
				wrapping_mode::none, horizontal_text_alignment::front, vertical_text_alignment::top
			);
		}
	};
}
namespace codepad {
	/// Parser for \ref ui::label::wrapping_width_mode.
	template <> struct enum_parser<ui::label::wrapping_width_mode> {
		/// The parser interface.
		static std::optional<ui::label::wrapping_width_mode> parse(std::u8string_view);
	};
}
