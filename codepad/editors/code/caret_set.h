// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets for \ref codepad::editors::code::editor.

#include "../caret_set.h"

namespace codepad::editors::code {
	class interpretation;

	/// The data associated with a \ref caret_selection.
	struct caret_data {
		/// Default constructor.
		caret_data() = default;
		/// Constructor that initializes all fields of this struct.
		caret_data(double align, bool sbnext) : alignment(align), after_stall(sbnext) {
		}

		double alignment = 0.0; ///< The alignment of the caret when it moves vertically.
		/// Only used when the caret is positioned at a soft linebreak, to determine which line it's on.
		/// \p false if it's on the former line, and \p true if it's on the latter.
		bool after_stall = false;

		std::size_t
			bytepos_first = 0, ///< The position, in bytes, of the first element of a \ref caret_selection.
			bytepos_second = 0; ///< The position, in bytes, of the second element of a \ref caret_selection.
	};

	/// Stores carets for an \ref editor.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	protected:
		using _base = caret_set_base<caret_data, caret_set>; ///< The base class.
		friend _base;
	public:
		/// Indicates whether \ref caret_data::bytepos_first and \ref caret_data::bytepos_second have been
		/// calculated.
		bool bytepos_valid = false;

		/// Calculates \ref caret_data::bytepos_first and \ref caret_data::bytepos_second with the given
		/// \ref interpretation if necessary.
		void calculate_byte_positions(const interpretation&);
	protected:
		/// Sets \ref bytepos_valid to \p false before adding the caret.
		inline static iterator _add_impl(caret_set &set, entry p, bool &merged) {
			set.bytepos_valid = false;
			return _base::_add_impl(set, p, merged);
		}
		/// Sets \ref bytepos_valid to \p false before resetting.
		inline static void _reset_impl(caret_set &set) {
			set.bytepos_valid = false;
			_base::_reset_impl(set);
		}
	};
}
