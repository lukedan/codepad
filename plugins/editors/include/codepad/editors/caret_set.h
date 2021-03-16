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
#include "codepad/ui/misc.h"

namespace codepad::editors {
	/// Template class of containers used to store a set of carets. Derived classes should define two additional
	/// types: \p position, which indicates the position of a caret, and \p selection, which indicates the full
	/// position of a caret and the associated selection, and must contain two \p std::size_t fields: \p caret and
	/// \p selection.
	///
	/// \tparam Data The data associated with a single caret.
	/// \tparam Derived The derived class, if any. Used for CRTP.
	template <typename Data, typename Derived> class caret_set_base {
	public:
		/// The container used to store carets.
		using container = std::map<ui::caret_selection, Data>;
		/// An entry in the container, stores a caret and its associated data.
		using entry = std::pair<ui::caret_selection, Data>;
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
			auto beg = cont.lower_bound(ui::caret_selection(range.first, range.first));
			if (beg != cont.begin()) {
				--beg;
			}
			while (beg != cont.end() && std::min(beg->first.caret, beg->first.selection) <= range.second) {
				if (auto merged_caret = try_merge_selection(et.first, beg->first)) {
					et.first = merged_caret.value();
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
		/// \tparam FrontCmp Used to determine whether being at the starting boundary of selected regions counts as
		///                  being inside the region.
		/// \tparam RearCmp Used to determine whether being at the finishing boundary of selected regions counts as
		///                 being inside the region.
		template <
			typename FrontCmp = std::less_equal<>, typename RearCmp = std::less_equal<>
		> bool is_in_selection(std::size_t cp) const {
			auto cur = carets.lower_bound(ui::caret_selection(cp, cp));
			if (cur != carets.begin()) {
				--cur;
			}
			FrontCmp front_cmp;
			RearCmp rear_cmp;
			while (cur != carets.end() && std::min(cur->first.caret, cur->first.selection) <= cp) {
				if (cur->first.caret != cur->first.selection) {
					auto range = cur->first.get_range();
					if (front_cmp(range.first, cp) && rear_cmp(cp, range.second)) {
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
		/// \return The merged \ref ui::caret_selection if the two carets need to be merged.
		inline static std::optional<ui::caret_selection> try_merge_selection(
			ui::caret_selection master, ui::caret_selection slave
		) {
			auto p1mmv = master.get_range(), p2mmv = slave.get_range();
			// carets without selections
			if (master.caret == master.selection && master.caret >= p2mmv.first && master.caret <= p2mmv.second) {
				return slave;
			}
			if (slave.caret == slave.selection && slave.caret >= p1mmv.first && slave.caret <= p1mmv.second) {
				return master;
			}
			if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) { // no need to merge
				return std::nullopt;
			}
			std::size_t gmin = std::min(p1mmv.first, p2mmv.first), gmax = std::max(p1mmv.second, p2mmv.second);
			assert_true_logical(
				!((master.caret == gmin && slave.caret == gmax) || (master.caret == gmax && slave.caret == gmin)),
				"invalid caret pair positioning"
			);
			if (master.caret < master.selection) {
				return ui::caret_selection(gmin, gmax);
			} else {
				return ui::caret_selection(gmax, gmin);
			}
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
			set.carets.emplace(); // insert caret at the front of the document
		}
	};
}
