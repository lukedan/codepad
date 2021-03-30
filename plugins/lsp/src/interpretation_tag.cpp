// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/interpretation_tag.h"

/// \file
/// Implementation of \ref codepad::lsp::interpretation_tag.

#include <codepad/ui/elements/label.h>
#include <codepad/ui/elements/stack_panel.h>
#include <codepad/ui/elements/text_edit.h>

namespace codepad::lsp {
	hover_tooltip::hover_tooltip(interpretation_tag &p, std::size_t pos) : _parent(&p) {
		_label = _parent->get_client().get_manager().get_plugin_context().ui_man->create_element<ui::label>();

		if (_parent->get_client().get_state() == client::state::ready) {
			_label->set_text(u8"Loading...");
			auto line_col = _parent->get_interpretation().get_linebreaks().get_line_and_column_of_char(pos);
			types::HoverParams params;
			params.textDocument = _parent->get_document_identifier();
			params.position = types::Position(line_col.line, line_col.position_in_line);
			_token = _parent->get_client().send_request<types::HoverResponse>(
				u8"textDocument/hover", params,
				[this](types::HoverResponse response) {
					_handle_reply(std::move(response));
					_token = client::request_token(); // token is no longer valid
				},
				[this](types::integer code, std::u8string_view msg, const json::value_t &data) {
					if (code != static_cast<types::integer>(types::ErrorCodesEnum::ContentModified)) {
						client::default_error_handler(code, msg, data);
						_label->set_text(u8"[Error: " + std::u8string(msg) + u8"]"); // FIXME C++20 use std::format
					} else {
						_label->set_text(u8"[Modified]");
					}
					_token = client::request_token();
				}
			);
		} else {
			_label->set_text(u8"[LSP client not ready]");
		}
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
	void hover_tooltip::_handle_reply(types::HoverResponse raw_response) {
		if (std::holds_alternative<types::Hover>(raw_response.value)) {
			auto &response = std::get<types::Hover>(raw_response.value);
			if (response.range.value.has_value()) {
				// TODO
			}

			std::visit(
				[this](auto &&val) {
					_set_hover_label(*_label, val);
				},
				response.contents.value
			);
		} else {
			_label->set_text(u8"[No result]");
		}
	}


	std::unique_ptr<editors::code::tooltip> hover_tooltip_provider::request_tooltip(std::size_t pos) {
		return std::make_unique<hover_tooltip>(*_parent, pos);
	}


	std::unique_ptr<editors::code::tooltip> diagnostic_tooltip_provider::request_tooltip(std::size_t pos) {
		auto &decorations = _parent->get_diagnostic_decorations_readonly();
		auto result = decorations.decorations.find_intersecting_ranges(pos);
		if (result.begin.get_iterator() == result.end.get_iterator()) {
			return nullptr;
		}

		auto &ui_man = *_parent->get_client().get_manager().get_plugin_context().ui_man;
		auto *pnl = ui_man.create_element<ui::stack_panel>();
		for (auto it = result.begin; it.get_iterator() != result.end.get_iterator(); ) {
			auto *lbl = ui_man.create_element<ui::label>();
			lbl->set_text(std::u8string(_parent->get_message_for_diagnostic(it.get_iterator()->value.cookie)));
			pnl->children().add(*lbl);

			it = decorations.decorations.find_next_range_ending_at_or_after(pos, it);
		}
		return std::make_unique<editors::code::simple_tooltip>(*pnl);
	}


	interpretation_tag::interpretation_tag(editors::code::interpretation &interp, client &c) :
		_interp(&interp), _client(&c) {

		assert_true_usage(
			std::holds_alternative<std::filesystem::path>(_interp->get_buffer().get_id()),
			"document tags are only available for files on disk"
		);
		auto &path = std::get<std::filesystem::path>(_interp->get_buffer().get_id());

		_change_params.textDocument.uri = uri::from_current_os_path(
			std::get<std::filesystem::path>(_interp->get_buffer().get_id())
		);

		_begin_edit_token = _interp->get_buffer().begin_edit += [this](editors::buffer::begin_edit_info &info) {
			_on_begin_edit(info);
		};
		_end_modification_token = _interp->end_modification +=
			[this](editors::code::interpretation::end_modification_info &info) {
				_on_end_modification(info);
			};
		_end_edit_token = _interp->get_buffer().end_edit += [this](editors::buffer::end_edit_info &info) {
			_on_end_edit(info);
		};

		_diagnostic_decoration_token = _interp->add_decoration_provider(std::make_unique<editors::decoration_provider>());
		_hover_tooltip_token = _interp->add_tooltip_provider(std::make_unique<hover_tooltip_provider>(*this));
		_diagnostic_tooltip_token = _interp->add_tooltip_provider(
			std::make_unique<diagnostic_tooltip_provider>(*this)
		);
		_theme_token = _interp->get_theme_providers().add_provider(
			editors::code::document_theme_provider_registry::priority::accurate
		);

		// send the requests if the client is ready
		if (_client->get_state() == client::state::ready) {
			// send didOpen
			types::DidOpenTextDocumentParams didopen;
			didopen.textDocument.version = 0;
			didopen.textDocument.languageId = u8"cpp"; // TODO
			didopen.textDocument.uri = uri::from_current_os_path(path);
			// encode document as utf8
			types::string text;
			text.reserve(interp.get_buffer().length());
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
				static_cast<types::integer>(params.version.value.value()) != tag->_change_params.textDocument.version
			) {
				return;
			}

			tag->_diagnostic_messages.clear();
			{
				auto modifier = tag->_diagnostic_decoration_token.modify();
				modifier->decorations = editors::decoration_provider::registry();
				std::vector<std::u8string_view> lang_profile{ u8"cpp" }; // TODO language
				modifier->renderers = {
					tag->_client->get_manager().get_error_decoration(lang_profile.begin(), lang_profile.end()),
					tag->_client->get_manager().get_warning_decoration(lang_profile.begin(), lang_profile.end()),
					tag->_client->get_manager().get_info_decoration(lang_profile.begin(), lang_profile.end()),
					tag->_client->get_manager().get_hint_decoration(lang_profile.begin(), lang_profile.end())
				};
				for (auto &diag : params.diagnostics.value) {
					std::size_t
						beg = tag->_position_to_character(diag.range.start),
						end = tag->_position_to_character(diag.range.end);
					auto severity = types::DiagnosticSeverityEnum::Error;
					if (diag.severity.value) {
						severity = diag.severity.value.value().value;
					}

					std::size_t index = tag->_diagnostic_messages.size();
					tag->_diagnostic_messages.emplace_back(std::move(diag.message));
					editors::decoration_provider::decoration_data data;
					data.cookie = static_cast<std::int32_t>(index);
					data.renderer = modifier->renderers[static_cast<std::size_t>(severity) - 1].get();
					modifier->decorations.insert_range_after(beg, end - beg, data);
				}
			}
		}
	}

