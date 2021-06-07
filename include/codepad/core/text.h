// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// String and text utilities.

#include <vector>
#include <string>

namespace codepad {
	/// The Knuth-Morris-Pratt algorithm. The pattern type must have a \p size() function and an indexing operator.
	/// The advantage of this matcher is that it's faster than naive matching for long strings while not requiring
	/// a bidirectional iterator.
	template <typename Pattern> struct kmp_matcher {
	public:
		/// The state of the matcher, used when finding multiple matches in the same string.
		struct state {
			friend kmp_matcher;
		protected:
			std::size_t _prefix_length = 0; ///< The current length of the matching prefix.
		};

		/// Initializes this matcher using the given pattern.
		explicit kmp_matcher(Pattern patt) : _patt(std::move(patt)) {
			_table = compute_prefix_table(_patt);
		}

		/// Processes one character/byte.
		///
		/// \return New state, and a bool indicating whether we're at the end of a match.
		[[nodiscard]] std::pair<state, bool> put(const typename Pattern::value_type &v, state st) {
			while (st._prefix_length > 0 && v != _patt[st._prefix_length]) {
				st._prefix_length = _table[st._prefix_length];
			}
			if (v == _patt[st._prefix_length]) {
				++st._prefix_length;
				if (st._prefix_length == _patt.size()) { // found a match
					st._prefix_length = _table.back();
					return { st, true };
				}
			}
			return { st, false };
		}

		/// Returns \ref _table.
		[[nodiscard]] const std::vector<std::size_t> &get_table() const {
			return _table;
		}


		/// Computes the prefix table for the given pattern.
		[[nodiscard]] inline static std::vector<std::size_t> compute_prefix_table(const Pattern &patt) {
			std::vector<std::size_t> result(patt.size() + 1, 0);
			std::size_t prefix_len = 0;
			for (std::size_t i = 1; i < patt.size(); ++i) {
				result[i] = prefix_len;
				while (prefix_len > 0 && patt[i] != patt[prefix_len]) {
					prefix_len = result[prefix_len];
				}
				if (patt[i] == patt[prefix_len]) {
					++prefix_len;
				}
			}
			result.back() = prefix_len;
			return result;
		}
	protected:
		Pattern _patt; ///< The pattern.
		std::vector<std::size_t> _table; ///< The table for partial (prefix) matches.
	};

	/// The type of a line ending.
	enum class line_ending : unsigned char {
		none, ///< Unspecified or invalid. Sometimes also used to indicate EOF or soft linebreaks.
		r, ///< \p \\r.
		n, ///< \p \\n, usually used in Linux.
		rn ///< \p \\r\\n, usually used in Windows.
	};
	/// Returns the length, in codepoints, of the string representation of a \ref line_ending.
	[[nodiscard]] std::size_t get_line_ending_length(line_ending);
	/// Returns the string representation of the given \ref line_ending.
	[[nodiscard]] std::u32string_view line_ending_to_string(line_ending);
}
