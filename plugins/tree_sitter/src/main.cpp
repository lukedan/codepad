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

#include "interpretation_interface.h"

namespace cp = codepad;

extern "C" TSLanguage *tree_sitter_cpp();

namespace tree_sitter {
	const cp::plugin_context *context = nullptr; ///< The \ref cp::plugin_context.
	cp::editors::manager *editor_manager = nullptr; ///< The editor manager.

	language_configuration *config; ///< Global language configuration used for initial testing and debugging.

	/// Used to listen to \ref cp::editors::buffer_manager::interpretation_created.
	cp::info_event<cp::editors::interpretation_info>::token interpretation_created_token;
	/// Token for the interpretation tag.
	cp::editors::buffer_manager::interpretation_tag_token interpretation_tag_token;

	/// Reads the entire given file as a string.
	std::u8string read_file(const std::filesystem::path &p) {
		constexpr std::size_t _read_size = 1024000;
		std::u8string res;
		std::ifstream fin(p);
		while (fin) {
			std::size_t pos = res.size();
			res.resize(res.size() + _read_size);
			fin.read(reinterpret_cast<char*>(res.data() + pos), _read_size);
			if (fin.eof()) {
				res.resize(pos + fin.gcount());
				break;
			}
		}
		return res;
	}
}

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plugin) {
		tree_sitter::context = &ctx;

		auto editors_plugin = ctx.plugin_man->find_plugin(u8"editors");
		if (editors_plugin.valid()) {
			this_plugin.add_dependency(editors_plugin);
			if (auto **ppman = editors_plugin.get_data<cp::editors::manager*>()) {
				tree_sitter::editor_manager = *ppman;
			}
		}

		tree_sitter::config = new tree_sitter::language_configuration(tree_sitter::language_configuration::create_for(
			tree_sitter_cpp(), u8"", u8"",
			tree_sitter::read_file("plugins/tree_sitter/languages/tree-sitter-cpp/queries/highlights.scm") +
			tree_sitter::read_file("plugins/tree_sitter/languages/tree-sitter-c/queries/highlights.scm")
		));
		
		auto highlight_config = std::make_shared<tree_sitter::highlight_configuration>();
		highlight_config->set_theme_for(u8"keyword", codepad::editors::code::text_theme_specification(
			codepad::colord(0.337, 0.612, 0.839, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->set_theme_for(u8"operator", codepad::editors::code::text_theme_specification(
			codepad::colord(0.863, 0.863, 0.667, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->set_theme_for(u8"string", codepad::editors::code::text_theme_specification(
			codepad::colord(0.808, 0.569, 0.471, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->set_theme_for(u8"comment", codepad::editors::code::text_theme_specification(
			codepad::colord(0.416, 0.600, 0.333, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->set_theme_for(u8"type", codepad::editors::code::text_theme_specification(
			codepad::colord(0.306, 0.788, 0.690, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));
		highlight_config->set_theme_for(u8"number", codepad::editors::code::text_theme_specification(
			codepad::colord(0.710, 0.808, 0.659, 1.0), codepad::ui::font_style::normal, codepad::ui::font_weight::normal
		));

		tree_sitter::config->set_highlight_configuration(std::move(highlight_config));
	}

	PLUGIN_FINALIZE() {
		tree_sitter::editor_manager = nullptr;
		tree_sitter::context = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "tree_sitter";
	}

	PLUGIN_ENABLE() {
		tree_sitter::interpretation_created_token = (tree_sitter::editor_manager->buffers.interpretation_created +=
			[](cp::editors::interpretation_info &info) {
				auto &data = tree_sitter::interpretation_tag_token.get_for(info.interp);
				auto interp_interface =
					std::make_shared<tree_sitter::interpretation_interface>(info.interp, *tree_sitter::config);
				data.emplace<std::shared_ptr<tree_sitter::interpretation_interface>>(std::move(interp_interface));
			}
		);
		tree_sitter::interpretation_tag_token = tree_sitter::editor_manager->buffers.allocate_interpretation_tag();
	}
	PLUGIN_DISABLE() {
		tree_sitter::editor_manager->buffers.deallocate_interpretation_tag(tree_sitter::interpretation_tag_token);
		tree_sitter::editor_manager->buffers.interpretation_created -= tree_sitter::interpretation_created_token;
	}
}
