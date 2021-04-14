// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets for \ref codepad::editors::code::contents_region.

#include "codepad/editors/caret_set.h"

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
		caret_position(std::size_t pos, bool back) : position(pos), after_stall(back) {
		}

		/// Comparison.
		friend std::strong_ordering operator<=>(caret_position lhs, caret_position rhs) {
			if (lhs.position == rhs.position) {
				if (lhs.after_stall == rhs.after_stall) {
					return std::strong_ordering::equal;
				}
				return rhs.after_stall ? std::strong_ordering::less : std::strong_ordering::greater;
			}
			return lhs.position <=> rhs.position;
		}
		/// Equality.
		friend bool operator==(caret_position, caret_position) = default;

		std::size_t position = 0; ///< The index of the unit that this caret is immediately before.
		/// Indicates whether the caret should be considered as being before the character after it, rather than
		/// being after the character before it. For example, if this caret is at the same position as a soft
		/// linebreak, this field determines whether it appears at the end of the first line, or at the beginning
		/// of the second line.
		bool after_stall = false;
	};
	/// Contains information about a \ref ui::caret_selection and more specific position info about the caret.
	struct caret_selection_position {
		/// Default constructor.
		caret_selection_position() = default;
		/// Converting constructor from a \ref caret_position.
		explicit caret_selection_position(caret_position cpos) :
			caret(ui::caret_selection(cpos.position)), caret_after_stall(cpos.after_stall) {
		}
		/// Initializes all fields of this struct.
		caret_selection_position(ui::caret_selection c, bool back) :caret(c), caret_after_stall(back) {
		}

		/// Extracts the part of this object that corresponds to a \ref ui::caret_selection, a.k.a. \ref caret.
		ui::caret_selection get_caret_selection() const {
			return caret;
		}

		/// Sets the value of the part of this struct that corresponds to a \ref caret_position.
		void set_caret_position(caret_position pos) {
			caret.move_caret(pos.position);
			caret_after_stall = pos.after_stall;
		}
		/// Returns the value of the part of this struct that corresponds to a \ref caret_position.
		caret_position get_caret_position() const {
			return caret_position(caret.get_caret_position(), caret_after_stall);
		}

		ui::caret_selection caret; ///< The caret and the associated selection.
		bool caret_after_stall = false; ///< \sa caret_position::at_back
	};

	/// The data associated with a \ref ui::caret_selection.
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
	};

	/// Stores carets for a \ref contents_region.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	protected:
		using _base = caret_set_base<caret_data, caret_set>; ///< The base class.
		friend _base;
	public:
		using position = caret_position; ///< Position with additional data.
		using selection = caret_selection_position; ///< Selection with additional data.

		using caret_set_base::is_in_selection;
		/// Returns whether the given position is in a selection. This is simply a wrapper of the variant that takes
		/// a \p std::size_t, and \ref position::at_back is discarded.
		template <typename Cmp = std::less_equal<>> bool is_in_selection(position pos) const {
			return this->is_in_selection<Cmp>(pos.position);
		}

		/// Wrapper around \ref caret_selection_position::set_caret_position().
		inline static void set_caret_position(selection &s, position pos) {
			s.set_caret_position(pos);
		}
		/// Wrapper around \ref caret_selection_position::get_caret_position().
		[[nodiscard]] inline static position get_caret_position(selection s) {
			return s.get_caret_position();
		}

		/// Returns the \ref selection corresponding to the given \ref iterator_position.
		[[nodiscard]] inline static selection get_caret_selection(iterator_position it) {
			return selection(it.get_caret_selection(), it.get_iterator()->data.after_stall);
		}
	};
}
