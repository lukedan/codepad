// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <iostream>

#include "core/logging.h"
#include "core/logger_sinks.h"
#include "core/plugin_interface.h"
#include "core/plugins.h"
#include "core/settings.h"
#include "core/json/parsing.h"
#include "core/json/rapidjson.h"
#include "os/current/all.h"
#ifdef CP_PLATFORM_WINDOWS
#   include "os/windows/direct2d_renderer.h"
#   include "os/windows/cairo_renderer.h"
#elif defined(CP_PLATFORM_UNIX)
#   include "os/linux/gtk/cairo_renderer.h"
#endif
#include "ui/cairo_renderer_base.h"
#include "ui/native_commands.h"
#include "ui/config_parsers.h"
#include "ui/elements/label.h"
#include "ui/elements/tabs/tab.h"
#include "ui/elements/tabs/manager.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;

int main(int argc, char **argv) {
	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("codepad.log"));
	logger::set_current(std::move(global_log));

	codepad::initialize(argc, argv);

	settings sett;
	global_settings = &sett;

	manager man(sett);
	global_manager = &man;

	sett.load("config/settings.json");

#ifdef CP_ENABLE_PLUGINS
	{ // load plugins
		auto parser = sett.create_retriever_parser<std::vector<std::u8string_view>>(
			{ u8"native_plugins" },
			settings::basic_parsers::basic_type_with_default<std::vector<std::u8string_view>>(
				{}, json::array_parser<std::u8string_view>()
				)
			);
		auto value = parser.get_main_profile().get_value();
		for (std::u8string_view p : value) {
			auto plugin = std::make_unique<native_plugin>(std::filesystem::path(p));
			if (!plugin->valid()) {
				logger::get().log_warning(CP_HERE) << "failed to load plugin " << p;
				plugin->diagnose();
				continue;
			}
			plugin_manager::get().attach(std::move(plugin)).enable();
			logger::get().log_info(CP_HERE) << "plugin loaded: " << p;
		}
	}
#endif

	{ // select renderer
		std::u8string_view default_graphics_backend =
#ifdef CP_PLATFORM_WINDOWS
			u8"direct2d";
#elif defined(CP_PLATFORM_UNIX)
			u8"cairo";
#endif
		auto parser = sett.create_retriever_parser<std::u8string_view>(
			{ u8"graphics_backend" },
			settings::basic_parsers::basic_type_with_default<std::u8string_view>(default_graphics_backend)
			);
		std::u8string_view renderer = parser.get_main_profile().get_value();
		logger::get().log_debug(CP_HERE) << "using renderer: " << renderer;
#ifdef CP_PLATFORM_WINDOWS
		if (renderer == u8"direct2d") {
			man.set_renderer(std::make_unique<direct2d::renderer>());
		}
#endif
#ifdef CP_USE_CAIRO
		if (renderer == u8"cairo") {
			man.set_renderer(std::make_unique<cairo_renderer>());
		}
#endif
		assert_true_usage(man.has_renderer(), "unrecognized renderer");
	}

	{
		auto doc = json::parse_file<json::rapidjson::document_t>("config/arrangements.json");
		auto val = json::parsing::make_value(doc.root());
		arrangements_parser<decltype(val)> parser(man);
		parser.parse_arrangements_config(val.get<decltype(val)::object_type>());
	}

	hotkey_json_parser<json::rapidjson::value_t>::parse_config(
		man.get_class_hotkeys().mapping,
		json::parse_file<json::rapidjson::document_t>("config/keys.json").root().get<json::rapidjson::object_t>()
	);

	tabs::tab_manager tabman(man);

	auto *lbl = man.create_element<label>();
	lbl->set_text(u8"Ctrl+O to open a file");
	tabs::tab *tmptab = tabman.new_tab();
	tmptab->set_label(u8"Welcome");
	tmptab->children().add(*lbl);
	tmptab->get_host()->activate_tab(*tmptab);

	while (!tabman.empty()) {
		man.get_scheduler().main_iteration();
	}

	return 0;
}
