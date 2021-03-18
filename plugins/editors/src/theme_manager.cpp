// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/theme_manager.h"

/// \file
/// Implementation of the theme manager.

#include "codepad/editors/json_parsers.inl"

namespace codepad::ui::property_finders {
	template <> property_info find_property_info_managed<editors::text_theme_specification>(
		component_property_accessor_builder &builder, manager &man
	) {
		if (!builder.move_next()) {
			return builder.finish_and_create_property_info_managed<editors::text_theme_specification>(man);
		}
		builder.expect_type(u8"text_theme_specification");
		if (builder.current_component().property == u8"color") {
			return builder.append_member_and_find_property_info_managed<
				&editors::text_theme_specification::color
			>(man);
		}
		if (builder.current_component().property == u8"style") {
			return builder.append_member_and_find_property_info<&editors::text_theme_specification::style>();
		}
		if (builder.current_component().property == u8"weight") {
			return builder.append_member_and_find_property_info<&editors::text_theme_specification::weight>();
		}
		return builder.fail();
	}
}


namespace codepad::editors {
	std::size_t theme_configuration::get_index_for(std::u8string_view key) const {
		std::vector<std::u8string_view> key_parts;
		split_string(U'.', key, [&key_parts](std::u8string_view part) {
			key_parts.emplace_back(part);
		});
		return get_index_for(std::move(key_parts));
	}

	std::size_t theme_configuration::get_index_for(std::vector<std::u8string_view> key_parts) const {
		std::sort(key_parts.begin(), key_parts.end());

		std::size_t max_index = no_associated_theme, max_matches = 0, min_length = 1, conflicts = 0;
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

			// if the same number of matching key parts are the same, take the shorter one
			if (matches > max_matches || (matches == max_matches && cur_entry.key.size() < min_length)) {
				max_matches = matches;
				min_length = cur_entry.key.size();
				max_index = i;
				conflicts = 0;
			} else if (matches == max_matches && cur_entry.key.size() == min_length) {
				++conflicts;
			}
		}

		auto entry = logger::get().log_debug(CP_HERE);
		bool first = true;
		for (auto &part : key_parts) {
			if (!first) {
				entry << ".";
			}
			first = false;
			entry << part;
		}
		entry << " -> ";
		if (max_index == no_associated_theme) {
			entry << "[none]";
		} else {
			first = true;
			for (auto &part : entries[max_index].key) {
				if (!first) {
					entry << ".";
				}
				first = false;
				entry << part;
			}
		}
		if (max_matches > 0 && conflicts > 0) {
			logger::get().log_error(CP_HERE) << "multiple candidates found; which one is picked is unspecified";
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
