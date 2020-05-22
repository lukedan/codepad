// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "native_commands.h"

/// \file
/// Definitions of natively supported commands.

#include <algorithm>

#include "commands.h"
#include "elements/tabs/manager.h"
#include "../editors/buffer_manager.h"
#include "../editors/code/contents_region.h"
#include "../editors/code/minimap.h"
#include "../editors/code/line_number_display.h"
#include "../editors/binary/contents_region.h"

using namespace std;

using namespace codepad::os;
using namespace codepad::ui::tabs;
using namespace codepad::editors;

namespace codepad::ui::native_commands {
	void register_all(command_registry &reg) {
		reg.register_command(
			u8"contents_region.carets.move_left", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_left(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_left_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_left(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_right", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_right(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_right_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_right(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_up", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_up(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_up_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_up(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_down", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_down(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_down_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_down(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_leftmost", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_beginning(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_leftmost_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_beginning(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_leftmost_noblank", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_beginning_advanced(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_leftmost_noblank_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_beginning_advanced(true);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_rightmost", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_ending(false);
			})
		);
		reg.register_command(
			u8"contents_region.carets.move_rightmost_selected", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->move_all_carets_to_line_ending(true);
			})
		);

		reg.register_command(
			u8"contents_region.folding.fold_selected", convert_type<editor>([](editor *e) {
				auto *edt = code::contents_region::get_from_editor(*e);
				for (auto caret : edt->get_carets().carets) {
					if (caret.first.caret != caret.first.selection) {
						edt->add_folded_region(minmax(caret.first.caret, caret.first.selection));
					}
				}
			})
		);

		reg.register_command(
			u8"contents_region.delete_before_carets", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->on_backspace();
			})
		);
		reg.register_command(
			u8"contents_region.delete_after_carets", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->on_delete();
			})
		);
		reg.register_command(
			u8"contents_region.insert_new_line", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->on_return();
			})
		);

		reg.register_command(
			u8"contents_region.toggle_insert", convert_type<editor>([](editor *e) {
				e->get_contents_region()->toggle_insert_mode();
			})
		);

		reg.register_command(
			u8"contents_region.undo", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->try_undo();
			})
		);
		reg.register_command(
			u8"contents_region.redo", convert_type<editor>([](editor *e) {
				code::contents_region::get_from_editor(*e)->try_redo();
			})
		);


		reg.register_command(
			u8"tab.request_close", convert_type<tab>([](tab *t) {
				t->request_close();
			})
		);

		reg.register_command(
			u8"tab.split_left", convert_type<tab>([](tab *t) {
				t->get_tab_manager().split_tab(*t, orientation::horizontal, true);
			})
		);
		reg.register_command(
			u8"tab.split_right", convert_type<tab>([](tab *t) {
				t->get_tab_manager().split_tab(*t, orientation::horizontal, false);
			})
		);
		reg.register_command(
			u8"tab.split_up", convert_type<tab>([](tab *t) {
				t->get_tab_manager().split_tab(*t, orientation::vertical, true);
			})
		);
		reg.register_command(
			u8"tab.split_down", convert_type<tab>([](tab *t) {
				t->get_tab_manager().split_tab(*t, orientation::vertical, false);
			})
		);

		reg.register_command(
			u8"tab.move_to_new_window", convert_type<tab>([](tab *t) {
				t->get_tab_manager().move_tab_to_new_window(*t);
			})
		);


		// TODO options to not use the default encoding
		reg.register_command(
			u8"open_file_dialog", convert_type<host>([](host *th) {
				auto files = file_dialog::show_open_dialog(th->get_window(), file_dialog::type::multiple_selection);
				tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = buffer_manager::get().open_file(path);
					auto interp = buffer_manager::get().open_interpretation(
						ctx, code::encoding_manager::get().get_default()
					);

					tab *tb = th->get_tab_manager().new_tab_in(th);
					tb->set_label(path.filename().u8string());
					auto *editor = dynamic_cast<editors::editor*>(
						th->get_manager().create_element(u8"editor", u8"code_editor")
					);
					auto *contents = dynamic_cast<code::contents_region*>(editor->get_contents_region());
					contents->code_selection_renderer() = std::make_unique<rounded_selection_renderer>();
					contents->set_document(interp);
					tb->children().add(*editor);
					last = tb;
				}
				if (last) {
					th->activate_tab(*last);
				}
			})
		);

		// TODO options to not use the default encoding
		reg.register_command(
			u8"new_file", convert_type<host>([](host *th) {
				auto buf = buffer_manager::get().new_file();
				auto interp = buffer_manager::get().open_interpretation(
					buf, code::encoding_manager::get().get_default()
				);

				tab *tb = th->get_tab_manager().new_tab_in(th);
				tb->set_label(u8"New file");
				auto *editor = dynamic_cast<editors::editor*>(
					th->get_manager().create_element(u8"editor", u8"code_editor")
				);
				auto *contents = dynamic_cast<code::contents_region*>(editor->get_contents_region());
				contents->code_selection_renderer() = std::make_unique<rounded_selection_renderer>();
				contents->set_document(interp);
				tb->children().add(*editor);
				th->activate_tab(*tb);
			})
		);

		reg.register_command(
			u8"open_binary_file_dialog", convert_type<host>([](host *th) {
				auto files = file_dialog::show_open_dialog(th->get_window(), file_dialog::type::multiple_selection);
				tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = buffer_manager::get().open_file(path);

					tab *tb = th->get_tab_manager().new_tab_in(th);
					tb->set_label(path.filename().u8string());
					auto *editor = dynamic_cast<editors::editor*>(
						th->get_manager().create_element(u8"editor", u8"binary_editor")
					);
					auto *contents = dynamic_cast<binary::contents_region*>(editor->get_contents_region());
					contents->code_selection_renderer() = std::make_unique<rounded_selection_renderer>();
					contents->set_buffer(std::move(ctx));
					tb->children().add(*editor);
					last = tb;
				}
				if (last) {
					th->activate_tab(*last);
				}
			})
		);
	}
}
