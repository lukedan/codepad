// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/interpretation_tag.h"

/// \file
/// Implementation of \ref codepad::lsp::interpretation_tag.

#include <codepad/ui/elements/label.h>

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

		_decoration_token = _interp->add_decoration_provider(std::make_unique<editors::decoration_provider>());

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
				[this](types::integer code, std::u8string_view msg, const json::value_t &data) {
					--_queued_highlight_version;
					client::default_error_handler(code, msg, data);
				}
			);
			++_queued_highlight_version;
		}
	}

	void interpretation_tag::on_publishDiagnostics(
		editors::code::interpretation &interp, types::PublishDiagnosticsParams params,
		const editors::buffer_manager::interpretation_tag_token &tok
	) {
		auto *tag = std::any_cast<interpretation_tag>(&tok.get_for(interp));
		if (tag) {
			if (
				params.version.value.has_value() &&
				params.version.value.value() != tag->_change_params.textDocument.version
			) {
				return;
			}
			auto &registry = tag->_decoration_token->decorations;
			registry = editors::decoration_provider::registry();
			for (auto &diag : params.diagnostics.value) {
				std::size_t
					beg = tag->_position_to_character(diag.range.start),
					end = tag->_position_to_character(diag.range.end);
				editors::decoration_provider::decoration_data data;
				data.description = std::move(diag.message);
				registry.insert_range(beg, end - beg, data);
			}
			tag->_interp->appearance_changed.invoke_noret(
				editors::code::interpretation::appearance_change_type::visual_only
			);
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

	void interpretation_tag::on_editor_created(
		editors::code::contents_region &contents, client &c,
		const editors::buffer_manager::interpretation_tag_token &tok
	) {
		auto *tag = std::any_cast<interpretation_tag>(&tok.get_for(*contents.get_document()));
		if (tag) {
			// TODO unregister for the event when the plugin is disabled
			contents.mouse_hover += [tag, &contents, &c](ui::mouse_hover_info &p) {
				if (c.get_state() == client::state::ready) {
					editors::code::caret_position caret = contents.hit_test_for_caret(p.position.get(contents));
					auto line_col = tag->_interp->get_linebreaks().get_line_and_column_of_char(caret.position);
					types::HoverParams params;
					params.textDocument = tag->_change_params.textDocument;
					params.position.line = line_col.line;
					params.position.character = line_col.position_in_line;
					c.send_request<types::HoverResponse>(
						u8"textDocument/hover", params, [tag, &contents](types::HoverResponse response) {
							logger::get().log_debug(CP_HERE) << "requesting hover popup";
							tag->_on_hover(contents, std::move(response));
						}
					);
				}
			};
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
		// update decorations
		_decoration_token->decorations.on_modification(
			info.start_character, info.removed_characters, info.inserted_characters
		);
		// since we're gonna invoke appearance_changed during end_edit anyway, no need to invoke it here
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
				[this](types::integer code, std::u8string_view msg, const json::value_t &data) {
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
		const std::vector<std::u8string> *types = nullptr, *modifiers = nullptr;
		std::visit(
			[&](const types::SemanticTokensOptions &opt) {
				types = &opt.legend.tokenTypes.value;
				modifiers = &opt.legend.tokenModifiers.value;
			},
			semantic_tokens->value
		);

		// TODO language
		// TODO theme caching
		// this is really ugly
		std::unordered_map<std::uint64_t, editors::code::text_theme_specification> theme_mapping;
		auto theme =
			_interp->get_buffer()->get_buffer_manager().get_manager()->themes.get_theme_for_language(u8"cpp");
		auto get_theme_for = [&](types::uinteger type, types::uinteger mods) {
			std::uint64_t key = (static_cast<std::uint64_t>(type) << 32) | mods;
			auto [it, inserted] = theme_mapping.try_emplace(key, editors::code::text_theme_specification());
			if (inserted) {
				std::vector<std::u8string_view> strings{ { types->at(type) } };
				std::size_t i = 0;
				while (mods > 0) {
					while ((mods & 1) == 0) {
						mods >>= 1;
						++i;
					}
					strings.emplace_back(modifiers->at(i));
					mods >>= 1;
					++i;
				}
				std::size_t v = theme->get_index_for(std::move(strings));
				if (v == editors::theme_configuration::no_associated_theme) {
					logger::get().log_warning(CP_HERE) << "no associated theme";
				} else {
					it->second = theme->entries[v].theme;
				}
			}
			return it->second;
		};

		auto &tokens = std::get<types::SemanticTokens>(response.value);
		std::size_t line = 0, character_offset = 0;
		// TODO we need a proper mechanic for multiple theme providers
		editors::code::text_theme_data data = _interp->set_text_theme(editors::code::text_theme_data());
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
				editors::code::text_theme_specification cur_theme = get_theme_for(tok.tokenType, tok.tokenModifiers);
				data.set_range(line_info.first_char + character_offset, token_end, cur_theme);
			}
		);
		_interp->set_text_theme(std::move(data));
	}

	/// Implementation for \ref _set_hover_label() with one or more \ref types::MarkedString.
	void _set_hover_label_impl(ui::label &lbl, const types::MarkedString *strs, std::size_t count) {
		std::u8string text;
		for (std::size_t i = 0; i < count; ++i) {
			if (i > 0) {
				text += u8"\n=== MY SEPARATOR ===\n";
			}
			if (std::holds_alternative<types::string>(strs[i].value)) {
				text += std::get<types::string>(strs[i].value);
			} else {
				auto &val = std::get<types::MarkedStringObject>(strs[i].value);
				text += u8"LANG: " + val.language + u8"\n";
				text += val.value;
			}
		}
		lbl.set_text(std::move(text));
	}
	/// Sets the contents of the label to the given \ref types::MarkedString.
	void _set_hover_label(ui::label &lbl, const types::MarkedString &str) {
		_set_hover_label_impl(lbl, &str, 1);
	}
	/// Sets the contents of the label to the given array of \ref types::MarkedString.
	void _set_hover_label(ui::label &lbl, const types::array<types::MarkedString> &strs) {
		_set_hover_label_impl(lbl, strs.value.data(), strs.value.size());
	}
	/// Sets the contents of the label to the given array of \ref types::MarkupContent.
	void _set_hover_label(ui::label &lbl, const types::MarkupContent &markup) {
		lbl.set_text(markup.value);
	}
	void interpretation_tag::_on_hover(editors::code::contents_region &contents, types::HoverResponse raw_response) {
		if (std::holds_alternative<types::Hover>(raw_response.value)) {
			logger::get().log_debug(CP_HERE) << "showing hover popup";
			auto &response = std::get<types::Hover>(raw_response.value);
			if (response.range.value.has_value()) {
				// TODO
			}
			auto *wnd = dynamic_cast<ui::window*>(
				contents.get_manager().create_element(u8"window", u8"hover_window")
			);
			auto *label = dynamic_cast<ui::label*>(
				contents.get_manager().create_element(u8"label", u8"hover_label")
			);
			wnd->set_width_size_policy(ui::window::size_policy::application);
			wnd->set_height_size_policy(ui::window::size_policy::application);
			wnd->set_display_border(false);
			wnd->set_display_caption_bar(false);
			wnd->set_sizable(false);
			wnd->set_show_icon(false);
			wnd->set_topmost(true);
			wnd->set_focusable(false);
			wnd->set_parent(contents.get_window());
			wnd->children().add(*label);
			std::visit(
				[label](auto &&val) {
					_set_hover_label(*label, val);
				},
				response.contents.value
			);
			wnd->show();
		}
	}
}
