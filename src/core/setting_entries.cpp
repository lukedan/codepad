// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Global definitions of getters for setting entries.

#include "settings.h"
#include "../ui/common_elements.h"
#include "../editors/editor.h"
#include "../editors/code/contents_region.h"
#include "../editors/binary/contents_region.h"

using namespace std;

namespace codepad {
	/// A setting that handles the case of multiple \ref settings objects. Note that actually using mutliple
	/// \ref setting objects can degrade performance.
	template <typename T> struct setting {
	public:
		using parser_type = typename settings::retriever_parser<T>::value_parser; ///< Parser type.

		/// Initializes this setting given the key and the parser.
		setting(std::vector<std::u8string> k, parser_type p) :
			_context(std::make_shared<typename settings::retriever_parser<T>::context>(std::move(k), std::move(p))) {
		}

		/// Returns the \ref settings::retriever_parser associated with the given \ref settings object.
		settings::retriever_parser<T> &get(settings &s) {
			if (&s != _settings) { // slow path
				if (_settings) { // put the current retriever back onto the shelf
					_slow_map.emplace(_settings, std::move(_retriever));
				}
				_settings = &s;
				auto it = _slow_map.find(_settings);
				if (it == _slow_map.end()) { // create new retriever
					_retriever = _settings->create_retriever_parser<T>(_context);
				} else { // take the retriever out & erase the entry
					_retriever = std::move(it->second);
					_slow_map.erase(it);
				}
			}
			return _retriever;
		}
	private:
		std::shared_ptr<typename settings::retriever_parser<T>::context> _context; ///< The context of this setting.
		/// A mapping for \ref settings objects that are not currently active.
		std::map<settings*, settings::retriever_parser<T>> _slow_map;
		settings::retriever_parser<T> _retriever; ///< The \ref settings::retriever_parser.
		settings *_settings = nullptr; ///< The current \ref settings object.
	};

	namespace editors {
		settings::retriever_parser<double> &editor::get_font_size_setting(settings &set) {
			static setting<double> _setting(
				{ u8"editor", u8"font_size" }, settings::basic_parsers::basic_type_with_default<double>(12.0)
			);
			return _setting.get(set);
		}

		settings::retriever_parser<std::u8string> &editor::get_font_family_setting(settings &set) {
			static setting<std::u8string> _setting(
				{ u8"editor", u8"font_family" },
				settings::basic_parsers::basic_type_with_default<std::u8string>(u8"Courier New")
			);
			return _setting.get(set);
		}

		settings::retriever_parser<vector<std::u8string>> &editor::get_interaction_modes_setting(settings &set) {
			static setting<vector<std::u8string>> _setting(
				{ u8"editor", u8"interaction_modes" },
				settings::basic_parsers::basic_type_with_default(vector<std::u8string>(), json::array_parser<std::u8string>())
			);
			return _setting.get(set);
		}


		namespace code {
			settings::retriever_parser<std::vector<std::u8string>> &contents_region::get_backup_fonts_setting(settings &set) {
				static setting<vector<std::u8string>> _setting(
					{ u8"editor", u8"backup_fonts" },
					settings::basic_parsers::basic_type_with_default<std::vector<std::u8string>>(
						std::vector<std::u8string>(), json::array_parser<std::u8string>()
						)
				);
				return _setting.get(set);
			}
		}
	}
}