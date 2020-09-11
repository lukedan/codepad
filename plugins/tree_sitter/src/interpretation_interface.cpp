// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/interpretation_interface.h"

/// \file
/// Implementation of \ref codepad::tree_sitter::interpretation_interface.

#include "details.h"

namespace codepad::tree_sitter {
	void interpretation_interface::_update_highlight() {
		_payload payload{ .interpretation = *_interp };

		TSInput input;
		input.payload = &payload;
		input.read = [](void *payload_void, uint32_t byte_index, TSPoint, uint32_t *bytes_read) {
			_payload *payload = static_cast<_payload*>(payload_void);
			auto &buf = payload->interpretation.get_buffer();

			auto byte_end = std::min<std::size_t>(byte_index + 1024, buf->length());
			*bytes_read = byte_end - byte_index;
			payload->read_buffer = buf->get_clip(buf->at(byte_index), buf->at(byte_end));
			return reinterpret_cast<const char*>(payload->read_buffer.data());
		};
		input.encoding = TSInputEncodingUTF8;

		codepad::editors::code::text_theme_data theme;
		if (_lang) {
			highlight_iterator it(input, *_interp, _parser, *_lang, [](std::u8string_view name) {
				return _details::get_language_manager().find_lanaguage(std::u8string(name));
			});
			std::size_t
				prev_pos = std::numeric_limits<std::size_t>::max(),
				prev_char_pos = std::numeric_limits<std::size_t>::max();
			codepad::editors::code::interpretation::character_position_converter pos_conv(*_interp);
			std::vector<std::size_t> event_stack;
			for (auto event = it.next(input, _parser); event; event = it.next(input, _parser)) {
				if (prev_pos != event->position) {
					std::size_t cur_char_pos = pos_conv.byte_to_character(event->position);
					codepad::assert_true_logical(
						prev_pos == std::numeric_limits<std::size_t>::max() || event->position >= prev_pos,
						"position does not monotonically increase"
					);
					if (!event_stack.empty()) {
						theme.set_range(
							prev_char_pos, cur_char_pos,
							_lang->get_highlight_configuration()->themes()[event_stack.back()]
						);
					}
					prev_pos = event->position;
					prev_char_pos = cur_char_pos;
				}
				if (event->highlight != highlight_configuration::no_associated_theme) {
					event_stack.emplace_back(event->highlight);
				} else {
					event_stack.pop_back();
				}
			}
		}
		_interp->set_text_theme(std::move(theme));
	}
}
