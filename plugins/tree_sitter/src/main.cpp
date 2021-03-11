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

	/// Used to listen to \ref cp::editors::buffer_manager::interpretation_created.
	info_event<editors::interpretation_info>::token _interpretation_created_token;
	/// Token for the interpretation tag.
	editors::buffer_manager::interpretation_tag_token _interpretation_tag_token;

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
			*cp::tree_sitter::_context->ui_man, cp::tree_sitter::_editor_manager->themes
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
		// start the highlighting thread before any requests may be issued
		cp::tree_sitter::_manager->start_highlighter_thread();

		cp::tree_sitter::_interpretation_created_token = (
			cp::tree_sitter::_editor_manager->buffers.interpretation_created +=
				[](cp::editors::interpretation_info &info) {
					auto *lang = cp::tree_sitter::_manager->find_lanaguage(u8"cpp");

					auto &data = cp::tree_sitter::_interpretation_tag_token.get_for(info.interp);
					data.emplace<cp::tree_sitter::interpretation_tag>(info.interp, lang);
				}
		);
		cp::tree_sitter::_interpretation_tag_token =
			cp::tree_sitter::_editor_manager->buffers.allocate_interpretation_tag();
	}
	PLUGIN_DISABLE() {
		cp::tree_sitter::_editor_manager->buffers.deallocate_interpretation_tag(
			cp::tree_sitter::_interpretation_tag_token
		);
		cp::tree_sitter::_editor_manager->buffers.interpretation_created -=
			cp::tree_sitter::_interpretation_created_token;

		// stop the highlighting thread after all requests have been cancelled
		cp::tree_sitter::_manager->stop_highlighter_thread();
	}
}
