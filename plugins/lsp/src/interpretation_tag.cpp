// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/interpretation_tag.h"

/// \file
/// Implementation of \ref codepad::lsp::interpretation_tag.

namespace codepad::lsp {
	interpretation_tag::interpretation_tag(editors::code::interpretation &interp, client &c) :
		_interp(&interp), _client(&c) {

		assert_true_usage(
			std::holds_alternative<std::filesystem::path>(_interp->get_buffer()->get_id()),
			"document tags are only available for files on disk"
		);
		auto &path = std::get<std::filesystem::path>(_interp->get_buffer()->get_id());

		_change_params.textDocument.uri = uri::from_current_os_path(
			std::get<std::filesystem::path>(_interp->get_buffer()->get_id())
		);

		_begin_edit_token = _interp->get_buffer()->begin_edit += [this](editors::buffer::begin_edit_info &info) {
			_on_begin_edit(info);
		};
		_modification_decoded_token = _interp->modification_decoded +=
			[this](editors::code::interpretation::modification_decoded_info &info) {
				_on_modification_decoded(info);
			};
		_end_modification_token = _interp->end_modification +=
			[this](editors::code::interpretation::end_modification_info &info) {
				_on_end_modification(info);
			};
		_end_edit_token = _interp->get_buffer()->end_edit += [this](editors::buffer::end_edit_info &info) {
			_on_end_edit(info);
		};

		// send the requests if the client is ready
		if (_client->get_state() == client::state::ready) {
			// send didOpen
			types::DidOpenTextDocumentParams didopen;
			didopen.textDocument.version = 0;
			didopen.textDocument.languageId = u8"cpp"; // TODO
			didopen.textDocument.uri = uri::from_current_os_path(path);
			// encode document as utf8
			types::string text;
			text.reserve(interp.get_buffer()->length());
			for (auto iter = interp.codepoint_begin(); !iter.ended(); iter.next()) {
				codepoint cp = iter.is_codepoint_valid() ? iter.get_codepoint() : encodings::replacement_character;
				auto str = encodings::utf8::encode_codepoint(cp);
				text += std::u8string_view(reinterpret_cast<const char8_t*>(str.data()), str.size());
			}
			didopen.textDocument.text = std::move(text);
			_client->send_notification(u8"textDocument/didOpen", didopen);


			// send semanticTokens
			types::SemanticTokensParams params;
			params.textDocument.uri = _change_params.textDocument.uri;
			_client->send_request<types::SemanticTokensResponse>(
				u8"textDocument/semanticTokens/full", params, [this](types::SemanticTokensResponse params) {
					_on_semanticTokens(std::move(params));
				},
				[this](types::integer code, std::u8string_view msg, const rapidjson::Value *data) {
					--_queued_highlight_version;
					client::default_error_handler(code, msg, data);
				}
			);
			++_queued_highlight_version;
		}
	}

	void interpretation_tag::on_interpretation_created(
		editors::code::interpretation &interp, client &c,
		const editors::buffer_manager::interpretation_tag_token &tok
	) {
		if (std::holds_alternative<std::filesystem::path>(interp.get_buffer()->get_id())) {
			tok.get_for(interp).emplace<interpretation_tag>(interp, c);
		}
	}

	void interpretation_tag::_on_modification_decoded(
		editors::code::interpretation::modification_decoded_info &info
	) {
		auto &change = _change_params.contentChanges.value.emplace_back();
		auto &range = change.range.value.emplace();
		// start of range
		range.start.line = info.start_line_column.line;
		range.start.character = info.start_line_column.position_in_line;
		auto start_line = *info.start_line_column.line_iterator;
		if (range.start.character > start_line.nonbreak_chars) {
			_change_start_offset =
				info.start_line_column.position_in_line - start_line.nonbreak_chars;
			range.start.character = start_line.nonbreak_chars;
		} else {
			_change_start_offset = 0;
		}
		// end of range
		range.end.line = info.past_end_line_column.line;
		range.end.character = info.past_end_line_column.position_in_line;
		auto end_line = *info.past_end_line_column.line_iterator;
		if (range.end.character > end_line.nonbreak_chars) {
			_change_end_offset =
				end_line.nonbreak_chars + ui::get_line_ending_length(end_line.ending) -
				info.past_end_line_column.position_in_line;
			++range.end.line;
			range.end.character = 0;
		} else {
			_change_end_offset = 0;
		}
	}

