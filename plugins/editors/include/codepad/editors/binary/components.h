// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional built-in components of the binary editor.

#include "contents_region.h"

namespace codepad::editors::binary {
	/// Used to display the offset for each line in the binary editor.
	class primary_offset_display : public ui::element {
	public:
		/// Returns the role for \ref _contents_region.
		inline static std::u8string_view get_contents_region_role() {
			return u8"contents_region";
		}
		/// Returns the default class of elements of type \ref primary_offset_display.
		inline static std::u8string_view get_default_class() {
			return u8"primary_offset_display";
		}
	protected:
		contents_region *_contents_region = nullptr; ///< The \ref contents_region associated with this display.

		/// Returns the label length that corresponds to the given buffer size.
		inline static std::size_t _get_label_length(std::size_t len) {
			return 1 + high_bit_index(std::max<std::size_t>(len, 1)) / 4;
		}
		/// Returns the hexadecimal representation of the given number, padded to the given length with zeros.
		static std::u8string _to_hex(std::size_t v, std::size_t len);

		/// Computes the maximum width of all offsets.
		vec2d _compute_desired_size_impl(vec2d available) const override {
			std::size_t chars = _get_label_length(_contents_region->get_buffer().length());
			double maxw = _contents_region->get_font()->get_maximum_character_width_em(
				reinterpret_cast<const codepoint*>(U"0123456789ABCDEF")
			) * _contents_region->get_font_size();
			return vec2d(get_padding().width() + static_cast<double>(chars) * maxw, available.y);
		}

		/// Renders the offsets.
		void _custom_render() const override;

		/// Handles \ref _contents_region and registers for events.
		bool _handle_reference(std::u8string_view, element*) override;
	};
}
