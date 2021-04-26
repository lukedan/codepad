// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/interpretation_tag.h"

/// \file
/// Implementation of \ref codepad::tree_sitter::interpretation_tag.

#include <codepad/ui/elements/label.h>

#include "details.h"

namespace codepad::tree_sitter {
	std::unique_ptr<editors::code::tooltip> highlight_debug_tooltip_provider::request_tooltip(std::size_t pos) {
		auto &highlight_ranges = _parent->get_highlight().ranges;
		auto result = highlight_ranges.find_intersecting_ranges(pos);
		if (result.begin.get_iterator() == result.end.get_iterator()) {
			return nullptr;
		}

		auto &manager = _parent->get_manager().get_manager();
		auto &names = _parent->get_capture_names();
		auto *pnl = manager.create_element<ui::stack_panel>();
		pnl->set_orientation(ui::orientation::vertical);
		for (auto it = result.begin; it.get_iterator() != result.end.get_iterator(); ) {
			auto *label = manager.create_element<ui::label>();
			label->set_text(names[static_cast<std::size_t>(it.get_iterator()->value.cookie)]);
			pnl->children().add(*label);

			it = highlight_ranges.find_next_range_ending_at_or_after(pos, it);
		}
		return std::make_unique<editors::code::simple_tooltip>(*pnl);
	}


	ui::async_task_base::status interpretation_tag::_highlight_task::execute() {
		highlight_collector::document_highlight_data theme;
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
						{
							auto mod = tag->_theme_token.get_modifier();
							*mod = std::move(t.theme);
						}
						tag->_capture_names = std::move(t.capture_names);
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
		_lang_changed_token = _interp->get_buffer().language_changed +=
			[this](value_update_info<std::vector<std::u8string>, value_update_info_contents::old_value>&) {
				if (auto task = _task_token.get_task()) {
					task->cancel();
					task->wait_finish();
				}
				_lang = _manager->find_lanaguage(_interp->get_buffer().get_language().back());
				start_highlight_task();
			};

		_theme_token = _interp->get_theme_providers().add_provider(
			editors::code::document_theme_provider_registry::priority::approximate
		);
		_debug_tooltip_provider_token = _interp->add_tooltip_provider(
			std::make_unique<highlight_debug_tooltip_provider>(*this)
		);

		start_highlight_task();
	}

	highlight_collector::document_highlight_data interpretation_tag::compute_highlight(std::size_t *cancel_tok) {
		if (!_lang) {
			return highlight_collector::document_highlight_data();
		}

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

		highlight_collector collector(
			input, *_interp, _parser, *_lang, [](std::u8string_view name) {
				return _details::get_manager().find_lanaguage(std::u8string(name));
			},
			cancel_tok
		);
		return collector.compute(_parser);
	}

	void interpretation_tag::start_highlight_task() {
		auto task = std::make_shared<_highlight_task>(*this);
		_task_token = get_manager().get_manager().get_async_task_scheduler().start_task(std::move(task));
		_task_token.weaken(); // so that there's no cyclic dependency
	}
}
