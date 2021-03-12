// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/theme_manager.h"

/// \file
/// Implementation of the theme manager.

#include "codepad/editors/json_parsers.inl"

namespace codepad::editors {
	std::size_t theme_configuration::get_index_for(std::u8string_view key) const {
		std::size_t max_index = no_associated_theme, max_matches = 0;

		std::vector<std::u8string_view> key_parts;
		split_string(U'.', key, [&key_parts](std::u8string_view part) {
			key_parts.emplace_back(part);
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
		return max_index;
	}


	theme_manager::theme_manager(ui::manager &man) :
		_setting(man.get_settings().create_retriever_parser<theme_configuration>(
			{ u8"editor", u8"theme" },
			settings::basic_parsers::basic_type_with_default<theme_configuration>(
				theme_configuration(), ui::managed_json_parser<theme_configuration>(man)
			)
		)) {
	}
}
