// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "details.h"

/// \file
/// Implementation of built-in commands.

#include <codepad/ui/elements/tabs/host.h>
#include <codepad/ui/elements/tabs/manager.h>

#include "codepad/editors/editor.h"
#include "codepad/editors/manager.h"
#include "codepad/editors/code/contents_region.h"
#include "codepad/editors/code/search_panel.h"
#include "codepad/editors/binary/contents_region.h"

namespace codepad::editors::_details {
	/// Opens the specified file with the specified encoding in a tab, and adds the tab to the given
	/// \ref ui::tabs::host.
	ui::tabs::tab *_open_file_with_encoding(
		const std::filesystem::path &file, ui::tabs::host &host, std::u8string_view encoding
	) {
		auto ctx = get_manager().buffers.open_file(file);
		if (auto *lang = get_manager().get_language_for_file(file)) {
			ctx->set_language(*lang);
		}
		const code::buffer_encoding *enc = nullptr;
		if (!encoding.empty()) {
			enc = get_manager().encodings.get_encoding(encoding);
			if (enc == nullptr) {
				logger::get().log_warning() <<
					"encoding not registered: " << encoding << ", using default encoding instead";
			}
		}
		if (enc == nullptr) {
			enc = &get_manager().encodings.get_default();
		}
		auto interp = get_manager().buffers.open_interpretation(*ctx, *enc);

		ui::tabs::tab *tb = host.get_tab_manager().new_tab_in(&host);
		tb->set_label(file.filename().u8string());
		auto *edt = dynamic_cast<editor*>(
			host.get_manager().create_element(u8"editor", u8"code_editor")
			);
		get_manager().buffers.initialize_code_editor(*edt, std::move(interp));
		tb->children().add(*edt);
		return tb;
	}
	/// Opens the specified file as binary in a tab, and adds the tab to the given \ref ui::tabs::host.
	ui::tabs::tab *_open_binary_file(const std::filesystem::path &file, ui::tabs::host &host) {
		auto ctx = get_manager().buffers.open_file(file);
		if (auto *lang = get_manager().get_language_for_file(file)) {
			ctx->set_language(*lang);
		}
		ui::tabs::tab *tb = host.get_tab_manager().new_tab_in(&host);
		tb->set_label(file.u8string());
		auto *edt = dynamic_cast<editor*>(
			host.get_manager().create_element(u8"editor", u8"binary_editor")
			);
		auto *contents =
			dynamic_cast<binary::contents_region*>(edt->get_contents_region());
		contents->set_buffer(std::move(ctx));
		tb->children().add(*edt);
		return tb;
	}
	/// Parses parameters for caret movement: `word', which indicates that caret movement should be based on words
	/// instead of characters, and `continue_selection', which indicates that the other end of the selection should
	/// be kept as-is. The \p continue_sel parameter can be \p nullptr which indicates that no such parameter is
	/// expected.
	void _parse_caret_movement_arguments(
		const json::value_storage &args, bool &word, bool *continue_sel = nullptr
	) {
		if (auto obj = args.get_parser_value().cast_optional<json::storage::object_t>()) {
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
	std::list<ui::command_registry::stub> get_builtin_commands(const plugin_context &plug_ctx) {
		std::list<ui::command_registry::stub> result;

		result.emplace_back(
			u8"contents_region.carets.move_left",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_left(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_right",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_right(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_up",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_up(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_down",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_down(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_to_line_beginning",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_to_line_beginning(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_to_line_beginning_noblank",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_to_line_beginning_advanced(continue_sel);
				}
				)
		);
		result.emplace_back(
			u8"contents_region.carets.move_to_line_ending",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false, continue_sel = false;
					_parse_caret_movement_arguments(args, word, &continue_sel);
					code::contents_region::get_from_editor(e)->move_all_carets_to_line_ending(continue_sel);
				}
				)
		);

		result.emplace_back(
			u8"contents_region.folding.fold_selected",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage&) {
					auto &edt = *code::contents_region::get_from_editor(e);
					for (
						auto it = edt.get_carets().begin();
						it.get_iterator() != edt.get_carets().carets.end();
						it.move_next()
					) {
						if (it.get_iterator()->caret.has_selection()) {
							edt.add_folded_region(it.get_caret_selection().get_range());
						}
					}
				}
				)
		);

