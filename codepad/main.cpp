// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <iostream>

#include "core/event.h"
#include "core/bst.h"
#include "core/logger_sinks.h"
#include "core/json/parsing.h"
#include "core/json/rapidjson.h"
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
	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("codepad.log"));
	logger::set_current(std::move(global_log));

	codepad::initialize(argc, argv);

	manager man;
	man.set_renderer(std::make_unique<direct2d::renderer>());

	{
		json::rapidjson::document_t doc = json::parse_file<json::rapidjson::document_t>("config/arrangements.json");
		auto val = json::parsing::make_value(doc.root());
		arrangements_parser<decltype(val)> parser(man);
		parser.parse_arrangements_config(val.get<decltype(val)::object_t>());
	}

	hotkey_json_parser<json::rapidjson::value_t>::parse_config(
		man.get_class_hotkeys().mapping,
		json::parse_file<json::rapidjson::document_t>("config/keys.json").root().get<json::rapidjson::object_t>()
	);

	tabs::tab_manager tabman(man);

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
