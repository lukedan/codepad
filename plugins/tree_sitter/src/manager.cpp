// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/tree_sitter/manager.h"

/// \file
/// Implementation of the manager.

extern "C" {
	TSLanguage *tree_sitter_c();
	TSLanguage *tree_sitter_cpp();
	TSLanguage *tree_sitter_css();
	TSLanguage *tree_sitter_html();
	TSLanguage *tree_sitter_javascript();
	TSLanguage *tree_sitter_json();
}

namespace codepad::tree_sitter {
	/// Reads the entire given file as a string.
	[[nodiscard]] std::u8string _read_file(const std::filesystem::path &p) {
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

	void manager::register_builtin_languages() {
		std::filesystem::path root = "plugins/tree_sitter/languages/";
		register_language(
			u8"c",
			std::make_shared<language_configuration>(language_configuration::create_for(
				u8"c", tree_sitter_c(),
				u8"", u8"", _read_file(root / "tree-sitter-c/queries/highlights.scm")
			))
		);
		register_language(
			u8"cpp",
			std::make_shared<language_configuration>(language_configuration::create_for(
				u8"cpp", tree_sitter_cpp(),
				u8"", u8"",
				_read_file(root / "tree-sitter-cpp/queries/highlights.scm") +
				_read_file(root / "tree-sitter-c/queries/highlights.scm")
			))
		);
		register_language(
			u8"css",
			std::make_shared<language_configuration>(language_configuration::create_for(
				u8"css", tree_sitter_css(),
				u8"", u8"",
				_read_file(root / "tree-sitter-css/queries/highlights.scm")
			))
		);
		register_language(
			u8"html",
			std::make_shared<language_configuration>(language_configuration::create_for(
				u8"html", tree_sitter_html(),
				_read_file(root / "tree-sitter-html/queries/injections.scm"),
				u8"",
				_read_file(root / "tree-sitter-html/queries/highlights.scm")
			))
		);
		{
			auto p05 = std::make_shared<language_configuration>(language_configuration::create_for(
				u8"javascript", tree_sitter_javascript(),
				_read_file(root / "tree-sitter-javascript/queries/injections.scm"),
				_read_file(root / "tree-sitter-javascript/queries/locals.scm"),
				_read_file(root / "tree-sitter-javascript/queries/highlights-jsx.scm") +
				_read_file(root / "tree-sitter-javascript/queries/highlights.scm")
			));
			register_language(u8"javascript", p05);
			register_language(u8"js", p05);
		}
		register_language(
			u8"json",
			std::make_shared<language_configuration>(language_configuration::create_for(
				u8"json", tree_sitter_json(),
				u8"", u8"", u8"" // TODO no scm files?
			))
		);
	}

	std::shared_ptr<language_configuration> manager::register_language(
		std::u8string lang, std::shared_ptr<language_configuration> config
	) {
		config->set_highlight_configuration(
			get_editor_manager().themes.get_theme_for_language(config->get_language_name())
		);
		auto [it, inserted] = _languages.try_emplace(std::move(lang), std::move(config));
		if (!inserted) {
			std::swap(config, it->second);
			return std::move(config);
		}
		return nullptr;
	}
}
