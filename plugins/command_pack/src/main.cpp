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
#include <codepad/editors/manager.h>
#include <codepad/editors/code/contents_region.h>
#include <codepad/editors/binary/contents_region.h>

namespace cp = ::codepad;

namespace command_pack {
	std::deque<cp::ui::command_registry::stub> _commands;
	const cp::plugin_context *_context = nullptr;
	cp::editors::manager *_editor_manager = nullptr;


	cp::editors::code::contents_region &get_code_contents_region_from(cp::editors::editor &e) {
		return *cp::editors::code::contents_region::get_from_editor(e);
	}

	/// Opens the specified file with the specified encoding in a tab, and adds the tab to the given
	/// \ref cp::ui::tabs::host.
	cp::ui::tabs::tab *open_file_with_encoding(
		const std::filesystem::path &file, cp::ui::tabs::host &host, std::u8string_view encoding
	) {
		auto ctx = _editor_manager->buffers.open_file(file);
		const cp::editors::code::buffer_encoding *enc = nullptr;
		if (!encoding.empty()) {
			enc = _editor_manager->encodings.get_encoding(encoding);
			if (enc == nullptr) {
				cp::logger::get().log_warning(CP_HERE) <<
					"encoding not registered: " << encoding << ", using default encoding instead";
			}
		}
		if (enc == nullptr) {
			enc = &_editor_manager->encodings.get_default();
		}
		auto interp = _editor_manager->buffers.open_interpretation(*ctx, *enc);

		cp::ui::tabs::tab *tb = host.get_tab_manager().new_tab_in(&host);
		tb->set_label(file.filename().u8string());
		auto *editor = dynamic_cast<cp::editors::editor*>(
			host.get_manager().create_element(u8"editor", u8"code_editor")
		);
		_editor_manager->buffers.initialize_code_editor(*editor, std::move(interp));
		tb->children().add(*editor);
		return tb;
	}

	/// Opens the specified file as binary in a tab, and adds the tab to the given \ref cp::ui::tabs::host.
	cp::ui::tabs::tab *open_binary_file(const std::filesystem::path &file, cp::ui::tabs::host &host) {
		auto ctx = _editor_manager->buffers.open_file(file);
		cp::ui::tabs::tab *tb = host.get_tab_manager().new_tab_in(&host);
		tb->set_label(file.u8string());
		auto *editor = dynamic_cast<cp::editors::editor*>(
			host.get_manager().create_element(u8"editor", u8"binary_editor")
		);
		auto *contents =
			dynamic_cast<cp::editors::binary::contents_region*>(editor->get_contents_region());
		contents->set_buffer(std::move(ctx));
		tb->children().add(*editor);
		return tb;
	}

