// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Global configurations, data, and the highlighting thread.

#include <deque>
#include <semaphore>

#include <codepad/editors/code/interpretation.h>
#include <codepad/editors/theme_manager.h>

#include "language_configuration.h"
#include "interpretation_tag.h"

namespace codepad::tree_sitter {
	/// Manages languages, the highlight configuration, and the highlighting thread.
	class manager {
	public:
		/// Initializes \ref _scheduler.
		manager(ui::manager &man, editors::manager &editor_man) : _manager(man), _editor_manager(editor_man) {
			_settings_changed_tok = (_manager.get_settings().changed += [this]() {
				cancel_and_wait_all_highlight_tasks();
				for (auto it = _languages.begin(); it != _languages.end(); ++it) {
					it->second->set_highlight_configuration(
						get_editor_manager().themes.get_theme_for_language(it->second->get_language_name())
					);
				}
				// TODO find a way to restart highlighting for all interpretations
			});
		}
		/// Unregisters from the settings::changed event.
		~manager() {
			_manager.get_settings().changed -= _settings_changed_tok;
		}

		/// Registers builtin languages.
		void register_builtin_languages();
		/// Registers or updates a language.
		///
		/// \return The previous configuration for that language.
		std::shared_ptr<language_configuration> register_language(
			std::u8string, std::shared_ptr<language_configuration>
		);

		/// Finds the language with the given name.
		[[nodiscard]] const language_configuration *find_lanaguage(std::u8string_view s) const {
			auto it = _languages.find(s);
			if (it == _languages.end()) {
				return nullptr;
			}
			return it->second.get();
		}


		/// Cancels all highlight tasks and waits for them to finish.
		void cancel_and_wait_all_highlight_tasks() {
			get_editor_manager().buffers.for_each_interpretation(
				[this](std::shared_ptr<editors::code::interpretation> interp) {
					auto *tag = get_tag_for(*interp);
					tag->cancel_highlight_task();
					tag->wait_for_highlight_task();
				}
			);
		}


		/// Registers for events and creates interpretation tags for all open interpretations.
		void enable() {
			_interpretation_created_token = (
				get_editor_manager().buffers.interpretation_created +=
					[this](editors::interpretation_info &info) {
						auto *lang = find_lanaguage(u8"cpp");

						auto &data = _interpretation_tag_token.get_for(info.interp);
						data.emplace<interpretation_tag>(info.interp, lang, *this);
					}
			);
			_interpretation_tag_token = get_editor_manager().buffers.allocate_interpretation_tag();

			// TODO create interpretation tags
		}
		/// Calls \ref cancel_and_wait_all_highlight_tasks() and unregisters from events.
		void disable() {
			cancel_and_wait_all_highlight_tasks();

			get_editor_manager().buffers.deallocate_interpretation_tag(_interpretation_tag_token);
			get_editor_manager().buffers.interpretation_created -= _interpretation_created_token;
		}

		/// Retrieves the \ref interpretation_tag associated with the \ref editors::code::interpretation using
		/// \ref _interpretation_tag_token. If the tag token is empty (i.e., the plugin is disabled), returns
		/// \p nullptr.
		[[nodiscard]] interpretation_tag *get_tag_for(editors::code::interpretation &interp) {
			if (_interpretation_tag_token.empty()) {
				return nullptr;
			}
			auto *tag = std::any_cast<interpretation_tag>(&_interpretation_tag_token.get_for(interp));
			assert_true_logical(tag, "missing interpretation tag while the plugin is active");
			return tag;
		}


		/// Returns \ref _manager.
		[[nodiscard]] ui::manager &get_manager() const {
			return _manager;
		}
		/// Returns \ref _editor_manager.
		[[nodiscard]] editors::manager &get_editor_manager() const {
			return _editor_manager;
		}
	protected:
		/// Mapping between language names and language configurations.
		std::unordered_map<
			std::u8string, std::shared_ptr<language_configuration>, string_hash<>, std::equal_to<>
		> _languages;

		info_event<void>::token _settings_changed_tok; ///< Used to listen to \ref settings::changed.
		/// Used to listen to \ref editors::buffer_manager::interpretation_created.
		info_event<editors::interpretation_info>::token _interpretation_created_token;

		/// Token for the interpretation tag.
		editors::buffer_manager::interpretation_tag_token _interpretation_tag_token;

		/// The manager. The \ref ui::async_task_scheduler is used for highlighting tasks, and the \ref ui::scheduler
		/// is used for trasnferring highlight results back to the main thread.
		ui::manager &_manager;
		/// The associated \ref editors::manager. This provides theme information and allows us to iterate over open
		/// buffers.
		editors::manager &_editor_manager;
	};
}