	void interpretation_tag::on_interpretation_created(
		editors::code::interpretation &interp, client &c,
		const editors::buffer_manager::interpretation_tag_token &tok
	) {
		if (std::holds_alternative<std::filesystem::path>(interp.get_buffer().get_id())) {
			tok.get_for(interp).emplace<interpretation_tag>(interp, c);
		}
	}

	void interpretation_tag::_on_end_modification(editors::code::interpretation::end_modification_info &info) {
		auto &change = _change_params.contentChanges.value.emplace_back();
		auto &range = change.range.value.emplace();

		auto [start_pos, start_cp] = _interp->get_linebreaks().get_line_and_column_and_codepoint_of_char(
			info.start_character
		);
		auto [end_pos, end_cp] = _interp->get_linebreaks().get_line_and_column_and_codepoint_of_char(
			info.start_character + info.inserted_characters
		); // end_pos here is not useful; we only need end_cp
		// start of range
		range.start = types::Position(start_pos.line, start_pos.position_in_line);
		// end of range
		range.end = types::Position(info.erase_end_line, info.erase_end_column);
		// decode new text
		std::size_t current_codepoint = start_cp;
		for (
			auto iter = _interp->codepoint_at(start_cp);
			current_codepoint < end_cp;
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
			_change_params.contentChanges.value.clear();

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
		std::unordered_map<std::uint64_t, std::optional<editors::text_theme>> theme_mapping;
		auto theme =
			_interp->get_buffer().get_buffer_manager().get_manager()->themes.get_theme_for_language(u8"cpp");
		auto get_theme_for = [&](types::uinteger type, types::uinteger mods) {
			std::uint64_t key = (static_cast<std::uint64_t>(type) << 32) | mods;
			auto [it, inserted] = theme_mapping.try_emplace(key, std::nullopt);
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
		editors::code::document_theme data;
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
				if (auto cur_theme = get_theme_for(tok.tokenType, tok.tokenModifiers)) {
					data.add_range(line_info.first_char + character_offset, token_end, cur_theme.value());
				}
			}
		);
		auto theme_modifier = _theme_token.get_modifier();
		*theme_modifier = std::move(data);
	}
}
