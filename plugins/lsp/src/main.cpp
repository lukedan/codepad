// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Plugin interface.

#include "codepad/ui/manager.h"
#include "codepad/ui/scheduler.h"
#include "codepad/core/plugins.h"

#include "codepad/editors/manager.h"

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
#include "codepad/lsp/uri.h"

namespace codepad::lsp {
	const plugin_context *_context = nullptr; ///< The \ref plugin_context.
	plugin *_this_plugin = nullptr; ///< Handle of this plugin.
	editors::manager *_editor_manager = nullptr; ///< The \ref editors::manager.

	std::unique_ptr<client> _client; ///< The LSP client.

	/// Token used to register for \ref editors::buffer_manager::interpretation_created.
	info_event<editors::interpretation_info>::token _interpretation_created_token;
}

namespace cp = codepad;

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plug) {
		cp::lsp::_context = &ctx;
		cp::lsp::_this_plugin = &this_plug;

		auto editors_plugin = ctx.plugin_man->find_plugin(u8"editors");
		if (editors_plugin.valid()) {
			this_plug.add_dependency(editors_plugin);
			if (auto **ppman = editors_plugin.get_data<cp::editors::manager*>()) {
				cp::lsp::_editor_manager = *ppman;
			}
		}
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

		auto &handlers = cp::lsp::_client->request_handlers();
		handlers.try_emplace(
			u8"textDocument/publishDiagnostics",
			cp::lsp::client::request_handler::create_notification_handler<cp::lsp::types::PublishDiagnosticsParams>(
				[](cp::lsp::client&, cp::lsp::types::PublishDiagnosticsParams params) {
					cp::lsp::types::logger_serializer ser(cp::logger::get().log_debug(CP_HERE));
					ser.visit(params);
				}
			)
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
		cp::lsp::_client->initialize(
			init, []() {
			}
		);

		cp::lsp::_interpretation_created_token = (cp::lsp::_editor_manager->buffers.interpretation_created +=
			[](cp::editors::interpretation_info &info) {
				if (cp::lsp::_client->get_state() == cp::lsp::client::state::ready) {
					cp::lsp::_client->didOpen(info.interp);
				} else {
					// TODO
				}
			}
		);
		// TODO
	}
	PLUGIN_DISABLE() {
		cp::lsp::_editor_manager->buffers.interpretation_created -= cp::lsp::_interpretation_created_token;

		cp::lsp::_client->shutdown_and_exit();
		cp::lsp::_client.reset();
		// TODO
	}
}
