// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Declaration and implementation of manager classes.

#include <stack>
#include <regex>

#include <codepad/os/filesystem.h>

#include "code/interpretation.h"
#include "code/caret_set.h"
#include "code/contents_region.h"
#include "binary/contents_region.h"

#include "buffer.h"
#include "buffer_manager.h"
#include "decoration.h"
#include "theme_manager.h"

namespace codepad::editors {
	/// Manages everything related to editors. Essentially a hub for \ref buffer_manager, \ref encoding_manager,
	class manager {
	public:
		/// Constructor.
		explicit manager(ui::manager &man) : buffers(this), themes(man) {
			_language_mapping = man.get_settings().create_retriever_parser<
				std::vector<std::pair<std::regex, std::vector<std::u8string>>>
			>(
				{ u8"editor", u8"language_mapping" }, [](std::optional<json::storage::value_t> val) {
					std::vector<std::pair<std::regex, std::vector<std::u8string>>> result;
					if (val) {
						if (auto obj = val->cast<json::storage::object_t>()) {
							for (auto it = obj->member_begin(); it != obj->member_end(); ++it) {
								if (auto lang = it.value().cast<std::u8string_view>()) {
									auto &entry = result.emplace_back();
									entry.first = std::regex(
										reinterpret_cast<const char*>(it.name().data()), it.name().size()
									);
									split_string(U'.', lang.value(), [&](std::u8string_view part) {
										entry.second.emplace_back(part);
									});
								}
							}
						}
					}
					return result;
				}
			);
		}

		/// Registers built-in interaction modes.
		void register_builtin_interactions() {
			code_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<code::caret_set>
					>();
				}
			);
			code_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<code::caret_set>
					>();
				}
			);

			binary_interactions.mapping.emplace(
				u8"prepare_drag", []() {
					return std::make_unique<
						interaction_modes::mouse_prepare_drag_mode_activator<binary::caret_set>
					>();
				}
			);
			binary_interactions.mapping.emplace(
				u8"single_selection", []() {
					return std::make_unique<
						interaction_modes::mouse_single_selection_mode_activator<binary::caret_set>
					>();
				}
			);
		}

		/// Registers built-in decoration renderers.
		void register_builtin_decoration_renderers() {
			decoration_renderers.register_renderer<decoration_renderers::rounded_renderer>(
				u8"rounded_decoration_renderer"
			);
			decoration_renderers.register_renderer<decoration_renderers::squiggle_renderer>(
				u8"squiggle_decoration_renderer"
			);
		}

		/// Returns the language corresponding to the given file name.
		[[nodiscard]] const std::vector<std::u8string> *get_language_for_file(
			const std::filesystem::path &path
		) const {
			auto &mapping = _language_mapping->get_main_profile().get_value();
			auto str = path.generic_u8string();
			const char *beg = reinterpret_cast<const char*>(&*str.begin());
			const char *end = beg + str.size();
			for (auto &pair : mapping) {
				if (std::regex_match(beg, end, pair.first)) {
					return &pair.second;
				}
			}
			return nullptr;
		}


		buffer_manager buffers; ///< Manager of all buffers.
		code::encoding_registry encodings; ///< Encodings.
		/// \ref interaction_mode_registry for code editors.
		interaction_mode_registry<code::caret_set> code_interactions;
		/// \ref interaction_mode_registry for binary editors.
		interaction_mode_registry<binary::caret_set> binary_interactions;
		decoration_renderer_registry decoration_renderers; ///< A registry of decoration renderer types.
		theme_manager themes; ///< Theme information of different languages.
	protected:
		/// Mapping between file names and language names. Each entry is a pair where the first entry is a pattern
		/// for the file name, and the second entry is the language.
		std::unique_ptr<settings::retriever_parser<
			std::vector<std::pair<std::regex, std::vector<std::u8string>>>
		>> _language_mapping;
	};
}
