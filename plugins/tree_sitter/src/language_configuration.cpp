// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/language_configuration.h"

/// \file
/// Implementation of language configurations.

namespace codepad::tree_sitter {
	language_configuration language_configuration::create_for(
		std::u8string name, TSLanguage *lang,
		std::u8string_view injection_query,
		std::u8string_view locals_query,
		std::u8string_view highlights_query
	) {
		language_configuration res;
		res._name = std::move(name);
		res._language = lang;

		// concatenate the query strings and construct a single query
		// then record the indices of patterns in each query
		std::u8string full_query;
		full_query.reserve(injection_query.size() + locals_query.size() + highlights_query.size());
		full_query.append(injection_query.begin(), injection_query.end());
		full_query.append(locals_query.begin(), locals_query.end());
		full_query.append(highlights_query.begin(), highlights_query.end());

		res._query = query::create_for(full_query, res._language);
		if (res._query) {
			uint32_t locals_query_offset = static_cast<uint32_t>(injection_query.size());
			uint32_t highlights_query_offset = locals_query_offset + static_cast<uint32_t>(locals_query.size());
			res._locals_pattern_index = res._highlights_pattern_index = 0;
			for (uint32_t i = 0; i < res._query.get_num_patterns(); ++i) {
				uint32_t pattern_offset = res._query.get_start_byte_for_pattern(i);
				if (pattern_offset >= highlights_query_offset) {
					break;
				}
				++res._highlights_pattern_index;
				if (pattern_offset < locals_query_offset) {
					++res._locals_pattern_index;
				}
			}
		}

		// construct a separate query for combined injections
		res._combined_injections_query = query::create_for(injection_query, res._language);
		bool has_combined_queries = false;
		if (res._combined_injections_query) {
			for (uint32_t i = 0; i < res._combined_injections_query.get_num_patterns(); ++i) {
				auto &properties = res._combined_injections_query.get_property_settings()[i];
				for (auto &prop : properties) {
					if (prop.key == u8"injection.combined") {
						has_combined_queries = false;
						res._query.disable_pattern(i);
					} else {
						res._combined_injections_query.disable_pattern(i);
					}
				}
			}
		}
		if (!has_combined_queries) {
			res._combined_injections_query = query();
		}

		// non local variable patterns
		for (auto &predicates : res._query.get_property_predicates()) {
			bool non_local = false;
			for (auto &pred : predicates) {
				if (pred.value.key == u8"local" && pred.inequality) {
					non_local = true;
					break;
				}
			}
			res._non_local_variable_patterns.emplace_back(non_local);
		}

		// cache indices of special captures
		auto &captures = res._query.get_captures();
		for (std::size_t i = 0; i < captures.size(); ++i) {
			std::u8string_view capture = captures[i];
			if (capture == u8"injection.content") {
				res._capture_injection_content = static_cast<uint32_t>(i);
			} else if (capture == u8"injection.language") {
				res._capture_injection_language = static_cast<uint32_t>(i);
			} else if (capture == u8"local.definition") {
				res._capture_local_definition = static_cast<uint32_t>(i);
			} else if (capture == u8"local.definition-value") {
				res._capture_local_definition_value = static_cast<uint32_t>(i);
			} else if (capture == u8"local.reference") {
				res._capture_local_reference = static_cast<uint32_t>(i);
			} else if (capture == u8"local.scope") {
				res._capture_local_scope = static_cast<uint32_t>(i);
			}
		}

		return res;
	}

	void language_configuration::set_highlight_configuration(std::shared_ptr<editors::theme_configuration> config) {
		_highlight = std::move(config);
		if (_highlight) {
			_capture_highlights.resize(_query.get_captures().size());
			for (std::size_t i = 0; i < _capture_highlights.size(); ++i) {
				_capture_highlights[i] = _highlight->get_index_for(_query.get_captures()[i]);
			}
		}
	}
}
