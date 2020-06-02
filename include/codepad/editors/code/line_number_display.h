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

		/// Returns the default class of elements of type \ref line_number_display.
		inline static std::u8string_view get_default_class() {
			return u8"line_number_display";
		}
	protected:
		bool _events_registered = false; ///< Indicates whether the events has been registered.

		/// Registers events if a \ref contents_region can be found.
		void _register_handlers();
		/// Registers for \ref contents_region::content_modified.
		void _on_added_to_parent() override {
			element::_on_added_to_parent();
			_register_handlers();
		}
		/// Calls \ref _register_handlers() if necessary.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			_register_handlers();
		}

		/// Renders all visible line numbers.
		void _custom_render() const override;
	};
}
