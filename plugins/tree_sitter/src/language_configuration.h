// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of language configurations.

#include <string_view>
#include <string>
#include <map>

#include <tree_sitter/api.h>

#include "query.h"

namespace tree_sitter {
	/// Used with the various \p _index_* fields to indicate that no such capture is present.
	constexpr static uint32_t index_none = std::numeric_limits<uint32_t>::max();

	/// Stores mapping between capture type strings and font parameters.
	class highlight_configuration {
	public:
		/// Indicates that no name is associated with a layer.
		constexpr static std::size_t no_associated_theme = std::numeric_limits<std::size_t>::max();

		/// One layer in the configuration.
		struct layer {
			std::map<std::u8string, layer, std::less<void>> layer_mapping; ///< Mapping into the next layer.
			std::size_t theme_index = no_associated_theme; ///< The index of this layer's theme in \ref _themes.
		};

		/// Calls the callback function for all components in the given path, terminating once the callback returns
		/// \p false.
		template <typename Callback> inline static void split_path(std::u8string_view path, Callback &&cb) {
			auto it = path.begin(), last = path.begin();
			while (it != last) {
				auto char_beg = it;
				codepad::codepoint cp;
				if (!codepad::encodings::utf8::next_codepoint(it, path.end(), cp)) {
					cp = codepad::encodings::replacement_character;
				}
				if (cp == '.') {
					if (!cb(path.substr(last - path.begin(), char_beg - last))) {
						return;
					}
					last = it;
				}
			}
			cb(path.substr(last - path.begin(), path.end() - last));
		}

		/// Pushes the given theme onto \ref _themes, and sets the index of that key to the index of the new theme.
		///
		/// \return The old highlight index.
		std::size_t set_theme_for(std::u8string_view key, codepad::editors::code::text_theme_specification theme) {
			std::size_t new_id = _themes.size();
			_themes.emplace_back(std::move(theme));
			return set_index_for(key, new_id);
		}

		/// Sets the highlight index for the given key.
		///
		/// \return The old highlight index.
		std::size_t set_index_for(std::u8string_view key, std::size_t id) {
			layer *current = &_root_layer;
			split_path(key, [this, &current](std::u8string_view part) {
				auto [it, inserted] = current->layer_mapping.emplace(std::u8string(part), layer());
				current = &it->second;
				return true;
			});
			std::size_t res = no_associated_theme;
			res = current->theme_index;
			current->theme_index = id;
			return res;
		}
		/// Returns the theme index for the given dot-separated key.
		std::size_t get_index_for(std::u8string_view key) const {
			const layer *current = &_root_layer;
			std::size_t index = no_associated_theme;

			split_path(key, [this, &current, &index](std::u8string_view part) {
				auto next_layer = current->layer_mapping.find(part);
				if (next_layer == current->layer_mapping.end()) {
					return false;
				}
				// update result
				if (next_layer->second.theme_index != no_associated_theme) {
					index = next_layer->second.theme_index;
				}
				// advance to the next layer
				current = &next_layer->second;
				return true;
			});

			if (index == no_associated_theme) {
				codepad::logger::get().log_info(CP_HERE) << "no highlight for " << key;
			}
			return index;
		}

		/// Returns \ref _themes.
		std::vector<codepad::editors::code::text_theme_specification> &themes() {
			return _themes;
		}
		/// \overload
		const std::vector<codepad::editors::code::text_theme_specification> &themes() const {
			return _themes;
		}
	protected:
		std::vector<codepad::editors::code::text_theme_specification> _themes; ///< The values of all themes.
		layer _root_layer; ///< The top layer.
	};

