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
		/// Returns the desired with.
		ui::size_allocation get_desired_width() const override;

		/// Returns the default class of elements of type \ref primary_offset_display.
		inline static std::u8string_view get_default_class() {
			return u8"primary_offset_display";
		}
	protected:
		bool _events_registered = false; ///< Marks if event handlers have been registered.

		/// Registers events if a \ref contents_region can be found.
		void _register_handlers();
		/// Calls \ref _register_handlers().
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
			_register_handlers();
		}
		/// Calls \ref _register_handlers() if necessary.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			_register_handlers();
		}

		/// Returns the label length that corresponds to the given buffer size.
		inline static std::size_t _get_label_length(std::size_t len) {
			return 1 + high_bit_index(std::max<std::size_t>(len, 1)) / 4;
		}

		/// Returns the hexadecimal representation of the given number, padded to the given length with zeros.
		static std::u8string _to_hex(std::size_t v, std::size_t len);
		/// Renders the offsets.
		void _custom_render() const override;
	};
}
