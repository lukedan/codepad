// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the \ref tree_sitter::interface class.

#include <tree_sitter/api.h>

#include <codepad/editors/code/interpretation.h>

#include "language_configuration.h"
#include "highlight_iterator.h"

namespace tree_sitter {
	/// Interface between the editor and \p tree-sitter.
	class interpretation_interface {
	public:
		/// Constructor.
		interpretation_interface(
			codepad::editors::code::interpretation &interp, const language_configuration &config
		) : _interp(&interp), _lang(&config) {
			// create parser
			_parser.set(ts_parser_new());

			// this object is owned by the interpretation and will be disposed when it's disposed
			// so no need to unregister
			interp.end_edit_interpret += [this](codepad::editors::buffer::end_edit_info&) {
				_update_highlight();
			};
			_update_highlight();
		}
	protected:
		parser_ptr _parser; ///< The parser.
		codepad::editors::code::interpretation *_interp = nullptr; ///< The associated interpretation.
		const language_configuration *_lang = nullptr; ///< The language configuration.

		/// Contains information used in a \p TSInput.
		struct _payload {
			codepad::editors::byte_string read_buffer;
			codepad::editors::code::interpretation &interpretation; ///< The interpretation.
		};

		/// Updates the highlight of \ref _interp.
		void _update_highlight() {
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

			highlight_iterator it(input, *_interp, _parser, *_lang, [](std::u8string_view) {
				return nullptr; // TODO
			});

			std::size_t prev_pos = std::numeric_limits<std::size_t>::max(), prev_char_pos;
			codepad::editors::code::interpretation::character_position_converter pos_conv(*_interp);
			std::vector<std::size_t> event_stack;
			codepad::editors::code::text_theme_data theme;
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
			_interp->set_text_theme(std::move(theme));
		}
	};
}
