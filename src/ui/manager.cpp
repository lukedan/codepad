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
		register_element_type<scroll_viewport>();
		register_element_type<scroll_view>();
		register_element_type<text_edit>();
		register_element_type<textbox>();
		register_element_type<window>();

		register_element_type<tabs::split_panel>();
		register_element_type<tabs::tab_button>();
		register_element_type<tabs::tab>();
		register_element_type<tabs::drag_destination_selector>();
		register_element_type<tabs::host>();
		register_element_type<tabs::animated_tab_buttons_panel>();


		_scheduler.get_hotkey_listener().triggered += [this](hotkey_info &info) {
			const auto *cmd = _commands.find_command(info.command.identifier);
			if (cmd) {
				(*cmd)(info.subject, info.command.arguments);
			} else {
				logger::get().log_warning(CP_HERE) << "invalid command: " << info.command.identifier;
			}
		};
	}

	element *manager::create_element_custom_no_event_or_property(
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
		element *elem = create_element_custom_no_event_or_property(type, cls);
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
				// set properties
				class_arrangements::set_properties_of(*elem, config.properties);
			}
		}
		return elem;
	}
}
