#include "native_commands.h"
#include "commands.h"
#include "../editors/docking.h"
#include "../editors/code/codebox.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"
#include "../editors/code/context_manager.h"

using namespace codepad::os;
using namespace codepad::editor;
using namespace codepad::editor::code;

namespace codepad::ui::native_commands {
	void register_all() {
		command_registry &reg = command_registry::get();
		reg.register_command(
			CP_STRLIT("editor.carets.move_left"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_left(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_left_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_left(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_right"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_right(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_right_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_right(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_up"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_up(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_up_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_up(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_down"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_down(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_down_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_down(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_beginning(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_beginning(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_noblank"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_beginning_advanced(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_leftmost_noblank_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_beginning_advanced(true);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_rightmost"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_ending(false);
				})
		);
		reg.register_command(
			CP_STRLIT("editor.carets.move_rightmost_selected"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->move_all_carets_to_line_ending(true);
				})
		);

		reg.register_command(
			CP_STRLIT("editor.delete_before_carets"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->delete_selection_or_char_before();
				})
		);
		reg.register_command(
			CP_STRLIT("editor.delete_after_carets"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->delete_selection_or_char_after();
				})
		);

		reg.register_command(
			CP_STRLIT("editor.toggle_insert"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->toggle_insert_mode();
				})
		);

		reg.register_command(
			CP_STRLIT("editor.undo"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->try_undo();
				})
		);
		reg.register_command(
			CP_STRLIT("editor.redo"), convert_type<codebox>([](codebox *e) {
				e->get_editor()->try_redo();
				})
		);


		reg.register_command(
			CP_STRLIT("tab.request_close"), convert_type<tab>([](tab *t) {
				t->request_close();
				})
		);

		reg.register_command(
			CP_STRLIT("tab.split_left"), convert_type<tab>([](tab *t) {
				dock_manager::get().split_tab(*t, orientation::horizontal, true);
				})
		);
		reg.register_command(
			CP_STRLIT("tab.split_right"), convert_type<tab>([](tab *t) {
				dock_manager::get().split_tab(*t, orientation::horizontal, false);
				})
		);
		reg.register_command(
			CP_STRLIT("tab.split_up"), convert_type<tab>([](tab *t) {
				dock_manager::get().split_tab(*t, orientation::vertical, true);
				})
		);
		reg.register_command(
			CP_STRLIT("tab.split_down"), convert_type<tab>([](tab *t) {
				dock_manager::get().split_tab(*t, orientation::vertical, false);
				})
		);

		reg.register_command(
			CP_STRLIT("tab.move_to_new_window"), convert_type<tab>([](tab *t) {
				dock_manager::get().move_tab_to_new_window(*t);
				})
		);


		reg.register_command(
			CP_STRLIT("open_file_dialog"), convert_type<tab_host>([](tab_host *th) {
				auto files = open_file_dialog(th->get_window(), file_dialog_type::multiple_selection);
				tab *last = nullptr;
				for (const auto &path : files) {
					auto ctx = context_manager::get().open_file(path);
					tab *tb = dock_manager::get().new_tab_in(th);
					tb->set_caption(convert_to_current_encoding(path.filename().native()));
					code::codebox *box = element::create<codebox>();
					box->add_component_left(*element::create<line_number>());
					box->add_component_right(*element::create<minimap>());
					box->get_editor()->set_context(ctx);
					tb->children().add(*box);
					last = tb;
				}
				if (last != nullptr) {
					th->activate_tab(*last);
				}
				})
		);

		reg.register_command(
			CP_STRLIT("new_file"), convert_type<tab_host>([](tab_host *th) {
				auto ctx = context_manager::get().new_file();
				tab *tb = dock_manager::get().new_tab_in(th);
				tb->set_caption(CP_STRLIT("new file"));
				code::codebox *box = element::create<codebox>();
				box->add_component_left(*element::create<line_number>());
				box->add_component_right(*element::create<minimap>());
				box->get_editor()->set_context(ctx);
				tb->children().add(*box);
				th->activate_tab(*tb);
				})
		);
	}
}
