// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/language_configuration.h"

/// \file
/// Implementation of language configurations.

namespace codepad::tree_sitter {
	language_configuration language_configuration::create_for(
		TSLanguage *lang,
		std::u8string_view injection_query,
		std::u8string_view locals_query,
		std::u8string_view highlights_query
	) {
		language_configuration res;
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

	void language_configuration::set_highlight_configuration(std::shared_ptr<highlight_configuration> config) {
		_highlight = std::move(config);
		if (_highlight) {
			_capture_highlights.resize(_query.get_captures().size());
			for (std::size_t i = 0; i < _capture_highlights.size(); ++i) {
				_capture_highlights[i] = _highlight->get_index_for(_query.get_captures()[i]);
			}
		}
	}
}


extern "C" {
	TSLanguage *tree_sitter_c();
	TSLanguage *tree_sitter_cpp();
	TSLanguage *tree_sitter_css();
	TSLanguage *tree_sitter_html();
	TSLanguage *tree_sitter_javascript();
	TSLanguage *tree_sitter_json();
}

namespace codepad::tree_sitter {
	/// Reads the entire given file as a string.
	std::u8string _read_file(const std::filesystem::path &p) {
		constexpr std::size_t _read_size = 1024000;
		std::u8string res;
		std::ifstream fin(p);
		while (fin) {
			std::size_t pos = res.size();
			res.resize(res.size() + _read_size);
			fin.read(reinterpret_cast<char*>(res.data() + pos), _read_size);
			if (fin.eof()) {
				res.resize(pos + fin.gcount());
				break;
			}
		}
		return res;
	}

	void language_manager::register_builtin_languages() {
		std::filesystem::path root = "plugins/tree_sitter/languages/";
		register_language(
			u8"c",
			std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_c(),
				u8"", u8"", _read_file(root / "tree-sitter-c/queries/highlights.scm")
			))
		);
		register_language(
			u8"cpp",
			std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_cpp(),
				u8"", u8"",
				_read_file(root / "tree-sitter-cpp/queries/highlights.scm") +
				_read_file(root / "tree-sitter-c/queries/highlights.scm")
			))
		);
		register_language(
			u8"css",
			std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_css(),
				u8"", u8"",
				_read_file(root / "tree-sitter-css/queries/highlights.scm")
			))
		);
		register_language(
			u8"html",
			std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_html(),
				_read_file(root / "tree-sitter-html/queries/injections.scm"),
				u8"",
				_read_file(root / "tree-sitter-html/queries/highlights.scm")
			))
		);
		{
			auto p05 = std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_javascript(),
				_read_file(root / "tree-sitter-javascript/queries/injections.scm"),
				_read_file(root / "tree-sitter-javascript/queries/locals.scm"),
				_read_file(root / "tree-sitter-javascript/queries/highlights-jsx.scm") +
				_read_file(root / "tree-sitter-javascript/queries/highlights.scm")
			));
			register_language(u8"javascript", p05);
			register_language(u8"js", p05);
		}
		register_language(
			u8"json",
			std::make_shared<language_configuration>(language_configuration::create_for(
				tree_sitter_json(),
				u8"", u8"", u8"" // TODO no scm files?
			))
		);
	}

	std::shared_ptr<language_configuration> language_manager::register_language(
		std::u8string lang, std::shared_ptr<language_configuration> config
	) {
		config->set_highlight_configuration(_highlight_config);
		auto [it, inserted] = _languages.try_emplace(std::move(lang), std::move(config));
		if (!inserted) {
			std::swap(config, it->second);
			return std::move(config);
		}
		return nullptr;
	}

	void language_manager::set_highlight_configuration(std::shared_ptr<highlight_configuration> new_cfg) {
		_highlight_config = std::move(new_cfg);
		for (auto &pair : _languages) {
			pair.second->set_highlight_configuration(_highlight_config);
		}
	}
}
