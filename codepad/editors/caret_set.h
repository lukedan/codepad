// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets.

#include <map>
#include <algorithm>

#include "../core/misc.h"

namespace codepad::editors {
	/// A caret and the associated selected region. The first element is the position of the caret,
	/// and the second indicates the other end of the selcted region.
	using caret_selection = std::pair<size_t, size_t>;

	/// Contains information about the position of a caret with word wrapping enabled.
	///
	/// \remark Although equality and ordering operators are defined for this struct, it is worth noting that
	///         they may be inaccurate when the \ref position "positions" of both oprands are the same and the
	///         \ref position is not at the same position as a soft linebreak.
	struct caret_position {
		/// Default constructor.
		caret_position() = default;
		/// Initializes all fields of this struct.
		explicit caret_position(size_t pos, bool back = false) : position(pos), at_back(back) {
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

		size_t position = 0; ///< The index of the unit that this caret is immediately before.
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
		/// Convertsion constructor from a \ref caret_position.
		caret_selection_position(caret_position cpos) :
			caret(cpos.position), selection(cpos.position), caret_at_back(cpos.at_back) {
		}
		/// Initializes all fields of this struct.
		caret_selection_position(size_t c, size_t s, bool back = false) : caret(c), selection(s), caret_at_back(back) {
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

		size_t
			caret = 0, ///< The position of the caret.
			selection = 0; ///< The position of the non-caret end of the selection.
		bool caret_at_back = false; ///< \sa caret_position::at_back
	};

	/// Template class of containers used to store a set of carets.
	///
	/// \tparam Data The data associated with a single caret.
	/// \tparam Derived The derived class, if any. Used for CRTP.
	template <typename Data, typename Derived> class caret_set_base {
	public:
		/// The container used to store carets.
		using container = std::map<caret_selection, Data>;
		/// An entry in the container, stores a caret and its associated data.
		using entry = std::pair<caret_selection, Data>;
		/// Iterator over the carets.
		using iterator = typename container::iterator;
		/// Const iterator over the carets.
		using const_iterator = typename container::const_iterator;
		/// The type of data associated with each caret.
		using data_type = Data;

		/// Default virtual destructor.
		virtual ~caret_set_base() = default;

		container carets; ///< The carets.

		/// Calls \ref add_caret to add a caret to this set. Note that this operation may invalidate
		/// \ref caret_data::bytepos_first and \ref caret_data::bytepos_second.
		///
		/// \param p The new caret to be added. The caret may be merged with overlapping carets.
		/// \param merged Indicates whether any merging has taken place.
		/// \return An iterator to the added entry.
		iterator add(entry p, bool &merged) {
			return Derived::_add_impl(static_cast<Derived&>(*this), p, merged);
		}
		/// \overload
		iterator add(entry p) {
			bool dummy;
			return add(p, dummy);
		}

		/// Resets the contents of this caret set, leaving only one caret at the beginning of the buffer.
		void reset() {
			Derived::_reset_impl(static_cast<Derived&>(*this));
		}

		/// Adds a caret to the given container, merging it with existing ones when necessary. Note that this
		/// operation may invalidate \ref caret_data::bytepos_first and \ref caret_data::bytepos_second.
		///
		/// \param cont The container.
		/// \param et The caret to be added to the container.
		/// \param merged Indicates whether the caret has been merged with existing ones.
		/// \return An iterator of the container to the inserted caret.
		inline static iterator add_caret(container &cont, entry et, bool &merged) {
			merged = false;
			auto minmaxv = std::minmax({et.first.first, et.first.second});
			auto beg = cont.lower_bound(caret_selection(minmaxv.first, minmaxv.first));
			if (beg != cont.begin()) {
				--beg;
			}
			while (beg != cont.end() && std::min(beg->first.first, beg->first.second) <= minmaxv.second) {
				if (try_merge_selection(
					et.first.first, et.first.second,
					beg->first.first, beg->first.second,
					et.first.first, et.first.second
				)) {
					beg = cont.erase(beg);
					merged = true;
				} else {
					++beg;
				}
			}
			return cont.insert(et).first;
		}
		/// \overload
		inline static iterator add_caret(container &cont, entry et) {
			bool dummy;
			return add_caret(cont, et, dummy);
		}

		/// Tests if the given position belongs to a selected region. Carets that have no selected regions
		/// are ignored.
		///
		/// \tparam Cmp Used to determine whether being at the boundaries of selected regions counts as being
		///             inside the region.
		template <typename Cmp = std::less_equal<>> bool is_in_selection(size_t cp) const {
			auto cur = carets.lower_bound(std::make_pair(cp, cp));
			if (cur != carets.begin()) {
				--cur;
			}
			Cmp cmp;
			while (cur != carets.end() && std::min(cur->first.first, cur->first.second) <= cp) {
				if (cur->first.first != cur->first.second) {
					auto minmaxv = std::minmax(cur->first.first, cur->first.second);
					if (cmp(minmaxv.first, cp) && cmp(cp, minmaxv.second)) {
						return true;
					}
				}
				++cur;
			}
			return false;
		}

		/// Tries to merge two carets together. The discrimination between `master' and `slave' carets is
		/// introduced to resolve conflicting caret placement relative to the selection.
		///
		/// \param mm The caret of the `master' \ref caret_selection.
		/// \param ms End of the selected region of the `master' \ref caret_selection.
		/// \param sm The caret of the `slave' \ref caret_selection.
		/// \param ss End of the selected region of the `slave' \ref caret_selection.
		/// \param rm The caret of the resulting \ref caret_selection.
		/// \param rs End of the selected region of the resulting \ref caret_selection.
		/// \return Whether the two carets should be merged.
		inline static bool try_merge_selection(size_t mm, size_t ms, size_t sm, size_t ss, size_t &rm, size_t &rs) {
			auto p1mmv = std::minmax(mm, ms), p2mmv = std::minmax(sm, ss);
			// carets without selections
			if (mm == ms && mm >= p2mmv.first && mm <= p2mmv.second) {
				rm = sm;
				rs = ss;
				return true;
			}
			if (sm == ss && sm >= p1mmv.first && sm <= p1mmv.second) {
				rm = mm;
				rs = ms;
				return true;
			}
			if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) { // no need to merge
				return false;
			}
			size_t gmin = std::min(p1mmv.first, p2mmv.first), gmax = std::max(p1mmv.second, p2mmv.second);
			assert_true_logical(!((mm == gmin && sm == gmax) || (mm == gmax && sm == gmin)), "caret layout shouldn't occur");
			if (mm < ms) {
				rm = gmin;
				rs = gmax;
			} else {
				rm = gmax;
				rs = gmin;
			}
			return true;
		}
	protected:
		/// Implementation of \ref add(). The derived class can declare a method with the same name to override this.
		inline static iterator _add_impl(caret_set_base &set, entry p, bool &merged) {
			return add_caret(set.carets, p, merged);
		}
		/// Implementation of \ref reset(). The derived class can declare a method with the same name to override
		/// this.
		inline static void _reset_impl(caret_set_base &set) {
			set.carets.clear();
			set.carets.insert(entry(caret_selection(0, 0), Data()));
		}
	};
}
