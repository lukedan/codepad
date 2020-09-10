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

namespace tree_sitter {
	/// Wrapper around a \p TSQuery with additional cached information.
	class query {
	public:
		/// Callback function used to obtain the contents of a node.
		using text_callback = std::function<std::u8string(const TSNode&)>;

		/// Finds the node with the specified capture index in the given \p TSQueryMatch. The returned \p TSNode is
		/// owned by the \p TSQueryMatch.
		[[nodiscard]] inline static const TSNode *find_node_for_capture(
			const TSQueryMatch &match, uint32_t capture_index
		) {
			for (uint16_t i = 0; i < match.capture_count; ++i) {
				if (match.captures[i].index == capture_index) {
					return &match.captures[i].node;
				}
			}
			return nullptr;
		}

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
			bool test(const TSQueryMatch &match, const text_callback &text_cb) const {
				const TSNode
					*node1 = find_node_for_capture(match, capture1),
					*node2 = find_node_for_capture(match, capture2);
				if (node1 && node2) {
					return (text_cb(*node1) == text_cb(*node2)) != inequality;
				}
				codepad::logger::get().log_error(CP_HERE) << "invalid capture indices";
				return false;
			}
		};
		/// A text predicate for equality and inequality between a capture and a string literal.
		struct capture_literal_equality_predicate {
			std::u8string_view literal; ///< The string literal.
			uint32_t capture = std::numeric_limits<uint32_t>::max(); ///< Index of the capture.
			bool inequality = false; ///< \sa captures_equality_predicate::inequality.

			/// Tests this predicate for the given match.
			bool test(const TSQueryMatch &match, const text_callback &text_cb) const {
				const TSNode *node = find_node_for_capture(match, capture);
				if (node) {
					return (text_cb(*node) == literal) != inequality;
				}
				codepad::logger::get().log_error(CP_HERE) << "invalid capture index";
				return false;
			}
		};
		/// A text predicate for regular expression matches and mismatches.
		struct capture_match_predicate {
			std::regex regex; ///< The regular expression.
			uint32_t capture = std::numeric_limits<uint32_t>::max(); ///< Index of the capture.
			bool inequality = false; ///< \sa capture_equality_predicate::inequality.

