// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include <deque>

#include "codepad/ui/window.h"
#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/elements/label.h"
#include "codepad/ui/elements/text_edit.h"
#include "codepad/ui/elements/tabs/manager.h"
#include "codepad/ui/elements/tabs/animated_tab_button_panel.h"
#include "codepad/editors/code/contents_region.h"
#include "codepad/editors/code/minimap.h"
#include "codepad/editors/code/line_number_display.h"
#include "codepad/editors/binary/contents_region.h"
#include "codepad/editors/binary/components.h"

namespace codepad::ui {
	manager::manager(settings &s) : _settings(s) {
		// TODO use reflection in C++23 (?) for everything below
		register_transition_function(u8"linear", transition_functions::linear);
		register_transition_function(u8"smoothstep", transition_functions::smoothstep);
		register_transition_function(u8"concave_quadratic", transition_functions::concave_quadratic);
		register_transition_function(u8"convex_quadratic", transition_functions::convex_quadratic);
		register_transition_function(u8"concave_cubic", transition_functions::concave_cubic);
		register_transition_function(u8"convex_cubic", transition_functions::convex_cubic);


		register_element_type<element>();
		register_element_type<panel>();
		register_element_type<stack_panel>();
		register_element_type<label>();
		register_element_type<button>();
		register_element_type<scrollbar>();
		register_element_type<scrollbar_drag_button>();
		register_element_type<text_edit>();
		register_element_type<os::window>();

		register_element_type<tabs::split_panel>();
		register_element_type<tabs::tab_button>();
		register_element_type<tabs::tab>();
		register_element_type<tabs::drag_destination_selector>();
		register_element_type<tabs::host>();
		register_element_type<tabs::animated_tab_buttons_panel>();

		register_element_type<editors::editor>();
		register_element_type<editors::code::contents_region>();
		register_element_type<editors::code::line_number_display>();
		register_element_type<editors::code::minimap>();
		register_element_type<editors::binary::contents_region>();
		register_element_type<editors::binary::primary_offset_display>();


		_scheduler.get_hotkey_listener().triggered += [this](hotkey_info &info) {
			const auto *cmd = _commands.try_find_command(info.command);
			if (cmd) {
				(*cmd)(info.parameter);
			} else {
				logger::get().log_warning(CP_HERE) << "invalid command: " << info.command;
			}
		};
	}

	element *manager::create_element_custom_no_event_or_attribute(
		std::u8string_view type, std::u8string_view cls
	) {
		auto it = _element_registry.find(type);
		if (it == _element_registry.end()) {
			return nullptr;
		}
		element *elem = it->second(); // the constructor must not use element::_manager
		elem->_manager = this;
		elem->_initialize(cls);
#ifdef CP_CHECK_USAGE_ERRORS
		assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
#endif
		return elem;
	}

	element *manager::create_element_custom(
		std::u8string_view type, std::u8string_view cls, const element_configuration &config
	) {
		element *elem = create_element_custom_no_event_or_attribute(type, cls);
		if (elem) {
			if (dynamic_cast<panel*>(elem) == nullptr) { // only register stuff for non-panels
				// register event triggers
				for (const auto &trigger : config.event_triggers) {
					if (!trigger.identifier.subject.empty()) {
						logger::get().log_warning(CP_HERE) <<
							"subjects can only be specified for events in composite elements: " <<
							cls << "::" << trigger.identifier.subject << "." << trigger.identifier.name;
					}
					class_arrangements::register_trigger_for(*elem, *elem, trigger);
				}
				// set attributes
				class_arrangements::set_attributes_of(*elem, config.attributes);
			}
		}
		return elem;
	}
}
