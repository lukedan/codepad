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

		/// Finds the next match within the specified range and returns the resulting iterator and \ref state. To
		/// find multiple matches in the same string, the caller needs to manually increment the iterator once
		/// between calls, and in each iteration pass in the \ref state object returned by the previous iteration.
		/// 
		/// \return The returned iterator points to the last character of the matching substring, not the start of
		///         it. If no matching substring is found, \p end will be returned.
		template <typename Iter> [[nodiscard]] std::pair<Iter, state> next_match(
			Iter begin, Iter end, state st
		) const {
			for (; begin != end; ++begin) {
				while (st._prefix_length > 0 && *begin != _patt[st._prefix_length]) {
					st._prefix_length = _table[st._prefix_length];
				}
				if (*begin == _patt[st._prefix_length]) {
					++st._prefix_length;
					if (st._prefix_length == _patt.size()) { // found a match
						st._prefix_length = _table.back();
						return { begin, st };
					}
				}
			}
			return { end, st };
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
}
