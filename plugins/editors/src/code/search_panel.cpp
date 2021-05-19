// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/search_panel.h"

/// \file
/// Implementation of search panels.

#include "../details.h"

namespace codepad::editors::code {
	ui::async_task_base::status search_panel::_match_task::execute() {
		using _codepoint_str = std::basic_string<codepoint>;

		std::vector<std::pair<std::size_t, std::size_t>> results;

		std::size_t pattern_length = 0;
		if (!_pattern.empty()) {
			// decode search string
			_codepoint_str pattern;
			for (auto it = _pattern.begin(); it != _pattern.end(); ) {
				codepoint cp;
				if (!encodings::utf8::next_codepoint(it, _pattern.end(), cp)) {
					logger::get().log_error(CP_HERE) << "invalid codepoint in search string: " << cp;
					cp = encodings::replacement_character;
				}
				pattern.push_back(cp);
			}
			pattern_length = pattern.size();

			if (cancelled) {
				return status::cancelled;
			}

			{ // match
				buffer::async_reader_lock lock(_parent._contents->get_document().get_buffer());

				kmp_matcher<_codepoint_str> matcher(std::move(pattern));
				kmp_matcher<_codepoint_str>::state st;
				std::size_t position = 0;
				auto it = _parent._contents->get_document().character_at(position);
				std::size_t counter = 0;
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
						results.emplace_back(position - pattern_length, position);
					}

					// check for cancellation
					if (++counter == cancellation_check_interval) {
						if (cancelled) {
							return status::cancelled;
						}
						counter = 0;
					}
				}
			}
		}

		if (cancelled) {
			return status::cancelled;
		}

		_parent._task_result_token = _parent.get_manager().get_scheduler().execute_callback(
			[results = std::move(results), parent = &_parent]() {
				parent->_update_results(std::move(results));
			}
		);

		return status::finished;
	}


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
		_clear_results();
		_cancel_task();

		// start new task
		_task_token = get_manager().get_async_task_scheduler().start_task(
			std::make_shared<_match_task>(_input->get_text(), *this)
		);
		_task_token.weaken();
	}

	void search_panel::_update_results(std::vector<std::pair<std::size_t, std::size_t>> results) {
		_results = std::move(results);
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
			for (auto [beg, end] : _results) {
				mod->decorations.insert_range_after(beg, end - beg, { .renderer = rend });
			}
		}
	}

	void search_panel::_clear_results() {
		_results.clear();
		if (_result_list) {
			static_cast<_match_result_source*>(_result_list->get_source())->on_items_changed();
		}
		{
			auto mod = _decoration_token.modify();
			mod->decorations.clear();
		}
	}

	void search_panel::_cancel_task() {
		if (auto task = _task_token.get_task()) {
			task->cancelled = true;
			task->wait_finish();
		}
		if (_task_result_token) {
			_task_result_token.cancel();
		}
	}

	void search_panel::_dispose() {
		_cancel_task();
		if (_interpretation) {
			_interpretation->get_buffer().begin_edit -= _begin_edit_token;
			_interpretation->get_buffer().end_edit -= _end_edit_token;
			_interpretation.reset();
		}
		ui::input_prompt::_dispose();
	}
}
