// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of commonly used commands.

#include <deque>

#include <plugin_defs.h>

#include <codepad/core/plugins.h>

#include "codepad/editors/editor.h"
#include "codepad/editors/manager.h"
#include "codepad/editors/code/contents_region.h"
#include "codepad/editors/code/line_number_display.h"
#include "codepad/editors/code/minimap.h"
#include "codepad/editors/binary/contents_region.h"
#include "codepad/editors/binary/components.h"

namespace cp = ::codepad;

namespace codepad::editors {
	const cp::plugin_context *_context = nullptr;
	cp::plugin *_this_plugin = nullptr;

	std::unique_ptr<cp::editors::manager> _manager;

	namespace _details {
		manager &get_manager() {
			return *_manager;
		}
	}
}

using namespace codepad::editors;

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plugin) {
		_context = &ctx;
		_this_plugin = &this_plugin;

		_manager = std::make_unique<cp::editors::manager>(*_context->ui_man);
		_this_plugin->plugin_data.emplace<cp::editors::manager*>(_manager.get());

		_manager->register_builtin_interactions();
		_manager->register_builtin_decoration_renderers();
	}

	PLUGIN_FINALIZE() {
		_manager.reset();

		_this_plugin = nullptr;
		_context = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "editors";
	}

	PLUGIN_ENABLE() {
		_context->ui_man->register_element_type<cp::editors::editor>();
		_context->ui_man->register_element_type<cp::editors::code::contents_region>();
		_context->ui_man->register_element_type<cp::editors::code::line_number_display>();
		_context->ui_man->register_element_type<cp::editors::code::minimap>();
		_context->ui_man->register_element_type<cp::editors::binary::contents_region>();
		_context->ui_man->register_element_type<cp::editors::binary::primary_offset_display>();
	}

	PLUGIN_DISABLE() {
		_context->ui_man->unregister_element_type<cp::editors::editor>();
		_context->ui_man->unregister_element_type<cp::editors::code::contents_region>();
		_context->ui_man->unregister_element_type<cp::editors::code::line_number_display>();
		_context->ui_man->unregister_element_type<cp::editors::code::minimap>();
		_context->ui_man->unregister_element_type<cp::editors::binary::contents_region>();
		_context->ui_man->unregister_element_type<cp::editors::binary::primary_offset_display>();
	}
}
