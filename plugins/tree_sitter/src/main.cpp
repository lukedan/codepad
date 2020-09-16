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

#include "codepad/tree_sitter/interpretation_interface.h"

namespace codepad::tree_sitter {
	const plugin_context *_context = nullptr; ///< The \ref cp::plugin_context.
	editors::manager *_editor_manager = nullptr; ///< The editor manager.

	std::unique_ptr<language_manager> _manager; ///< Global language manager.

	/// Used to listen to \ref cp::editors::buffer_manager::interpretation_created.
	info_event<editors::interpretation_info>::token _interpretation_created_token;
	/// Token for the interpretation tag.
	editors::buffer_manager::interpretation_tag_token _interpretation_tag_token;

	namespace _details {
		language_manager &get_language_manager() {
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

		cp::tree_sitter::_manager = std::make_unique<cp::tree_sitter::language_manager>();
		cp::tree_sitter::_manager->register_builtin_languages();

		auto highlight_config = std::make_shared<cp::tree_sitter::highlight_configuration>();
		highlight_config->add_entry(u8"keyword", codepad::editors::code::text_theme_specification(
			codepad::colord(0.337, 0.612, 0.839, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"operator", codepad::editors::code::text_theme_specification(
			codepad::colord(0.863, 0.863, 0.667, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"string", codepad::editors::code::text_theme_specification(
			codepad::colord(0.808, 0.569, 0.471, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"comment", codepad::editors::code::text_theme_specification(
			codepad::colord(0.416, 0.600, 0.333, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"type", codepad::editors::code::text_theme_specification(
			codepad::colord(0.306, 0.788, 0.690, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"number", codepad::editors::code::text_theme_specification(
			codepad::colord(0.710, 0.808, 0.659, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->add_entry(u8"tag", codepad::editors::code::text_theme_specification(
			codepad::colord(0.337, 0.612, 0.839, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		cp::tree_sitter::_manager->set_highlight_configuration(std::move(highlight_config));
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
		cp::tree_sitter::_interpretation_created_token = (
			cp::tree_sitter::_editor_manager->buffers.interpretation_created +=
				[](cp::editors::interpretation_info &info) {
					auto *lang = cp::tree_sitter::_manager->find_lanaguage(u8"cpp");

					auto &data = cp::tree_sitter::_interpretation_tag_token.get_for(info.interp);
					auto interp_interface =
						std::make_shared<cp::tree_sitter::interpretation_interface>(info.interp, lang);
					data.emplace<std::shared_ptr<cp::tree_sitter::interpretation_interface>>(
						std::move(interp_interface)
					);
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
	}
}
