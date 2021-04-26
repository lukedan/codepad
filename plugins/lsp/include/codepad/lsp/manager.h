// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous plugin-wide functionalities.

#include <codepad/core/plugins.h>
#include <codepad/ui/manager.h>

#include <codepad/editors/manager.h>

namespace codepad::lsp {
	class interpretation_tag;

	/// Managers settings of the LSP plugin.
	class manager {
	public:
		/// Shorthand for \p std::shared_ptr to \ref editors::decoration_renderer.
		using decoration_renderer_ptr = std::shared_ptr<editors::decoration_renderer>;

		/// Initializes the decoration renderers.
		manager(const plugin_context&, editors::manager&);

		/// Returns the error decoration renderer for the given language.
		template <typename It> [[nodiscard]] decoration_renderer_ptr get_error_decoration(It beg, It end) {
			return _error_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the warning decoration renderer for the given language.
		template <typename It> [[nodiscard]] decoration_renderer_ptr get_warning_decoration(It beg, It end) {
			return _warning_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the info decoration renderer for the given language.
		template <typename It> [[nodiscard]] decoration_renderer_ptr get_info_decoration(It beg, It end) {
			return _info_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the hint decoration renderer for the given language.
		template <typename It> [[nodiscard]] decoration_renderer_ptr get_hint_decoration(It beg, It end) {
			return _hint_decoration->get_profile(beg, end).get_value();
		}

		/// Returns the \ref interpretation_tag associated with the given \ref editors::code::interpretation.
		[[nodiscard]] interpretation_tag *get_interpretation_tag_for(editors::code::interpretation&) const;

		/// Returns the \ref plugin_context.
		[[nodiscard]] const plugin_context &get_plugin_context() const {
			return _plugin_context;
		}
		/// Returns the \ref editors::manager.
		[[nodiscard]] editors::manager &get_editor_manager() const {
			return _editor_manager;
		}
		/// Returns the token for the \ref interpretation_tag.
		[[nodiscard]] const editors::buffer_manager::interpretation_tag_token &get_interpretation_tag_token() const {
			return _interpretation_tag_token;
		}

		/// Called when the plugin is enabled. Registers tags and events.
		void enable() {
			_interpretation_tag_token = get_editor_manager().buffers.allocate_interpretation_tag();
		}
		/// Called when the plugin is disabled. Unregisters tags and events.
		void disable() {
			get_editor_manager().buffers.deallocate_interpretation_tag(_interpretation_tag_token);
		}
	protected:
		/// The token for per-interpretation tags.
		editors::buffer_manager::interpretation_tag_token _interpretation_tag_token;

		std::unique_ptr<settings::retriever_parser<decoration_renderer_ptr>>
			_error_decoration, ///< Decoration renderer for errors.
			_warning_decoration, ///< Decoration renderer for warnings.
			_info_decoration, ///< Decoration renderer for informational diagnostics.
			_hint_decoration; ///< Decoration renderer for hints.
		const plugin_context &_plugin_context; ///< The \ref plugin_context.
		editors::manager &_editor_manager; ///< The \ref editors::manager.


		/// Wrapper around \ref editors::decoration_renderer::parse_static().
		[[nodiscard]] inline static settings::retriever_parser<
			decoration_renderer_ptr
		>::value_parser _create_decoration_renderer_parser(ui::manager &man, editors::manager &editor_man) {
			return [&](
				const std::optional<json::storage::value_t> &val
			) -> std::shared_ptr<editors::decoration_renderer> {
				if (val) {
					return editors::decoration_renderer::parse_static(val.value(), man, editor_man);
				}
				return nullptr;
			};
		}
	};
}
