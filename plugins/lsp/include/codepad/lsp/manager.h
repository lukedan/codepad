// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous plugin-wide functionalities.

#include <codepad/ui/manager.h>

namespace codepad::lsp {
	/// Managers settings of the LSP plugin.
	class manager {
	public:
		/// Initializes the decoration renderers.
		manager(ui::manager &man, editors::manager &editor_man) :
			_error_decoration(
				man.get_settings().create_retriever_parser<std::shared_ptr<editors::decoration_renderer>>(
					{ u8"lsp", u8"error_decoration" }, _create_decoration_renderer_parser(man, editor_man)
				)
			),
			_warning_decoration(
				man.get_settings().create_retriever_parser<std::shared_ptr<editors::decoration_renderer>>(
					{ u8"lsp", u8"warning_decoration" }, _create_decoration_renderer_parser(man, editor_man)
				)
			),
			_info_decoration(
				man.get_settings().create_retriever_parser<std::shared_ptr<editors::decoration_renderer>>(
					{ u8"lsp", u8"info_decoration" }, _create_decoration_renderer_parser(man, editor_man)
				)
			),
			_hint_decoration(
				man.get_settings().create_retriever_parser<std::shared_ptr<editors::decoration_renderer>>(
					{ u8"lsp", u8"hint_decoration" }, _create_decoration_renderer_parser(man, editor_man)
				)
			)
		{
		}

		/// Returns the error decoration renderer for the given language.
		template <typename It> [[nodiscard]] std::shared_ptr<editors::decoration_renderer> get_error_decoration(
			It beg, It end
		) {
			return _error_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the warning decoration renderer for the given language.
		template <typename It> [[nodiscard]] std::shared_ptr<editors::decoration_renderer> get_warning_decoration(
			It beg, It end
		) {
			return _warning_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the info decoration renderer for the given language.
		template <typename It> [[nodiscard]] std::shared_ptr<editors::decoration_renderer> get_info_decoration(
			It beg, It end
		) {
			return _info_decoration->get_profile(beg, end).get_value();
		}
		/// Returns the hint decoration renderer for the given language.
		template <typename It> [[nodiscard]] std::shared_ptr<editors::decoration_renderer> get_hint_decoration(
			It beg, It end
		) {
			return _hint_decoration->get_profile(beg, end).get_value();
		}
	protected:
		std::unique_ptr<settings::retriever_parser<std::shared_ptr<editors::decoration_renderer>>>
			_error_decoration, ///< Decoration renderer for errors.
			_warning_decoration, ///< Decoration renderer for warnings.
			_info_decoration, ///< Decoration renderer for informational diagnostics.
			_hint_decoration; ///< Decoration renderer for hints.


		/// Wrapper around \ref editors::decoration_renderer::parse_static().
		[[nodiscard]] inline static settings::retriever_parser<
			std::shared_ptr<editors::decoration_renderer>
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
