// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <thread>
#include <fstream>

#include "core/event.h"
#include "core/bst.h"
#include "os/current/all.h"
#include "os/windows/direct2d_renderer.h"
#include "ui/common_elements.h"
#include "ui/native_commands.h"
#include "ui/config_parsers.h"
#include "ui/text_rendering.h"
#include "ui/tabs/tab.h"
#include "ui/tabs/manager.h"
#include "editors/code/components.h"
#include "editors/buffer.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;
using namespace codepad::editors;

int main(int argc, char **argv) {
	codepad::initialize(argc, argv);

	manager man;
	man.set_renderer(std::make_unique<direct2d::renderer>());
	/*man.set_renderer(std::make_unique<software_renderer>());*/
	/*man.set_font_manager(std::make_unique<font_manager>(man));*/

	{
		ui_config_json_parser<json::default_engine::value_t> parser(man);
		parser.parse_arrangements_config(json::parse_file<json::default_engine::document_t>("config/arrangements.json").root().get<json::default_engine::object_t>());
	}

	hotkey_json_parser<json::default_engine::value_t>::parse_config(man.get_class_hotkeys().mapping, json::parse_file<json::default_engine::document_t>("config/keys.json").root().get<json::default_engine::object_t>());

	tabs::tab_manager tabman(man);

	/*font_family codefnt(man.get_font_manager(), CP_STRLIT("Fira Code"), 12);
	code::contents_region::set_font(codefnt);*/

	auto *lbl = man.create_element<label>();
	lbl->set_text(CP_STRLIT("Ctrl+O to open a file"));
	tabs::tab *tmptab = tabman.new_tab();
	tmptab->set_label(CP_STRLIT("Welcome"));
	tmptab->children().add(*lbl);
	tmptab->get_host()->activate_tab(*tmptab);

	while (!tabman.empty()) {
		man.get_scheduler().idle_loop_body();
	}

	return 0;
}
