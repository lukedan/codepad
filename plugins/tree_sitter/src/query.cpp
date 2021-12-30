// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/query.h"

/// \file
/// Implementation of queries.

namespace codepad::tree_sitter {
	const TSNode *query::find_node_for_capture(const TSQueryMatch &match, uint32_t capture_index) {
		for (uint16_t i = 0; i < match.capture_count; ++i) {
			if (match.captures[i].index == capture_index) {
				return &match.captures[i].node;
			}
		}
		return nullptr;
	}

	bool query::captures_equality_predicate::test(const TSQueryMatch &match, const text_callback &text_cb) const {
		const TSNode
			*node1 = find_node_for_capture(match, capture1),
			*node2 = find_node_for_capture(match, capture2);
		if (node1 && node2) {
			return (text_cb(*node1) == text_cb(*node2)) != inequality;
		}
		codepad::logger::get().log_error() << "invalid capture indices";
		return false;
	}

	bool query::capture_literal_equality_predicate::test(
		const TSQueryMatch &match, const text_callback &text_cb
	) const {
		const TSNode *node = find_node_for_capture(match, capture);
		if (node) {
			return (text_cb(*node) == literal) != inequality;
		}
		codepad::logger::get().log_error() << "invalid capture index";
		return false;
	}

	bool query::capture_match_predicate::test(const TSQueryMatch &match, const text_callback &text_cb) const {
		const TSNode *node = find_node_for_capture(match, capture);
		if (node) {
			std::u8string text = text_cb(*node);
			const char *beg = reinterpret_cast<const char*>(text.data());
			const char *end = beg + text.size();
			return std::regex_match(beg, end, regex) != inequality;
		}
		codepad::logger::get().log_error() << "invalid capture index";
		return false;
	}

	query query::create_for(std::u8string_view str, const TSLanguage *language) {
		query res;
		uint32_t error_offset;
		TSQueryError error;
		res._query.set(ts_query_new(
			language,
			reinterpret_cast<const char*>(str.data()), static_cast<uint32_t>(str.size()),
			&error_offset, &error
		));
		if (!res._query) {
			codepad::logger::get().log_error() <<
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
					codepad::logger::get().log_error() << "invalid predicate name for pattern " << i;
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

	bool query::satisfies_text_predicates(const TSQueryMatch &match, const text_callback &text_cb) const {
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

	query::text_predicate query::_parse_equality_predicate(
		const TSQueryPredicateStep *pred, uint32_t pred_length, bool inequality
	) const {
		if (pred_length != 3) {
			codepad::logger::get().log_error() << "invalid number of arguments for equality predicate";
			return text_predicate();
		}
		if (pred[1].type != TSQueryPredicateStepTypeCapture) {
			codepad::logger::get().log_error() << "first parameter of #eq? or #not-eq? must be a capture";
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

	query::text_predicate query::_parse_match_predicate(
		const TSQueryPredicateStep *pred, uint32_t pred_length, bool inequality
	) const {
		if (pred_length != 3) {
			codepad::logger::get().log_error() << "invalid number of arguments for match predicate";
			return text_predicate();
		}
		if (pred[1].type != TSQueryPredicateStepTypeCapture) {
			codepad::logger::get().log_error() <<
				"first parameter of #match? or #not-match? must be a capture";
			return text_predicate();
		}
		if (pred[2].type == TSQueryPredicateStepTypeCapture) {
			codepad::logger::get().log_error() <<
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

	std::optional<query::property> query::_parse_property_predicate(
		const TSQueryPredicateStep *pred, uint32_t pred_length
	) const {
		if (pred_length <= 1) {
			codepad::logger::get().log_error() << "empty property predicate";
			return std::nullopt;
		}

		property result;
		const TSQueryPredicateStep *pred_end = pred + pred_length;
		for (++pred; pred != pred_end; ++pred) {
			if (pred->type == TSQueryPredicateStepTypeCapture) {
				if (result.capture.has_value()) {
					codepad::logger::get().log_error() << "too many captures in property predicate";
					return std::nullopt;
				}
				result.capture.emplace(pred->value_id);
			} else {
				if (result.key.empty()) {
					result.key = get_string_at(pred->value_id);
				} else {
					if (!result.value.empty()) {
						codepad::logger::get().log_error() << "too many literals in property predicate";
						return std::nullopt;
					}
					result.value = get_string_at(pred->value_id);
				}
			}
		}

		if (result.key.empty()) {
			codepad::logger::get().log_error() << "no key in property predicate";
			return std::nullopt;
		}
		return std::move(result);
	}
}