	/// Parses parameters for caret movement: `word', which indicates that caret movement should be based on words
	/// instead of characters, and `continue_selection', which indicates that the other end of the selection should
	/// be kept as-is. The \p continue_sel parameter can be \p nullptr which indicates that no such parameter is
	/// expected.
	void parse_caret_movement_parameter(
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

	/// If the given argument is \p null, opens a file dialog to let the user choose one or more files. Otherwise,
	/// attempts to parse a list of file names from the given arguments.
	std::vector<std::filesystem::path> parse_file_list_or_show_dialog(
		const cp::json::value_storage &args, const cp::ui::window *dialog_parent
	) {
		if (args.get_value().is<cp::json::null_t>()) {
			return cp::os::file_dialog::show_open_dialog(
				dialog_parent, cp::os::file_dialog::type::multiple_selection
			);
		}
		// otherwise parse the arguments
		std::vector<std::filesystem::path> files;
		auto args_parser = args.get_parser_value();
		if (auto paths = args_parser.try_parse<std::vector<std::u8string_view>>(
			cp::json::array_parser<std::u8string_view>()
			)) {
			for (auto &str : paths.value()) {
				files.emplace_back(str);
			}
		} else if (auto path = args_parser.try_parse<std::u8string_view>()) {
			files.emplace_back(path.value());
		} else {
			cp::logger::get().log_error(CP_HERE) <<
				"argument to 'open_file' must either be empty, a string, or a list of strings.";
		}
		return files;
	}
}


extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plugin) {
		command_pack::_context = &ctx;

		auto editors_plugin = command_pack::_context->plugin_man->find_plugin(u8"editors");
		if (editors_plugin.valid()) {
			this_plugin.add_dependency(editors_plugin);
			if (auto **ppman = editors_plugin.get_data<cp::editors::manager*>()) {
				command_pack::_editor_manager = *ppman;
			}
		}

		// TODO word movement
		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_left",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_left(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_right",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_right(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_up",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_up(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_down",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_down(continue_sel);
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_to_line_beginning",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_to_line_beginning(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"text_edit.move_caret_to_line_ending",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					e.move_caret_to_line_ending(continue_sel);
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"text_edit.delete_before",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false;
					command_pack::parse_caret_movement_parameter(args, word);
					e.delete_character_before_caret();
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"text_edit.delete_after",
			cp::ui::command_registry::convert_type<cp::ui::text_edit>(
				[](cp::ui::text_edit &e, const cp::json::value_storage &args) {
					bool word = false;
					command_pack::parse_caret_movement_parameter(args, word);
					e.delete_character_after_caret();
				}
				)
		);


		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_left",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_left(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_right",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_right(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_up",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_up(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_down",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_down(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_to_line_beginning",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_to_line_beginning(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_to_line_beginning_noblank",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_to_line_beginning_advanced(continue_sel);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.carets.move_to_line_ending",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false, continue_sel = false;
					command_pack::parse_caret_movement_parameter(args, word, &continue_sel);
					command_pack::get_code_contents_region_from(e).move_all_carets_to_line_ending(continue_sel);
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"contents_region.folding.fold_selected",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage&) {
					auto &edt = command_pack::get_code_contents_region_from(e);
					for (auto caret : edt.get_carets().carets) {
						if (caret.first.caret != caret.first.selection) {
							edt.add_folded_region(std::minmax(caret.first.caret, caret.first.selection));
						}
					}
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"contents_region.delete_before_carets",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false;
					command_pack::parse_caret_movement_parameter(args, word);
					command_pack::get_code_contents_region_from(e).on_backspace();
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.delete_after_carets",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage &args) {
					bool word = false;
					command_pack::parse_caret_movement_parameter(args, word);
					command_pack::get_code_contents_region_from(e).on_delete();
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.insert_new_line",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage&) {
					command_pack::get_code_contents_region_from(e).on_return();
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"contents_region.toggle_insert",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage&) {
					e.get_contents_region()->toggle_insert_mode();
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"contents_region.undo",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage&) {
					command_pack::get_code_contents_region_from(e).try_undo();
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"contents_region.redo",
			cp::ui::command_registry::convert_type<cp::editors::editor>(
				[](cp::editors::editor &e, const cp::json::value_storage&) {
					command_pack::get_code_contents_region_from(e).try_redo();
				}
				)
		);


		command_pack::_commands.emplace_back(
			u8"tab.request_close",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.request_close();
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"tab.split_left",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, true);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"tab.split_right",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::horizontal, false);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"tab.split_up",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, true);
				}
				)
		);
		command_pack::_commands.emplace_back(
			u8"tab.split_down",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().split_tab(t, cp::ui::orientation::vertical, false);
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"tab.move_to_new_window",
			cp::ui::command_registry::convert_type<cp::ui::tabs::tab>(
				[](cp::ui::tabs::tab &t, const cp::json::value_storage&) {
					t.get_tab_manager().move_tab_to_new_window(t);
				}
				)
		);


		// TODO options to not use the default encoding
		command_pack::_commands.emplace_back(
			u8"open_file",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>(
				[](cp::ui::tabs::host &th, const cp::json::value_storage &args) {
					std::vector<std::filesystem::path> files =
						command_pack::parse_file_list_or_show_dialog(args, th.get_window());
					cp::ui::tabs::tab *last = nullptr;
					for (const auto &path : files) {
						last = command_pack::open_file_with_encoding(path, th, u8"");
					}
					if (last) {
						th.activate_tab(*last);
					}
				}
				)
		);

		// TODO options to not use the default encoding
		command_pack::_commands.emplace_back(
			u8"new_file",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>(
				[](cp::ui::tabs::host &th, const cp::json::value_storage&) {
					auto buf = command_pack::_editor_manager->buffers.new_file();
					auto interp = command_pack::_editor_manager->buffers.open_interpretation(
						*buf, command_pack::_editor_manager->encodings.get_default()
					);

					cp::ui::tabs::tab *tb = th.get_tab_manager().new_tab_in(&th);
					tb->set_label(u8"New file");
					auto *editor = dynamic_cast<cp::editors::editor*>(
						th.get_manager().create_element(u8"editor", u8"code_editor")
					);
					command_pack::_editor_manager->buffers.initialize_code_editor(*editor, std::move(interp));
					tb->children().add(*editor);
					th.activate_tab(*tb);
				}
				)
		);

		command_pack::_commands.emplace_back(
			u8"open_binary_file_dialog",
			cp::ui::command_registry::convert_type<cp::ui::tabs::host>(
				[](cp::ui::tabs::host &th, const cp::json::value_storage &args) {
					std::vector<std::filesystem::path> files =
						command_pack::parse_file_list_or_show_dialog(args, th.get_window());
					cp::ui::tabs::tab *last = nullptr;
					for (const auto &path : files) {
						last = command_pack::open_binary_file(path, th);
					}
					if (last) {
						th.activate_tab(*last);
					}
				}
				)
		);
	}

	PLUGIN_FINALIZE() {
		command_pack::_context = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "command_pack";
	}

	PLUGIN_ENABLE() {
		auto &registry = command_pack::_context->ui_man->get_command_registry();
		for (auto &stub : command_pack::_commands) {
			stub.register_command(registry);
		}
	}

	PLUGIN_DISABLE() {
		auto &registry = command_pack::_context->ui_man->get_command_registry();
		for (auto &stub : command_pack::_commands) {
			stub.unregister_command(registry);
		}
	}
}
