// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structs and classes used to store carets.

#include <map>
#include <algorithm>

#include "../../core/misc.h"

namespace codepad::editor::code {
	class interpretation;

	/// A caret and the associated selected region. The first element is the position of the caret,
	/// and the second indicates the other end of the selcted region.
	using caret_selection = std::pair<size_t, size_t>;

	/// The data associated with a \ref caret_selection.
	struct caret_data {
		/// Default constructor.
		caret_data() = default;
		/// Constructor that initializes all fields of this struct.
		caret_data(double align, bool sbnext) : alignment(align), softbreak_next_line(sbnext) {
		}

		double alignment = 0.0; ///< The alignment of the caret when it moves vertically.
		/// Only used when the caret is positioned at a soft linebreak, to determine which line it's on.
		/// \p false if it's on the former line, and \p true if it's on the latter.
		bool softbreak_next_line = false;

		size_t
			bytepos_first = 0, ///< The position, in bytes, of the first element of a \ref caret_selection.
			bytepos_second = 0; ///< The position, in bytes, of the second element of a \ref caret_selection.
	};
	/// Stores a set of carets.
	struct caret_set {
		/// The container used to store carets.
		using container = std::map<caret_selection, caret_data>;
		/// An entry in the container, stores a caret and its associated data.
		using entry = std::pair<caret_selection, caret_data>;
		/// Iterator over the carets.
		using iterator = container::iterator;
		/// Const iterator over the carets.
		using const_iterator = container::const_iterator;

		container carets; ///< The carets.
		/// Indicates whether \ref caret_data::bytepos_first and \ref caret_data::bytepos_second have been
		/// calculated.
		bool bytepos_valid = false;

		/// Calculates \ref caret_data::bytepos_first and \ref caret_data::bytepos_second with the given
		/// \ref interpretation if necessary.
		void calculate_byte_positions(const interpretation&);

		/// Calls \ref add_caret to add a caret to this set. Note that this operation may invalidate
		/// \ref caret_data::bytepos_first and \ref caret_data::bytepos_second.
		///
		/// \param p The new caret to be added. The caret may be merged with overlapping carets.
		/// \param merged Indicates whether any merging has taken place.
		/// \return An iterator to the added entry.
		iterator add(entry p, bool &merged) {
			auto it = add_caret(carets, p, merged);
			bytepos_valid = false;
			return it;
		}
		/// \overload
		iterator add(entry p) {
			bool dummy;
			return add(p, dummy);
		}

		/// Resets the contents of this caret set, leaving only one caret at the beginning of the buffer.
		void reset() {
			carets.clear();
			carets.insert(entry(caret_selection(0, 0), caret_data()));
			bytepos_valid = false;
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
	};
}