	/// Wrapper around a \p TSLanguage and associated highlighting queries.
	class language_configuration {
	public:
		/// Creates a language configuration using the given \p TSLanguage and queries. The returned object takes
		/// control of the input \p TSLanguage.
		[[nodiscard]] inline static language_configuration create_for(
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

		/// Sets the highlight configuration and fills \ref _capture_highlights.
		void set_highlight_configuration(std::shared_ptr<highlight_configuration> config) {
			_highlight = std::move(config);
			if (_highlight) {
				_capture_highlights.resize(_query.get_captures().size());
				for (std::size_t i = 0; i < _capture_highlights.size(); ++i) {
					_capture_highlights[i] = _highlight->get_index_for(_query.get_captures()[i]);
				}
			}
		}
		/// Returns \ref _highlight.
		[[nodiscard]] const std::shared_ptr<highlight_configuration> &get_highlight_configuration() const {
			return _highlight;
		}
		/// Returns \ref _capture_highlights.
		[[nodiscard]] const std::vector<std::size_t> &get_capture_highlight_indices() const {
			return _capture_highlights;
		}

		/// Returns the query.
		[[nodiscard]] const query &get_query() const {
			return _query;
		}
		/// Returns the query for combined injections.
		[[nodiscard]] const query &get_combined_injections_query() const {
			return _combined_injections_query;
		}

		/// Returns \ref _locals_pattern_index.
		[[nodiscard]] uint32_t get_locals_pattern_index() const {
			return _locals_pattern_index;
		}
		/// Returns \ref _highlights_pattern_index.
		[[nodiscard]] uint32_t get_highlights_pattern_index() const {
			return _highlights_pattern_index;
		}

		/// Returns \ref _non_local_variable_patterns.
		[[nodiscard]] const std::vector<bool> get_non_local_variable_patterns() const {
			return _non_local_variable_patterns;
		}

		/// Returns \ref _capture_injection_content.
		[[nodiscard]] uint32_t get_injection_content_capture_index() const {
			return _capture_injection_content;
		}
		/// Returns \ref _capture_injection_language.
		[[nodiscard]] uint32_t get_injection_language_capture_index() const {
			return _capture_injection_language;
		}
		/// Returns \ref _capture_local_definition.
		[[nodiscard]] uint32_t get_local_definition_capture_index() const {
			return _capture_local_definition;
		}
		/// Returns \ref _capture_local_definition_value.
		[[nodiscard]] uint32_t get_local_definition_value_capture_index() const {
			return _capture_local_definition_value;
		}
		/// Returns \ref _capture_local_reference.
		[[nodiscard]] uint32_t get_local_reference_capture_index() const {
			return _capture_local_reference;
		}
		/// Returns \ref _capture_local_scope.
		[[nodiscard]] uint32_t get_local_scope_capture_index() const {
			return _capture_local_scope;
		}


		/// Returns the underlying \p TSLanguage.
		[[nodiscard]] const TSLanguage *get_language() const {
			return _language;
		}
	protected:
		std::shared_ptr<highlight_configuration> _highlight; ///< Current highlight configuration.
		std::vector<std::size_t> _capture_highlights; ///< Highlight indices of all captures.

		query
			_query, ///< Query that contains all patterns except for combined injections.
			_combined_injections_query; ///< Query that contains patterns for combined injections.
		std::vector<bool> _non_local_variable_patterns; ///< Whether a pattern has been disabled for local variables.
		const TSLanguage *_language = nullptr; ///< The language.
		uint32_t
			_locals_pattern_index = index_none, ///< Index of the first local pattern.
			_highlights_pattern_index = index_none, ///< Index of the first highlight pattern.

			_capture_injection_content = index_none, ///< Index of the <tt>injection.content</tt> capture.
			_capture_injection_language = index_none, ///< Index of the <tt>injection.language</tt> capture.
			_capture_local_definition = index_none, ///< Index of the <tt>local.definition</tt> capture.
			_capture_local_definition_value = index_none, ///< Index of the <tt>local.definition-value</tt> capture.
			_capture_local_reference = index_none, ///< Index of the <tt>local.reference</tt> capture.
			_capture_local_scope = index_none; ///< Index of the <tt>local.scope</tt> capture.
	};
}
