// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets.

#include <map>
#include <algorithm>
#include <compare>

#include "codepad/core/misc.h"
#include "codepad/core/assert.h"

namespace codepad::editors {
	/// A caret and the associated selected region. The first element is the position of the caret,
	/// and the second indicates the other end of the selcted region.
	struct caret_selection {
	public:
		/// Default constructor.
		caret_selection() = default;
		/// Sets the caret position, and sets the selection to empty.
		explicit caret_selection(std::size_t pos) : caret(pos), selection(pos) {
		}
		/// Initializes all fields of this struct.
		caret_selection(std::size_t c, std::size_t s) : caret(c), selection(s) {
		}

		/// Returns the range covered by this selection. Basically calls \p std::minmax().
		std::pair<std::size_t, std::size_t> get_range() const {
			return std::minmax({ caret, selection });
		}

		/// Sets \ref caret. This is only to provide a interface for interaction modes.
		void set_caret_position(std::size_t pos) {
			caret = pos;
		}
		/// Returns \ref caret. This is only to provide a interface for interaction modes.
		std::size_t get_caret_position() const {
			return caret;
		}

		/// Default comparisons. \ref caret will always be compared first.
		friend std::strong_ordering operator<=>(const caret_selection&, const caret_selection&) = default;

		std::size_t
			caret = 0, ///< The caret.
			selection = 0; ///< The other end of the selection region.
	};

	/// Template class of containers used to store a set of carets.
	///
	/// \tparam Data The data associated with a single caret.
	/// \tparam Derived The derived class, if any. Used for CRTP.
	/// \todo Custom merge function for binary editor.
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

		/// Calls \ref _add_impl() to add a caret to this set.
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

		/// Adds a caret to the given container, merging it with existing ones when necessary.
		///
		/// \param cont The container.
		/// \param et The caret to be added to the container.
		/// \param merged Indicates whether the caret has been merged with existing ones.
		/// \return An iterator of the container to the inserted caret.
		inline static iterator add_caret_to(container &cont, entry et, bool &merged) {
			merged = false;
			auto range = et.first.get_range();
			auto beg = cont.lower_bound(caret_selection(range.first, range.first));
			if (beg != cont.begin()) {
				--beg;
			}
			while (beg != cont.end() && std::min(beg->first.caret, beg->first.selection) <= range.second) {
				if (try_merge_selection(
					et.first.caret, et.first.selection,
					beg->first.caret, beg->first.selection,
					et.first.caret, et.first.selection
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
		inline static iterator add_caret_to(container &cont, entry et) {
			bool dummy;
			return add_caret_to(cont, et, dummy);
		}

		/// Tests if the given position belongs to a selected region. Carets that have no selected regions
		/// are ignored.
		///
		/// \tparam Cmp Used to determine whether being at the boundaries of selected regions counts as being
		///             inside the region.
		template <typename Cmp = std::less_equal<>> bool is_in_selection(std::size_t cp) const {
			auto cur = carets.lower_bound(caret_selection(cp, cp));
			if (cur != carets.begin()) {
				--cur;
			}
			Cmp cmp;
			while (cur != carets.end() && std::min(cur->first.caret, cur->first.selection) <= cp) {
				if (cur->first.caret != cur->first.selection) {
					auto range = cur->first.get_range();
					if (cmp(range.first, cp) && cmp(cp, range.second)) {
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
		inline static bool try_merge_selection(
			std::size_t mm, std::size_t ms,
			std::size_t sm, std::size_t ss,
			std::size_t &rm, std::size_t &rs
		) {
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
			std::size_t gmin = std::min(p1mmv.first, p2mmv.first), gmax = std::max(p1mmv.second, p2mmv.second);
			assert_true_logical(
				!((mm == gmin && sm == gmax) || (mm == gmax && sm == gmin)), "caret layout shouldn't occur"
			);
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
			return add_caret_to(set.carets, p, merged);
		}
		/// Implementation of \ref reset(). The derived class can declare a method with the same name to override
		/// this.
		inline static void _reset_impl(caret_set_base &set) {
			set.carets.clear();
			set.carets.insert(entry(caret_selection(0, 0), Data()));
		}
	};
}