			/// Tests this predicate for the given match.
			bool test(const TSQueryMatch &match, const text_callback &text_cb) const {
				const TSNode *node = find_node_for_capture(match, capture);
				if (node) {
					std::u8string text = text_cb(*node);
					const char *beg = reinterpret_cast<const char*>(text.data());
					const char *end = beg + text.size();
					return std::regex_match(beg, end, regex) != inequality;
				}
				codepad::logger::get().log_error(CP_HERE) << "invalid capture index";
				return false;
			}
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
		[[nodiscard]] inline static query create_for(std::u8string_view str, const TSLanguage *language) {
			query res;
			uint32_t error_offset;
			TSQueryError error;
			res._query.set(ts_query_new(
				language, reinterpret_cast<const char*>(str.data()), str.size(),
				&error_offset, &error
			));
			if (!res._query) {
				codepad::logger::get().log_error(CP_HERE) <<
					"failed to parse queries, offset: " << error_offset << ", error code: " << error;
				return res;
			}

			// capture names
			uint32_t num_captures = ts_query_capture_count(res._query.get());
			res._captures.reserve(static_cast<std::size_t>(num_captures));
			for (uint32_t i = 0; i < num_captures; ++i) {
				res._captures.emplace_back(res.get_capture_name_at(i));
			}

			// parse predicates for all patterns
			uint32_t num_patterns = ts_query_pattern_count(res._query.get());
			res._text_predicates.reserve(static_cast<std::size_t>(num_patterns));
			res._property_settings.reserve(static_cast<std::size_t>(num_patterns));
			res._property_predicates.reserve(static_cast<std::size_t>(num_patterns));
			res._general_predicates.reserve(static_cast<std::size_t>(num_patterns));
			for (uint32_t i = 0; i < num_patterns; ++i) {
				std::vector<text_predicate> text_preds;
				std::vector<property> set_preds;
				std::vector<property_predicate> is_preds;
				std::vector<general_predicate> general_preds;

				uint32_t num_preds;
				const TSQueryPredicateStep *preds = ts_query_predicates_for_pattern(res._query.get(), i, &num_preds);

				// parse all predicates for this pattern
				for (uint32_t start = 0, cur = 0; cur < num_preds; start = ++cur) {
					// find the end of the predicate
					for (; cur < num_preds && preds[cur].type != TSQueryPredicateStepTypeDone; ++cur) {
					}
					if (preds[start].type != TSQueryPredicateStepTypeString) {
						codepad::logger::get().log_error(CP_HERE) << "invalid predicate name for pattern " << i;
						continue;
					}

					// parse this predicate
					auto op = res.get_string_at(preds[start].value_id);
					if (op == u8"set!") {
						if (auto pred = res._parse_property_predicate(preds + start, cur - start)) {
							set_preds.emplace_back(std::move(pred.value()));
						}
					} else if (op == u8"is?") {
						if (auto pred = res._parse_property_predicate(preds + start, cur - start)) {
							property_predicate &prop_pred = is_preds.emplace_back();
							prop_pred.value = std::move(pred.value());
							prop_pred.inequality = false;
						}
					} else if (op == u8"is-not?") {
						if (auto pred = res._parse_property_predicate(preds + start, cur - start)) {
							property_predicate &prop_pred = is_preds.emplace_back();
							prop_pred.value = std::move(pred.value());
							prop_pred.inequality = true;
						}
					} else {
						text_predicate text_pred;
						if (op == u8"eq?") {
							text_pred = res._parse_equality_predicate(preds + start, cur - start, false);
						} else if (op == u8"not-eq?") {
							text_pred = res._parse_equality_predicate(preds + start, cur - start, true);
						} else if (op == u8"match?") {
							text_pred = res._parse_match_predicate(preds + start, cur - start, false);
						} else if (op == u8"not-match?") {
							text_pred = res._parse_match_predicate(preds + start, cur - start, true);
						} else { // other general predicates
							general_predicate &pred = general_preds.emplace_back();
							pred.op = op;
							pred.components.reserve(cur - start - 1);
							for (uint32_t comp_index = start + 1; comp_index < cur; ++comp_index) {
								const auto &comp = preds[comp_index];
								if (comp.type == TSQueryPredicateStepTypeString) {
									pred.components.emplace_back(
										std::in_place_type<std::u8string_view>, res.get_string_at(comp.value_id)
									);
								} else { // capture
									pred.components.emplace_back(
										std::in_place_type<uint32_t>, comp.value_id
									);
								}
							}
							continue;
						}
						if (!std::holds_alternative<invalid_predicate>(text_pred)) {
							text_preds.emplace_back(std::move(text_pred));
						}
					}
				}

				res._text_predicates.emplace_back(std::move(text_preds));
				res._property_settings.emplace_back(std::move(set_preds));
				res._property_predicates.emplace_back(std::move(is_preds));
				res._general_predicates.emplace_back(std::move(general_preds));
			}
			return res;
		}

