// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <array>

#include "codepad/core/logging.h"
#include "codepad/core/logger_sinks.h"
#include "codepad/core/plugins.h"
#include "codepad/core/settings.h"
#include "codepad/core/json/parsing.h"
#include "codepad/core/json/default_engine.h"
#ifdef CP_PLATFORM_WINDOWS
#	include "codepad/os/windows/direct2d_renderer.h"
#	ifdef CP_USE_CAIRO
#		include "codepad/os/windows/cairo_renderer.h"
#	endif
#	ifdef CP_USE_SKIA
#		include "codepad/os/windows/skia_renderer.h"
#	endif
#elif defined(CP_PLATFORM_UNIX)
#	ifdef CP_USE_CAIRO
#		include "codepad/os/linux/gtk/cairo_renderer.h"
#	endif
#endif
#include "codepad/ui/config_parsers.h"
#include "codepad/ui/json_parsers.inl"
#include "codepad/ui/elements/tabs/tab.h"
#include "codepad/ui/elements/tabs/manager.h"

#include "codepad/ui/elements/stack_panel.h"
#include "codepad/ui/elements/label.h"
#include "codepad/ui/elements/text_edit.h"

using namespace codepad;
using namespace codepad::os;
using namespace codepad::ui;

tabs::tab *make_textbox_test_tab(
	manager &man,
	tabs::tab_manager &tabman,
	std::u8string_view text =
	u8"this line ends with \\r\\n\r\n"
	u8"this line ends with \\r\r"
	u8"this line ends with \\n\n"
	u8"emoji: 😊\n"
	u8"rtl, longline: لحضور المؤتمر الدولي العاشر ليونيكود (Unicode Conference)، الذي سيعقد في 10-12 آذار 1997 بمدينة مَايِنْتْس، ألمانيا."
) {
	visuals textbox_visuals;
	auto &bkg = textbox_visuals.geometries.emplace_back();
	bkg.fill.value.emplace<brush_parameters::solid_color>().color = colord(1.0, 1.0, 1.0, 0.1);
	bkg.stroke.brush.value.emplace<brush_parameters::solid_color>().color = colord(1.0, 0.0, 0.0, 1.0);
	auto &bkg_rect = bkg.value.emplace<geometries::rectangle>();
	bkg_rect.top_left = relative_vec2d(vec2d(), vec2d());
	bkg_rect.bottom_right = relative_vec2d(vec2d(1.0, 1.0), vec2d());

	element_layout textbox_layout;
	textbox_layout.height_alloc = size_allocation_type::proportion;
	textbox_layout.width_alloc = size_allocation_type::proportion;
	textbox_layout.padding = thickness(2.0);
	textbox_layout.size = vec2d(1.0, 1.0);

	std::array<horizontal_text_alignment, 9> halign{
		horizontal_text_alignment::front, horizontal_text_alignment::center, horizontal_text_alignment::rear,
		horizontal_text_alignment::front, horizontal_text_alignment::center, horizontal_text_alignment::rear,
		horizontal_text_alignment::front, horizontal_text_alignment::center, horizontal_text_alignment::rear
	};
	std::array<vertical_text_alignment, 9> valign{
		vertical_text_alignment::top, vertical_text_alignment::top, vertical_text_alignment::top,
		vertical_text_alignment::center, vertical_text_alignment::center, vertical_text_alignment::center,
		vertical_text_alignment::bottom, vertical_text_alignment::bottom, vertical_text_alignment::bottom
	};
	std::array<anchor, 9> anc{
		anchor::top_left, anchor::top, anchor::top_right,
		anchor::left, anchor::none, anchor::right,
		anchor::bottom_left, anchor::bottom, anchor::bottom_right
	};
	std::array<thickness, 9> margin{
		thickness(30.0, 30.0, 2.4, 2.4), thickness(1.2, 30.0, 1.2, 2.4), thickness(2.4, 30.0, 30.0, 2.4),
		thickness(30.0, 1.2, 2.4, 1.2), thickness(1.2, 1.2, 1.2, 1.2), thickness(2.4, 1.2, 30.0, 1.2),
		thickness(30.0, 2.4, 2.4, 30.0), thickness(1.2, 2.4, 1.2, 30.0), thickness(2.4, 2.4, 30.0, 30.0)
	};

	auto *tab = tabman.new_tab();
	tab->set_label(u8"textbox_test");

	for (std::size_t i = 0; i < 9; ++i) {
		auto *edit = man.create_element<text_edit>();

		edit->set_horizontal_alignment(halign[i]);
		edit->set_vertical_alignment(valign[i]);
		edit->set_text(std::u8string(text));

		edit->set_visual_parameters_debug(textbox_visuals);
		textbox_layout.elem_anchor = anc[i];
		textbox_layout.margin = margin[i];
		edit->set_layout_parameters_debug(textbox_layout);

		edit->set_clip_to_bounds(true);

		tab->children().add(*edit);
	}

	return tab;
}

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
#ifdef CP_USE_SKIA
			if (renderer == u8"skia") {
				man.set_renderer(std::make_unique<skia_renderer>());
			}
#endif
			assert_true_usage(man.has_renderer(), "unrecognized renderer");
		}

		// parse visual arrangements
		// this is done after the renderer is created because this may require loading textures
		{
			auto doc = json::parse_file<json::document_t>("config/arrangements.json");
			auto val = json::parsing::make_value(doc.root());
			arrangements_parser<decltype(val)> parser(man);
			parser.parse_arrangements_config(val.get<decltype(val)::object_type>());
		}

		// parse hotkeys
		hotkey_json_parser<json::value_t>::parse_config(
			man.get_class_hotkeys().mapping,
			json::parse_file<json::document_t>("config/keys.json").root().get<json::object_t>()
		);


		// create welcome page
		auto *stack = man.create_element<stack_panel>();
		stack->set_orientation(orientation::vertical);

		std::u8string_view test_text =
			u8"this line ends with \\r\\n\r\n"
			u8"this line ends with \\r\r"
			u8"this line ends with \\n\n"
			u8"emoji: 😊\n"
			u8"rtl: لحضور المؤتمر الدولي العاشر ليونيكود (Unicode Conference)، الذي سيعقد في 10-12 آذار 1997 بمدينة مَايِنْتْس، ألمانيا.";


		auto *text = man.create_element<text_edit>();
		text->set_text(std::u8string(test_text));
		stack->children().add(*text);

		auto *lbl = man.create_element<label>();
		lbl->set_text(u8"Ctrl+O to open a file\nCtrl+Shift+O to open a file as binary");
		stack->children().add(*lbl);

		auto *tb = man.create_element<textbox>();
		tb->get_text_edit()->set_text(std::u8string(test_text));
		stack->children().add(*tb);

		tabs::tab *tmptab = tabman.new_tab();
		tmptab->set_label(u8"Welcome");
		tmptab->children().add(*stack);
		tmptab->get_host()->activate_tab(*tmptab);


		make_textbox_test_tab(man, tabman);


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
