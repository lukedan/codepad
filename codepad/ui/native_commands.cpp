#include "native_commands.h"

/// \file
/// Definitions of natively supported commands.

#include <algorithm>

#include "commands.h"
#include "../editors/tabs.h"
#include "../editors/code/codebox.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"
#include "../editors/buffer_manager.h"

using namespace std;

using namespace codepad::os;
using namespace codepad::editor;
using namespace codepad::editor::code;

namespace codepad::ui::native_commands {
	void register_all() {
		command_registry &reg = command_registry::get();


		reg.register_command(
			CP_STRLIT("editor.carets.move_left"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_left(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_left_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_left(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_right"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_right(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_right_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_right(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_up"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_up(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_up_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_up(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_down"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_down(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_down_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_down(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_beginning(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_beginning(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_noblank"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_beginning_advanced(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_noblank_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_beginning_advanced(true);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_rightmost"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_ending(false);
			})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_rightmost_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor().move_all_carets_to_line_ending(true);
			})
		);

		reg.register_command(
			CP_STRLIT("editor.folding.fold_selected"), convert_type<codebox>([](codebox *e) {
				code::editor &edt = e->get_editor();
				for (auto caret : edt.get_carets().carets) {
					if (caret.first.first != caret.first.second) {
						edt.add_folded_region(minmax(caret.first.first, caret.first.second));
					}
				}
			})
		);

		reg.register_command(
			CP_STRLIT("editor.delete_before_carets"), convert_type<codebox>([](codebox *e) {
				e->get_editor().on_backspace();
			})
		);
		reg.register_command(
			CP_STRLIT("editor.delete_after_carets"), convert_type<codebox>([](codebox *e) {
				e->get_editor().on_delete();
			})
		);

		reg.register_command(
			CP_STRLIT("editor.toggle_insert"), convert_type<codebox>([](codebox *e) {
				e->get_editor().toggle_insert_mode();
			})
		);

		reg.register_command(
			CP_STRLIT("editor.undo"), convert_type<codebox>([](codebox *e) {
				e->get_editor().try_undo();
			})
		);
		reg.register_command(
			CP_STRLIT("editor.redo"), convert_type<codebox>([](codebox *e) {
				e->get_editor().try_redo();
			})
		);


		reg.register_command(
			CP_STRLIT("tab.request_close"), convert_type<tab>([](tab *t) {
				t->request_close();
			})
		);

		reg.register_command(
			CP_STRLIT("tab.split_left"), convert_type<tab>([](tab *t) {
				tab_manager::get().split_tab(*t, false, true);
			})
		);
		reg.register_command(
			CP_STRLIT("tab.split_right"), convert_type<tab>([](tab *t) {
				tab_manager::get().split_tab(*t, false, false);
			})
		);
		reg.register_command(
			CP_STRLIT("tab.split_up"), convert_type<tab>([](tab *t) {
				tab_manager::get().split_tab(*t, true, true);
			})
		);
		reg.register_command(
			CP_STRLIT("tab.split_down"), convert_type<tab>([](tab *t) {
				tab_manager::get().split_tab(*t, true, false);
			})
		);

		reg.register_command(
			CP_STRLIT("tab.move_to_new_window"), convert_type<tab>([](tab *t) {
				tab_manager::get().move_tab_to_new_window(*t);
			})
		);


		// TODO options to not use the default encoding
		reg.register_command(
			CP_STRLIT("open_file_dialog"), convert_type<tab_host>([](tab_host *th) {
				auto files = open_file_dialog(th->get_window(), file_dialog_type::multiple_selection);
				tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = buffer_manager::get().open_file(path);
					auto interp = buffer_manager::get().open_interpretation(
						ctx, encoding_manager::get().get_default()
					);

					tab *tb = tab_manager::get().new_tab_in(th);
					tb->set_label(path.filename().u8string());
					auto *box = manager::get().create_element<codebox>();
					box->insert_component_before(
						&box->get_editor(), *manager::get().create_element<line_number_display>()
					);
					box->insert_component_before(nullptr, *manager::get().create_element<minimap>());
					box->get_editor().set_document(interp);
					tb->children().add(*box);
					last = tb;
				}
				if (last != nullptr) {
					th->activate_tab(*last);
				}
			})
		);

		// TODO options to not use the default encoding
		reg.register_command(
			CP_STRLIT("new_file"), convert_type<tab_host>([](tab_host *th) {
				auto buf = buffer_manager::get().new_file();
				auto interp = buffer_manager::get().open_interpretation(buf, encoding_manager::get().get_default());

				tab *tb = tab_manager::get().new_tab_in(th);
				tb->set_label(CP_STRLIT("New file"));
				auto *box = manager::get().create_element<codebox>();
				box->insert_component_before(
					&box->get_editor(), *manager::get().create_element<line_number_display>()
				);
				box->insert_component_before(nullptr, *manager::get().create_element<minimap>());
				box->get_editor().set_document(interp);
				tb->children().add(*box);
				th->activate_tab(*tb);
			})
		);
	}
}
