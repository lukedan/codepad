// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/interpretation_tag.h"

/// \file
/// Implementation of \ref codepad::tree_sitter::interpretation_tag.

#include "details.h"

namespace codepad::tree_sitter {
	ui::async_task_base::status interpretation_tag::_highlight_task::execute() {
		editors::code::text_theme_data theme;
		{
			editors::buffer::async_reader_lock lock(_interp->get_buffer());
			theme = _tag.compute_highlight(&_cancellation_token);
		}
		// transfer the highlight results back to the main thread
		std::atomic_ref<std::size_t> cancel(_cancellation_token);
		if (cancel == 0) {
			manager *man = &_tag.get_manager();
			_tag.get_manager().get_manager().get_scheduler().execute_callback(
				[t = std::move(theme), target = std::move(_interp), man]() mutable {
					if (auto *tag = man->get_tag_for(*target)) {
						auto mod = tag->_theme_token.get_modifier();
						*mod = std::move(t);
					}
					// the interpretation_tag can be empty if the plugin is disabled after this task has finished,
					// but before the callback is executed
				}
			);
			return status::finished;
		}
		return status::cancelled;
	}


	interpretation_tag::interpretation_tag(
		editors::code::interpretation &interp, const language_configuration *config, manager &man
	) : _lang(config), _interp(&interp), _manager(&man) {
		// create parser
		_parser.set(ts_parser_new());

		_begin_edit_token = _interp->get_buffer().begin_edit += [this](editors::buffer::begin_edit_info&) {
			if (auto task = _task_token.get_task()) {
				task->cancel();
				// ensure that there's only one instance of the task running at any given time, so that when this tag
				// is removed, there won't be a task that still uses this tag and the language
				task->wait_finish();
			}
		};
		_end_edit_token = _interp->get_buffer().end_edit += [this](editors::buffer::end_edit_info&) {
			start_highlight_task();
		};

		_theme_token = _interp->get_theme_providers().add_provider(
			editors::code::text_theme_provider_registry::priority::approximate
		);

		start_highlight_task();
	}

	editors::code::text_theme_data interpretation_tag::compute_highlight(std::size_t *cancel_tok) {
		editors::code::text_theme_data theme;
		if (_lang) {
			_payload payload{ .interpretation = *_interp };
			TSInput input;
			input.payload = &payload;
			input.read = [](void *payload_void, uint32_t byte_index, TSPoint, uint32_t *bytes_read) {
				_payload *payload = static_cast<_payload*>(payload_void);

				auto byte_end =
					std::min<std::size_t>(byte_index + 1024, payload->interpretation.get_buffer().length());
				*bytes_read = byte_end - byte_index;
				payload->read_buffer = payload->interpretation.get_buffer().get_clip(
					payload->interpretation.get_buffer().at(byte_index),
					payload->interpretation.get_buffer().at(byte_end)
				);
				return reinterpret_cast<const char*>(payload->read_buffer.data());
			};
			input.encoding = TSInputEncodingUTF8;

			highlight_iterator it(
				input, *_interp, _parser, *_lang, [](std::u8string_view name) {
					return _details::get_manager().find_lanaguage(std::u8string(name));
				},
				cancel_tok
			);
			std::size_t
				prev_pos = std::numeric_limits<std::size_t>::max(),
				prev_char_pos = std::numeric_limits<std::size_t>::max();
			editors::code::interpretation::character_position_converter pos_conv(*_interp);
			std::vector<std::size_t> event_stack;
			for (auto event = it.next(input, _parser); event; event = it.next(input, _parser)) {
				if (prev_pos != event->position) {
					std::size_t cur_char_pos = pos_conv.byte_to_character(event->position);
					assert_true_logical(
						prev_pos == std::numeric_limits<std::size_t>::max() || event->position >= prev_pos,
						"position does not monotonically increase"
					);
					if (!event_stack.empty()) {
						theme.add_range(
							prev_char_pos, cur_char_pos,
							_lang->get_highlight_configuration()->entries[event_stack.back()].theme
						);
					}
					prev_pos = event->position;
					prev_char_pos = cur_char_pos;
				}
				if (event->highlight != editors::theme_configuration::no_associated_theme) {
					event_stack.emplace_back(event->highlight);
				} else {
					event_stack.pop_back();
				}
			}
		}
		return theme;
	}

	void interpretation_tag::start_highlight_task() {
		auto task = std::make_shared<_highlight_task>(*this);
		_task_token = get_manager().get_manager().get_async_task_scheduler().start_task(std::move(task));
		_task_token.weaken(); // so that there's no cyclic dependency
	}
}
