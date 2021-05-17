// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/search_panel.h"

/// \file
/// Implementation of search panels.

#include "../details.h"

namespace codepad::editors::code {
	settings::retriever_parser<
		std::shared_ptr<decoration_renderer>
	> &search_panel::get_decoration_renderer_setting(settings &sett) {
		static setting<std::shared_ptr<decoration_renderer>> _setting(
			{ u8"editor", u8"search_result_decoration" },
			decoration_renderer::create_setting_parser(
				*_details::get_plugin_context().ui_man, _details::get_manager()
			)
		);
		return _setting.get(sett);
	}

	void search_panel::_on_input_changed() {
		using _codepoint_str = std::basic_string<codepoint>;

		_results.clear();

		std::size_t pattern_length = 0;
		if (!_input->get_text().empty()) {
			// decode search string
			_codepoint_str pattern;
			for (auto it = _input->get_text().begin(); it != _input->get_text().end(); ) {
				codepoint cp;
				if (!encodings::utf8::next_codepoint(it, _input->get_text().end(), cp)) {
					logger::get().log_error(CP_HERE) << "invalid codepoint in search string: " << cp;
					cp = encodings::replacement_character;
				}
				pattern.push_back(cp);
			}
			pattern_length = pattern.size();

			// match
			auto &doc = _contents->get_document();
			kmp_matcher<_codepoint_str> matcher(std::move(pattern));
			kmp_matcher<_codepoint_str>::state st;
			std::size_t position = 0;
			auto it = doc.character_at(position);
			while (!it.codepoint().ended()) {
				codepoint cp;
				if (it.is_linebreak()) {
					cp = U'\n';
				} else {
					cp =
						it.codepoint().is_codepoint_valid() ?
						it.codepoint().get_codepoint() :
						encodings::replacement_character;
				}
				auto [new_st, match] = matcher.put(cp, st);

				st = new_st;
				++position;
				it.next();
				if (match) {
					_results.emplace_back(position - pattern_length);
				}
			}
		}

		// update result list box source
		if (_result_list) {
			static_cast<_match_result_source*>(_result_list->get_source())->on_items_changed();
		}
		// update decorations
		{
			auto mod = _decoration_token.modify();
			if (mod->renderers.empty()) {
				auto &lang = _contents->get_document().get_buffer().get_language();
				mod->renderers.emplace_back(get_decoration_renderer_setting(
					*_details::get_plugin_context().sett
				).get_value(lang.begin(), lang.end()));
			}
			decoration_renderer *rend = mod->renderers.front().get();
			mod->decorations.clear();
			for (std::size_t pos : _results) {
				mod->decorations.insert_range_after(pos, pattern_length, { .renderer = rend });
			}
		}
	}
}
