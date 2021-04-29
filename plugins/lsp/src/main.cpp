// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Plugin interface.

#include <codepad/core/plugins.h>
#include <codepad/ui/manager.h>
#include <codepad/ui/scheduler.h>

#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

#include "plugin_defs.h"

#include "codepad/lsp/backends/stdio.h"
#include "codepad/lsp/types/client.h"
#include "codepad/lsp/types/common.h"
#include "codepad/lsp/types/diagnostics.h"
#include "codepad/lsp/types/general.h"
#include "codepad/lsp/types/language_features.h"
#include "codepad/lsp/types/serialization.h"
#include "codepad/lsp/types/text_synchronization.h"
#include "codepad/lsp/types/window.h"
#include "codepad/lsp/types/workspace.h"
#include "codepad/lsp/backend.h"
#include "codepad/lsp/client.h"
#include "codepad/lsp/interpretation_tag.h"
#include "codepad/lsp/manager.h"
#include "codepad/lsp/uri.h"

namespace codepad::lsp {
	plugin *_this_plugin = nullptr; ///< Handle of this plugin.
	plugin_context _plugin_context; ///< Context of the host application.

	std::unique_ptr<manager> _manager; ///< The global manager.
	// TODO create per-workspace clients instead
	//      also what about multiple languages?
	std::unique_ptr<client> _client; ///< The LSP client.

	/// Token used to register for \ref editors::buffer_manager::interpretation_created.
	info_event<editors::interpretation_info>::token _interpretation_created_token;
}

namespace cp = codepad;

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plug) {
		cp::lsp::_this_plugin = &this_plug;
		cp::lsp::_plugin_context = ctx;

		auto editors_plugin = ctx.plugin_man->find_plugin(u8"editors");
		cp::editors::manager *editor_man = nullptr;
		if (editors_plugin.valid()) {
			this_plug.add_dependency(editors_plugin);
			if (auto **ppman = editors_plugin.get_data<cp::editors::manager*>()) {
				editor_man = *ppman;
			}
		}

		cp::lsp::_manager = std::make_unique<cp::lsp::manager>(ctx, *editor_man);
	}

	PLUGIN_FINALIZE() {
		cp::lsp::_manager.reset();

		cp::lsp::_this_plugin = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "lsp";
	}

	PLUGIN_ENABLE() {
		auto server_path_setting = cp::lsp::_plugin_context.sett->create_retriever_parser<std::u8string>(
			{ u8"lsp", u8"server" },
			cp::settings::basic_parsers::basic_type_with_default<
				std::u8string, cp::json::default_parser<std::u8string>
			>(u8"")
		);
		auto bkend = std::make_unique<cp::lsp::stdio_backend>(
			server_path_setting->get_main_profile().get_value(), std::vector<std::u8string_view>{}
		);
		cp::lsp::_client = std::make_unique<cp::lsp::client>(std::move(bkend), *cp::lsp::_manager);

		auto &handlers = cp::lsp::_client->request_handlers();
		handlers.try_emplace(
			u8"textDocument/publishDiagnostics",
			cp::lsp::client::request_handler::create_notification_handler<cp::lsp::types::PublishDiagnosticsParams>(
				[](cp::lsp::client& clnt, cp::lsp::types::PublishDiagnosticsParams params) {
					std::filesystem::path path = cp::lsp::uri::to_current_os_path(params.uri);
					std::shared_ptr<cp::editors::code::interpretation> doc;

					cp::lsp::_manager->get_editor_manager().buffers.for_each_interpretation_of_buffer(
						// TODO handle multiple encodings
						[&doc](const std::u8string&, std::shared_ptr<cp::editors::code::interpretation> interp) {
							if (doc) {
								cp::logger::get().log_error(CP_HERE) << "document opened using multiple encodings";
							}
							doc = std::move(interp);
						},
						path
					);
					if (!doc) {
						cp::logger::get().log_error(CP_HERE) <<
							"received diagnostics for document that's not open: " << path;
						return;
					}
					if (auto *tag = cp::lsp::_manager->get_interpretation_tag_for(*doc)) {
						tag->on_publishDiagnostics(std::move(params));
					}
				}
			)
		);

		// initialize
		cp::lsp::types::InitializeParams init;
		init.processId.value.emplace<cp::lsp::types::integer>(
			static_cast<cp::lsp::types::integer>(cp::os::process::get_current_process_id())
		);
		init.capabilities.workspace.value.emplace().workspaceFolders.value.emplace(true);
		{ // capabilities
			auto &text_document = init.capabilities.textDocument.value.emplace();
			{
				auto &semantic_tokens = text_document.semanticTokens.value.emplace();
				semantic_tokens.multilineTokenSupport.value.emplace(true);
				semantic_tokens.overlappingTokenSupport.value.emplace(true);
				semantic_tokens.requests.full.value.emplace()
					.value.emplace<cp::lsp::types::SemanticTokensFullRequestsClientCapabilities>()
					.delta.value.emplace(true);

				auto &token_types = cp::lsp::types::SemanticTokenTypes::get_strings();
				semantic_tokens.tokenTypes.value.reserve(token_types.size());
				for (std::u8string_view type : token_types) {
					semantic_tokens.tokenTypes.value.emplace_back(type);
				}

				auto &token_modifiers = cp::lsp::types::SemanticTokenModifiers::get_strings();
				semantic_tokens.tokenModifiers.value.reserve(token_modifiers.size());
				for (std::u8string_view modifier : token_modifiers) {
					semantic_tokens.tokenModifiers.value.emplace_back(modifier);
				}

				semantic_tokens.formats.value.emplace_back().value = cp::lsp::types::TokenFormatEnum::Relative;
			}
			{
				auto &hover = text_document.hover.value.emplace();
				auto &content_format = hover.contentFormat.value.emplace();
				content_format.value.emplace_back().value = cp::lsp::types::MarkupKindEnum::markdown;
				content_format.value.emplace_back().value = cp::lsp::types::MarkupKindEnum::plaintext;
			}
		}
		{
			auto &folders = init.workspaceFolders.value.emplace()
				.value.emplace<cp::lsp::types::array<cp::lsp::types::WorkspaceFolder>>().value;
			{
				auto &folder = folders.emplace_back();
				folder.uri = u8"file:///D:/Documents/Projects/codepad";
				folder.name = u8"codepad";
			}
		}
		init.capabilities.offsetEncoding.value.emplace().value.emplace_back(u8"utf-32");
		cp::lsp::_client->initialize(
			init, [](const cp::lsp::types::InitializeResult &res) {
				if (!res.offsetEncoding.value || res.offsetEncoding.value.value() != u8"utf-32") {
					cp::logger::get().log_error(CP_HERE) << "LSP server does not support UTF-32";
				}
			}
		);

		cp::lsp::_interpretation_created_token = (
			cp::lsp::_manager->get_editor_manager().buffers.interpretation_created +=
				[](cp::editors::interpretation_info &info) {
					// TODO what happens for multiple encodings?
					// create per-interpretation data
					cp::lsp::interpretation_tag::on_interpretation_created(
						info.interp, *cp::lsp::_client, cp::lsp::_manager->get_interpretation_tag_token()
					);
				}
		);
		cp::lsp::_manager->enable();
	}
	PLUGIN_DISABLE() {
		cp::lsp::_manager->disable();
		cp::lsp::_manager->get_editor_manager().buffers.interpretation_created -=
			cp::lsp::_interpretation_created_token;

		cp::lsp::_client->shutdown_and_exit();
		cp::lsp::_client.reset();
	}
}
