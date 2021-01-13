// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Plugin interface.

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
		cp::lsp::_client = std::make_unique<cp::lsp::client>(std::move(bkend));
		// TODO
	}
	PLUGIN_DISABLE() {
		// TODO
	}
}
