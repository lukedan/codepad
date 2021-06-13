// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Common Unicode types and functions.

#include <cstddef>
#include <string>
#include <vector>
#include <algorithm>

namespace codepad {
	/// Type used to store codepoints. \p char32_t is not used because its range is 0~0x10FFFF, so it may not be able
	/// to correctly represent invalid codepoints.
	using codepoint = std::uint32_t;
	using codepoint_string = std::basic_string<codepoint>; ///< A string of codepoints.

	/// A range of codepoints.
	struct codepoint_range {
		/// Default constructor.
		codepoint_range() = default;
		/// Initializes a range that contains only the given codepoint.
		explicit codepoint_range(codepoint cp) : first(cp), last(cp) {
		}
		/// Initializes the entire range.
		codepoint_range(codepoint f, codepoint l) : first(f), last(l) {
		}

		codepoint first = 0; ///< The first codepoint in this range.
		codepoint last = 0; ///< The last codepoint in this range, inclusive.
	};

	///< A list of \ref codepoint_range objects.
	struct codepoint_range_list {
		std::vector<codepoint_range> ranges; ///< The ranges.

		/// Sorts the ranges and merges any ranges that are intersecting.
		void sort_and_compact() {
			std::sort(
				ranges.begin(), ranges.end(),
				[](const codepoint_range &lhs, const codepoint_range &rhs) {
					return lhs.first < rhs.first;
				}
			);
			if (!ranges.empty()) {
				auto prev = ranges.begin();
				for (auto it = ranges.begin() + 1; it != ranges.end(); ++it) {
					if (it->first <= prev->last + 1) {
						prev->last = it->last;
					} else {
						*++prev = *it;
					}
				}
				ranges.erase(prev + 1, ranges.end());
			}
		}

		/// Returns whether this list contains the given codepoint. This should only be called after
		/// \ref sort_and_compact() has been called.
		[[nodiscard]] bool contains(codepoint cp) const {
			auto it = std::lower_bound(
				ranges.begin(), ranges.end(), cp,
				[](codepoint_range range, codepoint c) {
					return range.last < c;
				}
			);
			return it != ranges.end() && cp >= it->first;
		}
	};

	namespace unicode {
		constexpr codepoint
			replacement_character = 0xFFFD, ///< Unicode replacement character.
			codepoint_invalid_min = 0xD800, ///< Minimum code point value reserved by UTF-16.
			codepoint_invalid_max = 0xDFFF, ///< Maximum code point value (inclusive) reserved by UTF-16.
			codepoint_max = 0x10FFFF; ///< Maximum code point value (inclusive) of Unicode.

		/// Determines if a codepoint lies in the valid range of Unicode points.
		constexpr inline bool is_valid_codepoint(codepoint c) {
			return c < codepoint_invalid_min || (c > codepoint_invalid_max && c <= codepoint_max);
		}
	}
}
