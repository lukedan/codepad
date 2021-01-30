// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of language configurations.

#include <string_view>
#include <string>
#include <algorithm>
#include <map>
#include <unordered_map>

#include <tree_sitter/api.h>

#include <codepad/editors/code/interpretation.h>

#include "query.h"

namespace codepad::tree_sitter {
	/// Used with the various \p _index_* fields to indicate that no such capture is present.
	constexpr static uint32_t index_none = std::numeric_limits<uint32_t>::max();

	/// Stores mapping between capture type strings and font parameters.
	class highlight_configuration {
	public:
		/// Indicates that no name is associated with a layer.
		constexpr static std::size_t no_associated_theme = std::numeric_limits<std::size_t>::max();

		/// One entry in the configuration.
		struct entry {
			/// The key used to identify this entry. In order for lookups to function properly this should be sorted.
			std::vector<std::u8string> key;
			editors::code::text_theme_specification theme; ///< Theme associated with the key.

			/// Constructs a new entry from the given key.
			[[nodiscard]] inline static entry construct(
				std::u8string_view key, editors::code::text_theme_specification theme
			) {
				entry result;
				result.theme = theme;
				split_path(key, [&result](std::u8string_view s) {
					result.key.emplace_back(s);
					return true;
				});
				std::sort(result.key.begin(), result.key.end());
				return result;
			}
		};

		/// Calls the callback function for all components in the given path, terminating once the callback returns
		/// \p false.
		template <typename Callback> inline static void split_path(std::u8string_view path, Callback &&cb) {
			auto it = path.begin(), last = path.begin();
			while (it != last) {
				auto char_beg = it;
				codepoint cp;
				if (!encodings::utf8::next_codepoint(it, path.end(), cp)) {
					cp = encodings::replacement_character;
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

		/// Shorthand for constructring an entry and adding it to \ref entries.
		void add_entry(std::u8string_view key, editors::code::text_theme_specification theme) {
			entries.emplace_back(entry::construct(key, theme));
		}
		/// Returns the theme index for the given dot-separated key.
		[[nodiscard]] std::size_t get_index_for(std::u8string_view key) const {
			std::size_t max_index = no_associated_theme, max_matches = 0;

			std::vector<std::u8string_view> key_parts;
			split_path(key, [this, &key_parts](std::u8string_view part) {
				key_parts.emplace_back(part);
				return true;
			});
			std::sort(key_parts.begin(), key_parts.end());

			for (std::size_t i = 0; i < entries.size(); ++i) {
				auto &cur_entry = entries[i];

				std::size_t matches = 0;
				auto part_it = key_parts.begin();
				auto ent_part_it = cur_entry.key.begin();
				while (part_it != key_parts.end() && ent_part_it != cur_entry.key.end()) {
					int cmp = part_it->compare(*ent_part_it);
					if (cmp == 0) {
						++matches;
						++part_it;
						++ent_part_it;
					} else if (cmp < 0) {
						++part_it;
					} else {
						++ent_part_it;
					}
				}

				if (matches > max_matches) {
					max_matches = matches;
					max_index = i;
				}
			}

			if (max_index == no_associated_theme) {
				codepad::logger::get().log_info(CP_HERE) << "no highlight for " << key;
			}
			return max_index;
		}

		std::vector<entry> entries; ///< Entries of this configuration.
	};

	/// Wrapper around a \p TSLanguage and associated highlighting queries.
	class language_configuration {
	public:
		/// Creates a language configuration using the given \p TSLanguage and queries. The returned object takes
		/// control of the input \p TSLanguage.
		[[nodiscard]] static language_configuration create_for(
			TSLanguage*,
			std::u8string_view injection_query,
			std::u8string_view locals_query,
			std::u8string_view highlights_query
		);

		/// Sets the highlight configuration and fills \ref _capture_highlights.
		void set_highlight_configuration(std::shared_ptr<highlight_configuration>);
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