		/// Checks that the given match satisfies all text predicates of that pattern.
		[[nodiscard]] bool satisfies_text_predicates(const TSQueryMatch &match, const text_callback &text_cb) const {
			for (const text_predicate &pred : _text_predicates[match.pattern_index]) {
				bool good = std::visit(
					[&match, &text_cb](const auto &pred) {
						return pred.test(match, text_cb);
					}, pred
				);
				if (!good) {
					return false;
				}
			}
			return true;
		}

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
		[[nodiscard]] text_predicate _parse_equality_predicate(
			const TSQueryPredicateStep *pred, uint32_t pred_length, bool inequality
		) const {
			if (pred_length != 3) {
				codepad::logger::get().log_error(CP_HERE) << "invalid number of arguments for equality predicate";
				return text_predicate();
			}
			if (pred[1].type != TSQueryPredicateStepTypeCapture) {
				codepad::logger::get().log_error(CP_HERE) << "first parameter of #eq? or #not-eq? must be a capture";
				return text_predicate();
			}

			text_predicate result;
			if (pred[2].type == TSQueryPredicateStepTypeCapture) {
				auto &eq = result.emplace<captures_equality_predicate>();
				eq.capture1 = pred[1].value_id;
				eq.capture2 = pred[2].value_id;
				eq.inequality = inequality;
			} else {
				auto &eq = result.emplace<capture_literal_equality_predicate>();
				eq.capture = pred[1].value_id;
				eq.literal = get_string_at(pred[2].value_id);
				eq.inequality = inequality;
			}
			return result;
		}
		/// Parses a regular expression predicate.
		[[nodiscard]] text_predicate _parse_match_predicate(
			const TSQueryPredicateStep *pred, uint32_t pred_length, bool inequality
		) const {
			if (pred_length != 3) {
				codepad::logger::get().log_error(CP_HERE) << "invalid number of arguments for match predicate";
				return text_predicate();
			}
			if (pred[1].type != TSQueryPredicateStepTypeCapture) {
				codepad::logger::get().log_error(CP_HERE) <<
					"first parameter of #match? or #not-match? must be a capture";
				return text_predicate();
			}
			if (pred[2].type == TSQueryPredicateStepTypeCapture) {
				codepad::logger::get().log_error(CP_HERE) <<
					"second parameter of #match? or #not-match? must be a literal";
				return text_predicate();
			}

			text_predicate result;
			auto &match = result.emplace<capture_match_predicate>();
			match.capture = pred[1].value_id;
			auto regex_str = get_string_at(pred[2].value_id);
			match.regex = std::regex(reinterpret_cast<const char*>(regex_str.data()), regex_str.size());
			match.inequality = inequality;
			return result;
		}

		/// Parses a \ref property.
		[[nodiscard]] std::optional<property> _parse_property_predicate(
			const TSQueryPredicateStep *pred, uint32_t pred_length
		) const {
			if (pred_length <= 1) {
				codepad::logger::get().log_error(CP_HERE) << "empty property predicate";
				return std::nullopt;
			}

			property result;
			const TSQueryPredicateStep *pred_end = pred + pred_length;
			for (++pred; pred != pred_end; ++pred) {
				if (pred->type == TSQueryPredicateStepTypeCapture) {
					if (result.capture.has_value()) {
						codepad::logger::get().log_error(CP_HERE) << "too many captures in property predicate";
						return std::nullopt;
					}
					result.capture.emplace(pred->value_id);
				} else {
					if (result.key.empty()) {
						result.key = get_string_at(pred->value_id);
					} else {
						if (!result.value.empty()) {
							codepad::logger::get().log_error(CP_HERE) << "too many literals in property predicate";
							return std::nullopt;
						}
						result.value = get_string_at(pred->value_id);
					}
				}
			}

			if (result.key.empty()) {
				codepad::logger::get().log_error(CP_HERE) << "no key in property predicate";
				return std::nullopt;
			}
			return std::move(result);
		}

		std::vector<std::u8string_view> _captures; ///< Capture names.
		// FIXME this could be done better
		std::vector<std::vector<text_predicate>> _text_predicates; ///< All text predicates.
		std::vector<std::vector<property>> _property_settings; ///< All \p set! predicates.
		std::vector<std::vector<property_predicate>> _property_predicates; ///< All \p is? and \p is-not? predicates.
		std::vector<std::vector<general_predicate>> _general_predicates; ///< All other predicates.
		query_ptr _query; ///< The query used for highlighting.
	};
}