		result.emplace_back(
			u8"contents_region.delete_before_carets",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false;
					_parse_caret_movement_arguments(args, word);
					code::contents_region::get_from_editor(e)->on_backspace();
				}
				)
		);
		result.emplace_back(
			u8"contents_region.delete_after_carets",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage &args) {
					bool word = false;
					_parse_caret_movement_arguments(args, word);
					code::contents_region::get_from_editor(e)->on_delete();
				}
				)
		);
		result.emplace_back(
			u8"contents_region.insert_new_line",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage&) {
					code::contents_region::get_from_editor(e)->on_return();
				}
				)
		);

		result.emplace_back(
			u8"contents_region.toggle_insert",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage&) {
					e.get_contents_region()->toggle_insert_mode();
				}
				)
		);

		result.emplace_back(
			u8"contents_region.undo",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage&) {
					code::contents_region::get_from_editor(e)->try_undo();
				}
				)
		);
		result.emplace_back(
			u8"contents_region.redo",
			ui::command_registry::convert_type<editor>(
				[](editor &e, const json::value_storage&) {
					code::contents_region::get_from_editor(e)->try_redo();
				}
				)
		);


		result.emplace_back(
			u8"code_contents_region.search",
			ui::command_registry::convert_type<editor>(
				[&](editor &e, const json::value_storage&) {
					auto *pnl = plug_ctx.ui_man->create_element<code::search_panel>();
					pnl->set_contents_region(*code::contents_region::get_from_editor(e));
					e.children().add(*pnl);
				}
				)
		);
		result.emplace_back(
			u8"code_search_panel.close",
			ui::command_registry::convert_type<code::search_panel>(
				[&](code::search_panel &p, const json::value_storage&) {
					p.on_close();
				}
				)
		);


		// TODO options to not use the default encoding
		result.emplace_back(
			u8"open_file",
			ui::command_registry::convert_type<ui::tabs::host>(
				[](ui::tabs::host &th, const json::value_storage&) {
					std::vector<std::filesystem::path> files = os::file_dialog::show_open_dialog(
						th.get_window(), os::file_dialog::type::multiple_selection
					);
					ui::tabs::tab *last = nullptr;
					for (const auto &path : files) {
						last = _open_file_with_encoding(path, th, u8"");
					}
					if (last) {
						th.activate_tab_and_focus(*last);
					}
				}
				)
		);

		// TODO options to not use the default encoding
		result.emplace_back(
			u8"new_file",
			ui::command_registry::convert_type<ui::tabs::host>(
				[](ui::tabs::host &th, const json::value_storage&) {
					auto buf = get_manager().buffers.new_file();
					auto interp = get_manager().buffers.open_interpretation(
						*buf, get_manager().encodings.get_default()
					);

					ui::tabs::tab *tb = th.get_tab_manager().new_tab_in(&th);
					tb->set_label(u8"New file");
					auto *edt = dynamic_cast<editor*>(
						th.get_manager().create_element(u8"editor", u8"code_editor")
					);
					get_manager().buffers.initialize_code_editor(*edt, std::move(interp));
					tb->children().add(*edt);
					th.activate_tab_and_focus(*tb);
				}
				)
		);

		result.emplace_back(
			u8"open_binary_file_dialog",
			ui::command_registry::convert_type<ui::tabs::host>(
				[](ui::tabs::host &th, const json::value_storage&) {
					std::vector<std::filesystem::path> files = os::file_dialog::show_open_dialog(
						th.get_window(), os::file_dialog::type::multiple_selection
					);
					ui::tabs::tab *last = nullptr;
					for (const auto &path : files) {
						last = _open_binary_file(path, th);
					}
					if (last) {
						th.activate_tab_and_focus(*last);
					}
				}
				)
		);


		return result;
	}
}
