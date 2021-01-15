// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Plugin interface.

#include "codepad/ui/manager.h"
#include "codepad/ui/scheduler.h"
#include "codepad/core/plugins.h"

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
#include "codepad/lsp/messages/general.h"
#include "codepad/lsp/backend.h"
#include "codepad/lsp/client.h"

namespace codepad::lsp {
	const plugin_context *_context = nullptr; ///< The \ref plugin_context.
	plugin *_this_plugin = nullptr; ///< Handle of this plugin.
	std::unique_ptr<client> _client;
}

namespace cp = codepad;

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plug) {
		cp::lsp::_context = &ctx;
		cp::lsp::_this_plugin = &this_plug;

		/*auto editors_plugin = ctx.plugin_man->find_plugin(u8"editors");
		if (editors_plugin.valid()) {
			this_plug.add_dependency(editors_plugin);
		}*/
	}

	PLUGIN_FINALIZE() {
		cp::lsp::_context = nullptr;
		cp::lsp::_this_plugin = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "lsp";
	}

	PLUGIN_ENABLE() {
		auto bkend = std::make_unique<cp::lsp::stdio_backend>(TEXT("D:/Software/LLVM/bin/clangd.exe"));
		cp::lsp::_client = std::make_unique<cp::lsp::client>(
			std::move(bkend), cp::lsp::_context->ui_man->get_scheduler()
		);

		// initialize
		cp::lsp::types::InitializeParams init;
		init.processId.value.emplace<cp::lsp::types::integer>(GetCurrentProcessId()); // TODO winapi
		init.capabilities.workspace.value.emplace().workspaceFolders.value.emplace(true);
		{
			auto &folders = init.workspaceFolders.value.emplace()
				.value.emplace<cp::lsp::types::array<cp::lsp::types::WorkspaceFolder>>().value;
			{
				auto &folder = folders.emplace_back();
				folder.uri = u8"file:///D:/Documents/Projects/codepad";
				folder.name = u8"codepad";
			}
		}
		cp::lsp::_client->send_request<cp::lsp::types::InitializeResult>(
			u8"initialize", init, [](cp::lsp::types::InitializeResult res) {
				cp::lsp::types::InitializedParams initialized;
				cp::lsp::_client->send_notification(u8"initialized", initialized);

				cp::lsp::types::SetTraceParams trace;
				trace.value.value = cp::lsp::types::TraceValueEnum::verbose;
				cp::lsp::_client->send_notification(u8"$/setTrace", trace);

				cp::lsp::types::DidOpenTextDocumentParams didopen;
				didopen.textDocument.languageId = u8"cpp";
				didopen.textDocument.uri = u8"file:///D:/Documents/Projects/codepad/src/core/globals.cpp";
				didopen.textDocument.version = 0;
				cp::lsp::_client->send_notification(u8"textDocument/didOpen", didopen);

				cp::lsp::types::CompletionParams completion;
				completion.position.line = 0;
				completion.position.character = 0;
				completion.textDocument.uri = u8"file:///D:/Documents/Projects/codepad/src/core/globals.cpp";
				cp::lsp::_client->send_request<cp::lsp::types::CompletionResponse>(
					u8"textDocument/completion", completion, [](cp::lsp::types::CompletionResponse resp) {
						cp::lsp::types::logger_serializer log(cp::logger::get().log_debug(CP_HERE));
						log.visit(resp);
					}
				);
			}
		);
		// TODO
	}
	PLUGIN_DISABLE() {
		// TODO
	}
}
