// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "manager.h"

/// \file
/// Implementation of certain methods of codepad::ui::manager.

#include <deque>

#include "window.h"
#include "element.h"
#include "panel.h"
#include "native_commands.h"
#include "tabs/manager.h"
#include "tabs/animated_tab_button_panel.h"
#include "../editors/code/contents_region.h"
#include "../editors/code/components.h"
#include "../editors/binary/contents_region.h"
#include "../editors/binary/components.h"

using namespace std;
using namespace std::chrono;
using namespace codepad::os;

namespace codepad::ui {
	manager::manager() {
		_predef_states.mouse_over = register_state_id(CP_STRLIT("mouse_over"), element_state_type::passive);
		_predef_states.mouse_down = register_state_id(CP_STRLIT("mouse_down"), element_state_type::passive);
		_predef_states.focused = register_state_id(CP_STRLIT("focused"), element_state_type::passive);
		_predef_states.child_focused = register_state_id(CP_STRLIT("child_focused"), element_state_type::passive);
		_predef_states.selected = register_state_id(CP_STRLIT("selected"), element_state_type::passive);
		_predef_states.corpse = register_state_id(CP_STRLIT("corpse"), element_state_type::passive);

		_predef_states.render_invisible = register_state_id(CP_STRLIT("render_invisible"), element_state_type::configuration);
		_predef_states.hittest_invisible = register_state_id(CP_STRLIT("hittest_invisible"), element_state_type::configuration);
		_predef_states.layout_invisible = register_state_id(CP_STRLIT("layout_invisible"), element_state_type::configuration);
		_predef_states.vertical = register_state_id(CP_STRLIT("vertical"), element_state_type::configuration);


		// TODO use reflection in C++20 (?) for everything below
		register_transition_function(CP_STRLIT("linear"), transition_functions::linear);
		register_transition_function(CP_STRLIT("smoothstep"), transition_functions::smoothstep);
		register_transition_function(CP_STRLIT("concave_quadratic"), transition_functions::concave_quadratic);
		register_transition_function(CP_STRLIT("convex_quadratic"), transition_functions::convex_quadratic);
		register_transition_function(CP_STRLIT("concave_cubic"), transition_functions::concave_cubic);
		register_transition_function(CP_STRLIT("convex_cubic"), transition_functions::convex_cubic);


		register_element_type<element>();
		register_element_type<panel>();
		register_element_type<stack_panel>();
		register_element_type<label>();
		register_element_type<button>();
		register_element_type<scrollbar>();
		register_element_type<scrollbar_drag_button>();
		register_element_type<window>();

		register_element_type<split_panel>();
		register_element_type<tab_button>();
		register_element_type<tab>();
		register_element_type<drag_destination_selector>();
		register_element_type<tab_host>();
		register_element_type<animated_tab_buttons_panel>();

		register_element_type<editors::editor>();
		register_element_type<editors::code::contents_region>();
		register_element_type<editors::code::line_number_display>();
		register_element_type<editors::code::minimap>();
		register_element_type<editors::binary::contents_region>();
		register_element_type<editors::binary::primary_offset_display>();


		native_commands::register_all(_commands);
		_scheduler.get_hotkey_listener().triggered += [this](hotkey_info &info) {
			const auto *cmd = _commands.try_find_command(info.command);
			if (cmd) {
				(*cmd)(info.parameter);
			} else {
				logger::get().log_warning(CP_HERE, "invalid command: ", info.command);
			}
		};
	}
}