	void interpretation_tag::_on_end_modification(editors::code::interpretation::end_modification_info &info) {
		auto &change = _change_params.contentChanges.value.back();
		// decode new text
		change.text.clear();
		std::size_t
			current_codepoint = info.start_codepoint - _change_start_offset,
			past_end_codepoint = info.past_end_codepoint + _change_end_offset;
		for (
			auto iter = _interp->codepoint_at(current_codepoint);
			current_codepoint < past_end_codepoint;
			iter.next(), ++current_codepoint
			) {
			codepoint cp =
				iter.is_codepoint_valid() ?
				iter.get_codepoint() :
				encodings::replacement_character;
			editors::byte_string str = encodings::utf8::encode_codepoint(cp);
			change.text.append(std::u8string_view(
				reinterpret_cast<const char8_t*>(str.data()), str.size()
			));
		}
	}

	void interpretation_tag::_on_end_edit(editors::buffer::end_edit_info&) {
		if (_client->get_state() == client::state::ready) {
			++_change_params.textDocument.version;
			_client->send_notification(u8"textDocument/didChange", _change_params);

			types::SemanticTokensParams params;
			params.textDocument.uri = _change_params.textDocument.uri;
			_client->send_request<types::SemanticTokensResponse>(
				u8"textDocument/semanticTokens/full", params, [this](types::SemanticTokensResponse params) {
					_on_semanticTokens(std::move(params));
				},
				[this](types::integer code, std::u8string_view msg, const rapidjson::Value *data) {
					--_queued_highlight_version;
					// ignore errors caused by content modifications
					if (code != static_cast<types::integer>(types::ErrorCodesEnum::ContentModified)) {
						client::default_error_handler(code, msg, data);
					}
				}
			);
			++_queued_highlight_version;
		}
	}

	void interpretation_tag::_on_semanticTokens(types::SemanticTokensResponse response) {
		performance_monitor mon(u8"semanticTokens", std::chrono::milliseconds(40));
		if (--_queued_highlight_version != 0) {
			// skip; highlight is not for the latest document
			return;
		}
		if (std::holds_alternative<types::null>(response.value)) {
			// TODO handle null response
			return;
		};
		auto &semantic_tokens = _client->get_initialize_result().capabilities.semanticTokensProvider.value;
		if (!semantic_tokens.has_value()) {
			return;
		}
		std::vector<std::u8string> types;
		std::visit(
			[&](const types::SemanticTokensOptions &opt) {
				types = opt.legend.tokenTypes.value;
			},
			semantic_tokens->value
		);

		auto &tokens = std::get<types::SemanticTokens>(response.value);
		std::size_t line = 0, character_offset = 0;
		editors::code::text_theme_data data;
		_interp->swap_text_theme(data);
		auto &linebreaks = _interp->get_linebreaks();
		editors::code::linebreak_registry::linebreak_info line_info = linebreaks.get_line_info(0);
		_semantic_token::iterate_over_range(
			tokens.data.value, [&](const _semantic_token &tok) {
				// update current position
				if (tok.deltaLine > 0) {
					line += tok.deltaLine;
					character_offset = tok.deltaStart;
					line_info = linebreaks.get_line_info(line);
				} else {
					character_offset += tok.deltaStart;
				}
				// check if the token spans multiple lines
				std::size_t token_end = line_info.first_char + character_offset + tok.length;
				std::size_t line_end =
					line_info.first_char + line_info.entry->nonbreak_chars +
					(line_info.entry->ending != ui::line_ending::none ? 1 : 0);
				if (token_end > line_end) {
					std::size_t codepoint =
						linebreaks.get_beginning_codepoint_of(line_info.entry) +
						character_offset + tok.length;
					token_end = linebreaks.get_line_and_column_and_char_of_codepoint(codepoint).second;
				}
				// get theme from token type
				editors::code::text_theme_specification spec;
				// TODO
				if (types[tok.tokenType] == u8"comment") {
					spec.color = colord(0.0, 1.0, 0.0, 1.0);
				} else {
					spec.color = colord(1.0, 0.0, 0.0, 1.0);
				}
				data.set_range(line_info.first_char + character_offset, token_end, spec);
			}
		);
		_interp->swap_text_theme(data);
	}
}
