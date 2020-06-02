// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets for \ref codepad::editors::code::contents_region.

#include "../caret_set.h"

namespace codepad::editors::code {
	class interpretation;

	/// Contains information about the position of a caret with word wrapping enabled.
	///
	/// \remark Although equality and ordering operators are defined for this struct, it is worth noting that
	///         they may be inaccurate when the \ref position "positions" of both oprands are the same and the
	///         \ref position is not at the same position as a soft linebreak.
	struct caret_position {
		/// Default constructor.
		caret_position() = default;
		/// Initializes all fields of this struct.
		explicit caret_position(std::size_t pos, bool back = false) : position(pos), at_back(back) {
		}

		/// Equality. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator==(caret_position lhs, caret_position rhs) {
			return lhs.position == rhs.position && lhs.at_back == rhs.at_back;
		}
		/// Inquality. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator!=(caret_position lhs, caret_position rhs) {
			return !(lhs == rhs);
		}

		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator<(caret_position lhs, caret_position rhs) {
			return lhs.position == rhs.position ? (!lhs.at_back && rhs.at_back) : lhs.position < rhs.position;
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator>(caret_position lhs, caret_position rhs) {
			return rhs < lhs;
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator<=(caret_position lhs, caret_position rhs) {
			return !(rhs < lhs);
		}
		/// Comparison. This may be inaccurate when both \ref position "positions" are the same.
		friend bool operator>=(caret_position lhs, caret_position rhs) {
			return !(lhs < rhs);
		}

		std::size_t position = 0; ///< The index of the unit that this caret is immediately before.
		/// Indicates whether the caret should be considered as being before the character after it, rather than
		/// being after the character before it. For example, if this caret is at the same position as a soft
		/// linebreak, this field determines whether it appears at the end of the first line, or at the beginning
		/// of the second line.
		bool at_back = false;
	};
	/// Contains information about a \ref caret_selection and relative position info.
	struct caret_selection_position {
		/// Default constructor.
		caret_selection_position() = default;
		/// Converting constructor from a \ref caret_position.
		caret_selection_position(caret_position cpos) :
			caret(cpos.position), selection(cpos.position), caret_at_back(cpos.at_back) {
		}
		/// Initializes this struct with a \ref caret_position and the position of the other end of the selection.
		caret_selection_position(caret_position caret, std::size_t selection) :
			caret_selection_position(caret.position, selection, caret.at_back) {
		}
		/// Initializes all fields of this struct.
		caret_selection_position(std::size_t c, std::size_t s, bool back = false) :
			caret(c), selection(s), caret_at_back(back) {
		}

		/// Sets the value of the part of this struct that corresponds to a \ref caret_position.
		void set_caret_position(caret_position pos) {
			caret = pos.position;
			caret_at_back = pos.at_back;
		}
		/// Returns the value of the part of this struct that corresponds to a \ref caret_position.
		caret_position get_caret_position() const {
			return caret_position(caret, caret_at_back);
		}

		std::size_t
			caret = 0, ///< The position of the caret.
			selection = 0; ///< The position of the non-caret end of the selection.
		bool caret_at_back = false; ///< \sa caret_position::at_back
	};

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

	/// Stores carets for a \ref contents_region.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	protected:
		using _base = caret_set_base<caret_data, caret_set>; ///< The base class.
		friend _base;
	public:
		using position = caret_position; ///< Position with additional data.
		using selection = caret_selection_position; ///< Selection with additional data.

		/// Indicates whether \ref caret_data::bytepos_first and \ref caret_data::bytepos_second have been
		/// calculated.
		bool bytepos_valid = false;

		/// Calculates \ref caret_data::bytepos_first and \ref caret_data::bytepos_second with the given
		/// \ref interpretation if necessary.
		void calculate_byte_positions(const interpretation&);

		/// Returns whether the given position is in a selection. This is simply a wrapper of the variant that takes
		/// a \p std::size_t, and \ref position::at_back is discarded.
		template <typename Cmp = std::less_equal<>> bool is_in_selection(position pos) const {
			return _base::is_in_selection<Cmp>(pos.position);
		}
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
