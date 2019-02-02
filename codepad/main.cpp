// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <thread>
#include <fstream>

#include "core/event.h"
#include "core/bst.h"
#include "os/current/all.h"
#include "ui/font_family.h"
#include "ui/draw.h"
#include "ui/common_elements.h"
#include "ui/native_commands.h"
#include "ui/config_parsers.h"
#include "ui/text_rendering.h"
#include "editors/tabs.h"
#include "editors/code/codebox.h"
#include "editors/code/components.h"
#include "editors/code/editor.h"
#include "editors/buffer.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editor;

int main(int argc, char **argv) {
	codepad::initialize(argc, argv);

	manager man;
	man.set_renderer(std::make_unique<opengl_renderer>());
	man.set_font_manager(std::make_unique<font_manager>(man));

	{
		ui_config_json_parser parser(man);
		parser.parse_visual_config(json::parse_file("skin/skin.json"));
		parser.parse_arrangements_config(json::parse_file("skin/arrangements.json"));
		parser.get_texture_table().load_all(man.get_renderer(), "skin/");
	}

	hotkey_json_parser::parse_config(man.get_class_hotkeys().hotkeys, json::parse_file("keys.json"));

	tab_manager tabman(man);

	auto fnt = create_font(man.get_font_manager(), CP_STRLIT("Segoe UI"), 13.0, font_style::normal);
	font_family codefnt(man.get_font_manager(), CP_STRLIT("Fira Code"), 12.0);

	content_host::set_default_font(fnt);
	code::editor::set_font(codefnt);

	auto *lbl = man.create_element<label>();
	lbl->content().set_text(CP_STRLIT("Ctrl+O to open a file"));
	tab *tmptab = tabman.new_tab();
	tmptab->set_label(CP_STRLIT("welcome"));
	tmptab->children().add(*lbl);
	tmptab->activate();

	while (!tabman.empty()) {
		man.get_scheduler().idle_loop_body();
	}

	return 0;
}
