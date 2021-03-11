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

#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

#include "query.h"

namespace codepad::tree_sitter {
	/// Used with the various \p _index_* fields to indicate that no such capture is present.
	constexpr static uint32_t index_none = std::numeric_limits<uint32_t>::max();

	/// Wrapper around a \p TSLanguage and associated highlighting queries.
	class language_configuration {
	public:
		/// Creates a language configuration using the given \p TSLanguage and queries. The returned object takes
		/// control of the input \p TSLanguage.
		[[nodiscard]] static language_configuration create_for(
			std::u8string name, TSLanguage*,
			std::u8string_view injection_query,
			std::u8string_view locals_query,
			std::u8string_view highlights_query
		);

		/// Sets the highlight configuration and fills \ref _capture_highlights.
		void set_highlight_configuration(std::shared_ptr<editors::theme_configuration>);
		/// Returns \ref _highlight.
		[[nodiscard]] const std::shared_ptr<editors::theme_configuration> &get_highlight_configuration() const {
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
		/// Returns the name of this language. This may be different from the name registered to \ref manager.
		[[nodiscard]] const std::u8string &get_language_name() const {
			return _name;
		}
	protected:
		std::u8string _name; ///< The name of this language.
		std::shared_ptr<editors::theme_configuration> _highlight; ///< Current highlight configuration.
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
