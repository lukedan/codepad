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
#include <codepad/editors/editor.h>
#include <codepad/editors/buffer_manager.h>
#include <codepad/editors/code/contents_region.h>
#include <codepad/editors/binary/contents_region.h>

namespace cp = ::codepad;

/// Used to automatically register and unregister commands.
struct command_stub {
	/// Initializes this command.
	command_stub(std::u8string n, std::function<void(cp::ui::element*)> f) :
		_name(std::move(n)), _exec(std::move(f)) {
	}
	/// Checks that the command is unregistered.
	~command_stub() {
		cp::assert_true_usage(!_registered);
	}

	/// Registers this command.
	void register_command(cp::ui::command_registry &reg) {
		if (reg.register_command(_name, _exec)) {
			_registered = true;
		} else {
			cp::logger::get().log_warning(CP_HERE) << "failed to register command: " << _name;
		}
	}
	/// Unregisters this command.
	void unregister_command(cp::ui::command_registry &reg) {
		if (_registered) {
			_exec = reg.unregister_command(_name);
			_registered = false;
		}
	}
private:
	std::u8string _name; ///< The name of this command.
	std::function<void(cp::ui::element*)> _exec; ///< The function to be executed.
	bool _registered = false; ///< Whether this command has been registered.
};

std::deque<command_stub> commands;
const cp::plugin_context *context = nullptr;

cp::editors::code::contents_region &get_code_contents_region_from(cp::editors::editor &e) {
	return *cp::editors::code::contents_region::get_from_editor(e);
}

