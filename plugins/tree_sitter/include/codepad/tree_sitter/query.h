// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Used to handle highlight queries.

#include <string_view>
#include <variant>
#include <regex>

#include <tree_sitter/api.h>

#include <codepad/core/logging.h>

#include "wrappers.h"

namespace codepad::tree_sitter {
	/// Wrapper around a \p TSQuery with additional cached information.
	class query {
	public:
		/// Callback function used to obtain the contents of a node.
		using text_callback = std::function<std::u8string(const TSNode&)>;

		/// Finds the node with the specified capture index in the given \p TSQueryMatch. The returned \p TSNode is
		/// owned by the \p TSQueryMatch.
		[[nodiscard]] static const TSNode *find_node_for_capture(const TSQueryMatch&, uint32_t);

		/// Dummy struct used to indicate an empty predicate.
		struct invalid_predicate {
			/// Logs an error indicating that an invalid predicate is tested.
			bool test(const TSQueryMatch&, const text_callback&) const {
				codepad::logger::get().log_error(CP_HERE) << "invalid predicate tested";
				return false;
			}
		};
		/// A text predicate for equality and inequality between captures.
		struct captures_equality_predicate {
			uint32_t
				capture1 = std::numeric_limits<uint32_t>::max(), ///< Index of the first capture.
				capture2 = std::numeric_limits<uint32_t>::max(); ///< Index of the second capture.
			/// If this is \p true, then this is an equality predicate; otherwise this is an inequality predicate.
			bool inequality = false;

			/// Tests this predicate for the given match.
			bool test(const TSQueryMatch&, const text_callback&) const;
		};
		/// A text predicate for equality and inequality between a capture and a string literal.
		struct capture_literal_equality_predicate {
			std::u8string_view literal; ///< The string literal.
			uint32_t capture = std::numeric_limits<uint32_t>::max(); ///< Index of the capture.
			bool inequality = false; ///< \sa captures_equality_predicate::inequality.

			/// Tests this predicate for the given match.
			bool test(const TSQueryMatch&, const text_callback&) const;
		};
		/// A text predicate for regular expression matches and mismatches.
		struct capture_match_predicate {
			std::regex regex; ///< The regular expression.
			uint32_t capture = std::numeric_limits<uint32_t>::max(); ///< Index of the capture.
			bool inequality = false; ///< \sa capture_equality_predicate::inequality.

			/// Tests this predicate for the given match.
			bool test(const TSQueryMatch&, const text_callback&) const;
		};

		/// A union of text predicate types.
		using text_predicate = std::variant<
			invalid_predicate,
			captures_equality_predicate,
			capture_literal_equality_predicate,
			capture_match_predicate
		>;

		/// Core component of \p set!, \p is?, and \p is-not? predicates.
		struct property {
			std::u8string_view
				key, ///< The key.
				value; ///< The value.
			std::optional<uint32_t> capture; ///< The capture index.
		};
		/// Includes a \ref property and a \p bool for \p is? and \p is-not? predicates.
		struct property_predicate {
			property value; ///< The property value.
			/// If \p true, then this is a \p is-not? predicate. Otherwise it's a \p is? predicate.
			bool inequality = false;
		};

		/// A general predicate composed of multiple parts.
		struct general_predicate {
			/// A component can contain either a string literal or a capture index.
			using component = std::variant<std::u8string_view, uint32_t>;

			std::u8string_view op; ///< The operator of this predicate.
			std::vector<component> components; ///< The components of this predicate.
		};

		/// Default constructor.
		query() = default;

		/// Creates a \ref query for the given query string. This operation can fail, in which case an invalid
		/// \ref query object will be returned.
		[[nodiscard]] static query create_for(std::u8string_view, const TSLanguage*);

		/// Checks that the given match satisfies all text predicates of that pattern.
		[[nodiscard]] bool satisfies_text_predicates(const TSQueryMatch&, const text_callback&) const;

		/// Invokes the given callback function for every valid match. The loop terminates if the callback function
		/// returns \p false.
		template <typename Callback> void pattern_matches(
			TSQueryCursor *cursor, TSNode node, const text_callback &text_cb, Callback &&cb
		) const {
			ts_query_cursor_exec(cursor, _query.get(), node);
			TSQueryMatch match;
			while (ts_query_cursor_next_match(cursor, &match)) {
				if (satisfies_text_predicates(match, text_cb)) {
					if (!cb(match)) {
						break;
					}
				}
			}
		}

		/// Disables the pattern at the given index.
		void disable_pattern(uint32_t index) {
			ts_query_disable_pattern(_query.get(), index);
		}

		/// Returns the starting byte of the given pattern in the source.
		[[nodiscard]] uint32_t get_start_byte_for_pattern(uint32_t index) const {
			return ts_query_start_byte_for_pattern(_query.get(), index);
		}

		/// Returns the number of query patterns.
		[[nodiscard]] uint32_t get_num_patterns() const {
			return ts_query_pattern_count(_query.get());
		}

		/// Returns the list of capture names.
		[[nodiscard]] const std::vector<std::u8string_view> &get_captures() const {
			return _captures;
		}
		/// Returns \ref _property_settings.
		[[nodiscard]] const std::vector<std::vector<property>> &get_property_settings() const {
			return _property_settings;
		}
		/// Returns \ref _property_predicates.
		[[nodiscard]] const std::vector<std::vector<property_predicate>> &get_property_predicates() const {
			return _property_predicates;
		}

		/// Returns \ref _query.
		[[nodiscard]] const query_ptr &get_query() const {
			return _query;
		}

		/// Returns whether this query is valid.
		[[nodiscard]] bool valid() const {
			return !_query.empty();
		}
		/// Returns the result of \ref valid().
		[[nodiscard]] explicit operator bool() const {
			return valid();
		}

		/// Retrieves the name of the capture at the given index from the query object. The result is obtained from
		/// tree-sitter directly and not from \ref _captures.
		[[nodiscard]] std::u8string_view get_capture_name_at(uint32_t index) const {
			uint32_t len;
			const char *name = ts_query_capture_name_for_id(_query.get(), index, &len);
			return std::u8string_view(reinterpret_cast<const char8_t*>(name), static_cast<std::size_t>(len));
		}
		/// Returns the string in the given query at the given index.
		[[nodiscard]] std::u8string_view get_string_at(uint32_t index) const {
			uint32_t len;
			const char *name = ts_query_string_value_for_id(_query.get(), index, &len);
			return std::u8string_view(reinterpret_cast<const char8_t*>(name), static_cast<std::size_t>(len));
		}
	protected:
		/// Parses an equality predicate.
		[[nodiscard]] text_predicate _parse_equality_predicate(const TSQueryPredicateStep*, uint32_t, bool) const;
		/// Parses a regular expression predicate.
		[[nodiscard]] text_predicate _parse_match_predicate(const TSQueryPredicateStep*, uint32_t, bool) const;

		/// Parses a \ref property.
		[[nodiscard]] std::optional<property> _parse_property_predicate(const TSQueryPredicateStep*, uint32_t) const;

		std::vector<std::u8string_view> _captures; ///< Capture names.
		// FIXME this could be done better
		std::vector<std::vector<text_predicate>> _text_predicates; ///< All text predicates.
		std::vector<std::vector<property>> _property_settings; ///< All \p set! predicates.
		std::vector<std::vector<property_predicate>> _property_predicates; ///< All \p is? and \p is-not? predicates.
		std::vector<std::vector<general_predicate>> _general_predicates; ///< All other predicates.
		query_ptr _query; ///< The query used for highlighting.
	};
}
