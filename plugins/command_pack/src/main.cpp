// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of commonly used commands.

#include <deque>

#include <plugin_defs.h>

#include <codepad/core/plugins.h>
#include <codepad/ui/element.h>
#include <codepad/ui/manager.h>
#include <codepad/ui/commands.h>
#include <codepad/ui/elements/text_edit.h>
#include <codepad/ui/elements/tabs/host.h>
#include <codepad/ui/elements/tabs/manager.h>

namespace cp = ::codepad;

namespace command_pack {
	cp::ui::command_registry::command_list _commands;


	/// Parses parameters for caret movement: `word', which indicates that caret movement should be based on words
	/// instead of characters, and `continue_selection', which indicates that the other end of the selection should
	/// be kept as-is. The \p continue_sel parameter can be \p nullptr which indicates that no such parameter is
	/// expected.
	void parse_caret_movement_arguments(
		const cp::json::value_storage &args, bool &word, bool *continue_sel = nullptr
	) {
		if (auto obj = args.get_parser_value().cast_optional<cp::json::storage::object_t>()) {
			if (auto b_word = obj->parse_optional_member<bool>(u8"word")) {
				word = b_word.value();
			}
			if (continue_sel) {
				if (auto continue_selection = obj->parse_optional_member<bool>(u8"continue_selection")) {
					*continue_sel = continue_selection.value();
				}
			}
		}
	}
}


extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plugin) {
		command_pack::_commands = cp::ui::command_registry::command_list(
			ctx.ui_man->get_command_registry()
		);

		// TODO word movement
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_left",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_left(continue_sel);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_right",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_right(continue_sel);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_up",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_up(continue_sel);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_down",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_down(continue_sel);
				}
				)
		);

		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_to_line_beginning",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_to_line_beginning(continue_sel);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.move_caret_to_line_ending",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_arguments(args, word, &continue_sel);
					e.move_caret_to_line_ending(continue_sel);
				}
				)
		);

		command_pack::_commands.commands.emplace_back(
			u8"text_edit.delete_before",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					if (!e.is_readonly()) {
						bool word = false;
						command_pack::parse_caret_movement_arguments(args, word);
						e.delete_character_before_caret();
					}
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"text_edit.delete_after",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					if (!e.is_readonly()) {
						bool word = false;
						command_pack::parse_caret_movement_arguments(args, word);
						e.delete_character_after_caret();
					}
				}
				)
		);


		command_pack::_commands.commands.emplace_back(
			u8"tab.request_close",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.request_close();
				}
				)
		);

		command_pack::_commands.commands.emplace_back(
			u8"tab.split_left",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, true);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"tab.split_right",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, false);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"tab.split_up",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, true);
				}
				)
		);
		command_pack::_commands.commands.emplace_back(
			u8"tab.split_down",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, false);
				}
				)
		);

		command_pack::_commands.commands.emplace_back(
			u8"tab.move_to_new_window",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().move_tab_to_new_window(t);
				}
				)
		);
	}

	PLUGIN_FINALIZE() {
	}

	PLUGIN_GET_NAME() {
		return "command_pack";
	}

	PLUGIN_ENABLE() {
		command_pack::_commands.register_all();
	}

	PLUGIN_DISABLE() {
		command_pack::_commands.unregister_all();
	}
}