extern "C" {
	PLUGIN_INITIALIZE(ctx, ) {
		context = &ctx;

		// do not capture anything in these lambdas - there's no reason to
		commands.emplace_back(
			u8"text_edit.move_caret_left",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.move_caret_left(false);
				})
		);
		commands.emplace_back(
			u8"text_edit.move_caret_left_selected",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.move_caret_left(true);
				})
		);
		commands.emplace_back(
			u8"text_edit.move_caret_right",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.move_caret_right(false);
				})
		);
		commands.emplace_back(
			u8"text_edit.move_caret_right_selected",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.move_caret_right(true);
				})
		);

		commands.emplace_back(
			u8"text_edit.delete_before",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.delete_character_before_caret();
				})
		);
		commands.emplace_back(
			u8"text_edit.delete_after",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>([](cp::ui::text_edit &e) {
				e.delete_character_after_caret();
				})
		);


		commands.emplace_back(
			u8"contents_region.carets.move_left",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_left(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_left_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_left(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_right",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_right(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_right_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_right(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_up",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_up(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_up_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_up(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_down",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_down(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_down_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_down(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_leftmost",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_beginning(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_leftmost_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_beginning(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_leftmost_noblank",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_beginning_advanced(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_leftmost_noblank_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_beginning_advanced(true);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_rightmost",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_ending(false);
				})
		);
		commands.emplace_back(
			u8"contents_region.carets.move_rightmost_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).move_all_carets_to_line_ending(true);
				})
		);

		commands.emplace_back(
			u8"contents_region.folding.fold_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				auto &edt = get_code_contents_region_from(e);
				for (auto caret : edt.get_carets().carets) {
					if (caret.first.caret != caret.first.selection) {
						edt.add_folded_region(std::minmax(caret.first.caret, caret.first.selection));
					}
				}
				})
		);

		commands.emplace_back(
			u8"contents_region.delete_before_carets",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).on_backspace();
				})
		);
		commands.emplace_back(
			u8"contents_region.delete_after_carets",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).on_delete();
				})
		);
		commands.emplace_back(
			u8"contents_region.insert_new_line",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).on_return();
				})
		);

		commands.emplace_back(
			u8"contents_region.toggle_insert",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				e.get_contents_region()->toggle_insert_mode();
				})
		);

		commands.emplace_back(
			u8"contents_region.undo",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).try_undo();
				})
		);
		commands.emplace_back(
			u8"contents_region.redo",
			cp::ui::command_registry::convert_type<cp::editors::editor>([](cp::editors::editor &e) {
				get_code_contents_region_from(e).try_redo();
				})
		);


		commands.emplace_back(
			u8"tab.request_close",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.request_close();
				})
		);

		commands.emplace_back(
			u8"tab.split_left",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, true);
				})
		);
		commands.emplace_back(
			u8"tab.split_right",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, false);
				})
		);
		commands.emplace_back(
			u8"tab.split_up",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, true);
				})
		);
		commands.emplace_back(
			u8"tab.split_down",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, false);
				})
		);

		commands.emplace_back(
			u8"tab.move_to_new_window",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>([](cp::ui::tabs::tab &t) {
				t.get_tab_manager().move_tab_to_new_window(t);
				})
		);


		// TODO options to not use the default encoding
		commands.emplace_back(
			u8"open_file_dialog",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>([](cp::ui::tabs::host &th) {
				auto files = cp::os::file_dialog::show_open_dialog(
					th.get_window(), cp::os::file_dialog::type::multiple_selection
				);
				cp::ui::tabs::tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = cp::editors::buffer_manager::get().open_file(path);
					auto interp = cp::editors::buffer_manager::get().open_interpretation(
						ctx, cp::editors::code::encoding_manager::get().get_default()
					);

					cp::ui::tabs::tab *tb = th.get_tab_manager().new_tab_in(&th);
					tb->set_label(path.filename().u8string());
					auto *editor = dynamic_cast<cp::editors::editor*>(
						th.get_manager().create_element(u8"editor", u8"code_editor")
					);
					auto *contents =
						dynamic_cast<cp::editors::code::contents_region*>(editor->get_contents_region());
					contents->code_selection_renderer() =
						std::make_unique<cp::editors::rounded_selection_renderer>();
					contents->set_document(interp);
					tb->children().add(*editor);
					last = tb;
				}
				if (last) {
					th.activate_tab(*last);
				}
				})
		);

		// TODO options to not use the default encoding
		commands.emplace_back(
			u8"new_file",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>([](cp::ui::tabs::host &th) {
				auto buf = cp::editors::buffer_manager::get().new_file();
				auto interp = cp::editors::buffer_manager::get().open_interpretation(
					buf, cp::editors::code::encoding_manager::get().get_default()
				);

				cp::ui::tabs::tab *tb = th.get_tab_manager().new_tab_in(&th);
				tb->set_label(u8"New file");
				auto *editor = dynamic_cast<cp::editors::editor*>(
					th.get_manager().create_element(u8"editor", u8"code_editor")
				);
				auto *contents = dynamic_cast<cp::editors::code::contents_region*>(editor->get_contents_region());
				contents->code_selection_renderer() = std::make_unique<cp::editors::rounded_selection_renderer>();
				contents->set_document(interp);
				tb->children().add(*editor);
				th.activate_tab(*tb);
				})
		);

		commands.emplace_back(
			u8"open_binary_file_dialog",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>([](cp::ui::tabs::host &th) {
				auto files = cp::os::file_dialog::show_open_dialog(
					th.get_window(), cp::os::file_dialog::type::multiple_selection
				);
				cp::ui::tabs::tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = cp::editors::buffer_manager::get().open_file(path);

					cp::ui::tabs::tab *tb = th.get_tab_manager().new_tab_in(&th);
					tb->set_label(path.filename().u8string());
					auto *editor = dynamic_cast<cp::editors::editor*>(
						th.get_manager().create_element(u8"editor", u8"binary_editor")
					);
					auto *contents =
						dynamic_cast<cp::editors::binary::contents_region*>(editor->get_contents_region());
					contents->code_selection_renderer() =
						std::make_unique<cp::editors::rounded_selection_renderer>();
					contents->set_buffer(std::move(ctx));
					tb->children().add(*editor);
					last = tb;
				}
				if (last) {
					th.activate_tab(*last);
				}
				})
		);
	}

	PLUGIN_FINALIZE() {
		context = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "command_pack";
	}

	PLUGIN_ENABLE() {
		auto &registry = context->ui_man->get_command_registry();
		for (command_stub &stub : commands) {
			stub.register_command(registry);
		}
	}

	PLUGIN_DISABLE() {
		auto &registry = context->ui_man->get_command_registry();
		for (command_stub &stub : commands) {
			stub.unregister_command(registry);
		}
	}
}
