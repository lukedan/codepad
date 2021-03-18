// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager for theme configurations for different languages.

#include <vector>
#include <memory>

#include <codepad/core/settings.h>
#include <codepad/ui/manager.h>

namespace codepad::editors {
	/// Specifies the theme of the text at a specific point.
	struct text_theme_specification {
		/// Default constructor.
		text_theme_specification() = default;
		/// Initializes all members of this struct.
		text_theme_specification(colord c, ui::font_style st, ui::font_weight w) : color(c), style(st), weight(w) {
		}

		colord color; ///< The color of the text.
		ui::font_style style = ui::font_style::normal; ///< The font style.
		ui::font_weight weight = ui::font_weight::normal; ///< The font weight.
	};
}
namespace codepad::ui {
	/// Managed parser for \ref editors::text_theme_specification.
	template <> struct managed_json_parser<editors::text_theme_specification> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &man) : _manager(man) {
		}

		/// Parses the theme specification.
		template <typename Value> std::optional<
			editors::text_theme_specification
		> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};
	namespace property_finders {
		/// Specialization for \ref editors::text_theme_specification.
		template <> property_info find_property_info_managed<editors::text_theme_specification>(
			component_property_accessor_builder&, manager&
		);
	}
}

namespace codepad::editors {
	/// Theme information for various token types of a specific language.
	struct theme_configuration {
	public:
		/// Indicates that no name is associated with a layer.
		constexpr static std::size_t no_associated_theme = std::numeric_limits<std::size_t>::max();

		/// One entry in the configuration.
		struct entry {
			/// The key used to identify this entry. In order for lookups to function properly this should be sorted.
			std::vector<std::u8string> key;
			editors::text_theme_specification theme; ///< Theme associated with the key.

			/// Constructs a new entry from the given key.
			[[nodiscard]] inline static entry construct(
				std::u8string_view key, editors::text_theme_specification theme
			) {
				entry result;
				result.theme = theme;
				split_string(U'.', key, [&result](std::u8string_view s) {
					result.key.emplace_back(s);
				});
				std::sort(result.key.begin(), result.key.end());
				return result;
			}
		};

		/// Shorthand for constructring an entry and adding it to \ref entries.
		void add_entry(std::u8string_view key, editors::text_theme_specification theme) {
			entries.emplace_back(entry::construct(key, theme));
		}
		/// Returns the theme index for the given dot-separated key.
		[[nodiscard]] std::size_t get_index_for(std::u8string_view) const;
		/// Returns the theme index for the key composed of the given parts.
		[[nodiscard]] std::size_t get_index_for(std::vector<std::u8string_view>) const;

		std::vector<entry> entries; ///< Entries of this configuration.
	};
}
namespace codepad::ui {
	/// Parser for \ref editors::theme_configuration.
	template <> struct managed_json_parser<editors::theme_configuration> {
	public:
		/// Initializes \ref _manager.
		explicit managed_json_parser(manager &man) : _manager(man) {
		}

		/// The parser interface.
		template <typename Value> std::optional<editors::theme_configuration> operator()(const Value&) const;
	protected:
		manager &_manager; ///< The associated \ref manager.
	};
}
namespace codepad::editors {
	/// Manages theme data.
	class theme_manager {
	public:
		/// Initializes \ref _setting using the given \ref ui::manager.
		explicit theme_manager(ui::manager&);

		/// Returns a \ref theme_configuration for the given language.
		[[nodiscard]] const std::shared_ptr<theme_configuration> &get_theme_for_language(
			std::u8string_view lang
		) {
			auto [it, inserted] = _themes.emplace(lang, nullptr);
			if (inserted) {
				std::vector<std::u8string_view> profile;
				split_string(U'.', lang, [&profile](std::u8string_view s) {
					profile.emplace_back(s);
				});
				it->second = std::make_shared<theme_configuration>(
					_setting->get_value(profile.begin(), profile.end())
				);
			}
			return it->second;
		}
	protected:
		/// All cached theme data.
		std::unordered_map<
			std::u8string, std::shared_ptr<theme_configuration>, string_hash<>, std::equal_to<>
		> _themes;
		/// Used to retrieve and parse configuration values.
		std::unique_ptr<settings::retriever_parser<theme_configuration>> _setting;
	};
}
