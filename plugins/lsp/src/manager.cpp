// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/manager.h"

/// \file
/// Implementation of manager functions.

#include "codepad/lsp/interpretation_tag.h"

namespace codepad::lsp {
	manager::manager(const plugin_context &context, editors::manager &editor_man) :
		_plugin_context(context), _editor_manager(editor_man) {

		_error_decoration =
			_plugin_context.ui_man->get_settings().create_retriever_parser<decoration_renderer_ptr>(
				{ u8"lsp", u8"error_decoration" },
				editors::decoration_renderer::create_setting_parser(*_plugin_context.ui_man, editor_man)
			);
		_warning_decoration =
			_plugin_context.ui_man->get_settings().create_retriever_parser<decoration_renderer_ptr>(
				{ u8"lsp", u8"warning_decoration" },
				editors::decoration_renderer::create_setting_parser(*_plugin_context.ui_man, editor_man)
			);
		_info_decoration =
			_plugin_context.ui_man->get_settings().create_retriever_parser<decoration_renderer_ptr>(
				{ u8"lsp", u8"info_decoration" },
				editors::decoration_renderer::create_setting_parser(*_plugin_context.ui_man, editor_man)
			);
		_hint_decoration =
			_plugin_context.ui_man->get_settings().create_retriever_parser<decoration_renderer_ptr>(
				{ u8"lsp", u8"hint_decoration" },
				editors::decoration_renderer::create_setting_parser(*_plugin_context.ui_man, editor_man)
			);
	}

	interpretation_tag *manager::get_interpretation_tag_for(editors::code::interpretation &interp) const {
		return std::any_cast<interpretation_tag>(&_interpretation_tag_token.get_for(interp));
	}
}
