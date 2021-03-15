// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Main source of the tree-sitter plugin.

#include "plugin_defs.h"

#include <filesystem>

#include <tree_sitter/api.h>

#include <codepad/core/plugins.h>
#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

#include "codepad/tree_sitter/interpretation_tag.h"
#include "codepad/tree_sitter/manager.h"

namespace codepad::tree_sitter {
	const plugin_context *_context = nullptr; ///< The \ref cp::plugin_context.
	editors::manager *_editor_manager = nullptr; ///< The editor manager.

	std::unique_ptr<manager> _manager; ///< Global language manager.

	namespace _details {
		manager &get_manager() {
			return *_manager;
		}
	}
}

namespace cp = codepad;

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plugin) {
		cp::tree_sitter::_context = &ctx;

		auto editors_plugin = ctx.plugin_man->find_plugin(u8"editors");
		if (editors_plugin.valid()) {
			this_plugin.add_dependency(editors_plugin);
			if (auto **ppman = editors_plugin.get_data<cp::editors::manager*>()) {
				cp::tree_sitter::_editor_manager = *ppman;
			}
		}

		cp::tree_sitter::_manager = std::make_unique<cp::tree_sitter::manager>(
			*cp::tree_sitter::_context->ui_man, *cp::tree_sitter::_editor_manager
		);
		cp::tree_sitter::_manager->register_builtin_languages();
	}

	PLUGIN_FINALIZE() {
		cp::tree_sitter::_manager.reset();

		cp::tree_sitter::_editor_manager = nullptr;
		cp::tree_sitter::_context = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "tree_sitter";
	}

	PLUGIN_ENABLE() {
		cp::tree_sitter::_manager->enable();
	}
	PLUGIN_DISABLE() {
		cp::tree_sitter::_manager->disable();
	}
}
