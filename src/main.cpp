﻿// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/logging.h"
#include "codepad/core/logger_sinks.h"
#include "codepad/core/plugins.h"
#include "codepad/core/settings.h"
#include "codepad/core/json/parsing.h"
#include "codepad/core/json/rapidjson.h"
#include "codepad/os/current/all.h"
#ifdef CP_PLATFORM_WINDOWS
#   include "codepad/os/windows/direct2d_renderer.h"
#   include "codepad/os/windows/cairo_renderer.h"
#elif defined(CP_PLATFORM_UNIX)
#   include "codepad/os/linux/gtk/cairo_renderer.h"
#endif
#include "codepad/ui/cairo_renderer_base.h"
#include "codepad/ui/config_parsers.h"
#include "codepad/ui/json_parsers.h"
#include "codepad/ui/elements/tabs/tab.h"
#include "codepad/ui/elements/tabs/manager.h"

#include "codepad/ui/elements/stack_panel.h"
#include "codepad/ui/elements/label.h"
#include "codepad/ui/elements/text_edit.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;

int main(int argc, char **argv) {
	// startup
	// create logger
	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("codepad.log"));
	logger::set_current(std::move(global_log));

	// call initialize
	codepad::initialize(argc, argv);

	settings sett;
	plugin_manager plugman;

	{ // this scope here is used to ensure the order of destruction of managers
		// create settings, plugin manager, ui manager, tab manager
		manager man(sett);
		tabs::tab_manager tabman(man);

		plugin_context context{ &sett, &plugman, &man, &tabman };
		plugman.context = &context;

		// load settings
		sett.load("config/settings.json");

		{ // load plugins
			auto parser = sett.create_retriever_parser<std::vector<std::u8string_view>>(
				{ u8"native_plugins" },
				settings::basic_parsers::basic_type_with_default<std::vector<std::u8string_view>>(
					{}, json::array_parser<std::u8string_view>()
					)
				);
			auto value = parser.get_main_profile().get_value();
			for (std::u8string_view p : value) {
				if (auto plugin = native_plugin::load(std::filesystem::path(p))) {
					native_plugin &plug_ref = *plugin;
					plugman.attach(std::move(plugin));
					plug_ref.enable();
				} else {
					logger::get().log_warning(CP_HERE) << "failed to load plugin " << p;
					continue;
				}
			}
		}

		{ // create renderer
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

		// parse visual arrangements
		// this is done after the renderer is created because this may require loading textures
		{
			auto doc = json::parse_file<json::rapidjson::document_t>("config/arrangements.json");
			auto val = json::parsing::make_value(doc.root());
			arrangements_parser<decltype(val)> parser(man);
			parser.parse_arrangements_config(val.get<decltype(val)::object_type>());
		}

		// parse hotkeys
		hotkey_json_parser<json::rapidjson::value_t>::parse_config(
			man.get_class_hotkeys().mapping,
			json::parse_file<json::rapidjson::document_t>("config/keys.json").root().get<json::rapidjson::object_t>()
		);


		// create welcome page
		auto *stack = man.create_element<stack_panel>();
		stack->set_orientation(orientation::vertical);

		auto *text = man.create_element<text_edit>();
		text->set_text(u8"this line ends with \\r\\n\r\nthis line ends with \\r\rsample text\nعندما يريد العالم أن ‪يتكلّم ‬ ،\n فهو يتحدّث بلغة يونيكود. تسجّل الآن \nلحضور المؤتمر الدولي العاشر ليونيكود (Unicode \nConference)، الذي سيعقد في 10-12 آذار 1997 بمدينة مَايِنْتْس، ألمانيا.\n و سيجمع المؤتمر بين خبراء من كافة قطاعات \nالصناعة على الشبكة العالمية انترنيت ويونيكود،\n حيث ستتم، على الصعيدين الدولي والمحلي على حد\n سواء مناقشة سبل استخدام يونكود في النظم القائمة\n وفيما يخص التطبيقات الحاسوبية،\n الخطوط، تصميم النصوص والحوسبة متعددة اللغات.");
		stack->children().add(*text);

		auto *lbl = man.create_element<label>();
		lbl->set_text(u8"Ctrl+O to open a file\nCtrl+Shift+O to open a file as binary");
		stack->children().add(*lbl);

		tabs::tab *tmptab = tabman.new_tab();
		tmptab->set_label(u8"Welcome");
		tmptab->children().add(*stack);
		tmptab->get_host()->activate_tab(*tmptab);


		// main loop
		while (!tabman.empty()) {
			man.get_scheduler().main_iteration();
		}

		// destroy all elements before shutting down so that elements referring to the plugins are destroyed
		man.get_scheduler().dispose_marked_elements();
		// disable & finalize plugins before disposing of managers because their disposal may rely on them
		plugman.shutdown();
	}

	return 0;
}
