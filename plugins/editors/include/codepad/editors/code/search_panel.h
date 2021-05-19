// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A panel for displaying search results.

#include <codepad/core/text.h>
#include <codepad/ui/elements/input_prompt.h>
#include <codepad/ui/elements/list_viewport.h>

#include "contents_region.h"

namespace codepad::editors::code {
	/// A search panel.
	class search_panel : public ui::input_prompt {
	public:
		/// Jumps to the selected match.
		void on_confirm() {
			// TODO
		}
		/// This should be used to close this panel instead of simply \ref scheduler::mark_for_disposal(), unless
		/// the editor is being disposed along with its children.
		void on_close() {
			_cancel_task();
			_contents->get_decoration_providers().remove_provider(_decoration_token);
			get_manager().get_scheduler().mark_for_disposal(*this);
		}

		/// Sets \ref _contents_region. Also starts the first search.
		void set_contents_region(contents_region &rgn) {
			_contents = &rgn;
			_interpretation = _contents->get_document().shared_from_this();

			_decoration_token = _contents->get_decoration_providers().add_provider(
				std::make_unique<decoration_provider>()
			);
			_begin_edit_token = _interpretation->get_buffer().begin_edit += [this](buffer::begin_edit_info&) {
				_cancel_task();
				_clear_results();
			};
			_end_edit_token = _interpretation->get_buffer().end_edit += [this](buffer::end_edit_info&) {
				_on_input_changed();
			};
			_on_input_changed();
		}


		/// Retrieves the setting entry that determines the decorations for search highlight decoration.
		static settings::retriever_parser<
			std::shared_ptr<decoration_renderer>
		> &get_decoration_renderer_setting(settings&);

		/// Returns the name for \ref _result_list.
		[[nodiscard]] inline static std::u8string_view get_result_list_name() {
			return u8"result_list";
		}
		/// Returns the default class of elements of this type.
		[[nodiscard]] inline static std::u8string_view get_default_class() {
			return u8"search_panel";
		}
	protected:
		/// The matching task.
		class _match_task : public ui::async_task_base {
		public:
			/// Interval between cancellation checks.
			constexpr static std::size_t cancellation_check_interval = 100000;

			/// Initializes all fields of this task.
			_match_task(std::u8string patt, search_panel &p) : _pattern(std::move(patt)), _parent(p) {
			}

			/// Finds all matches in the document.
			status execute() override;

			std::atomic_bool cancelled = false; ///< Used to cancel this task.
		protected:
			std::u8string _pattern; ///< The search pattern.
			search_panel &_parent; ///< The panel that created this task.
		};
		/// An item source containing all match results.
		class _match_result_source : public ui::virtual_list_viewport::item_source {
		public:
			/// Initializes \ref _parent.
			explicit _match_result_source(search_panel &parent) : _parent(&parent) {
			}

			/// Returns the number of elements in \ref _results.
			std::size_t get_item_count() const override {
				return _parent->_results.size();
			}
			/// Sets the \p position reference to the corresponding position.
			void set_item(std::size_t i, ui::reference_container &container) const override {
				if (auto ref = container.get_reference<ui::label>(u8"position")) {
					// TODO proper display
					auto str = std::to_string(_parent->_results[i].first) + " - " + std::to_string(_parent->_results[i].second);
					std::u8string_view sv(reinterpret_cast<const char8_t*>(str.c_str()), str.size());
					ref->set_text(std::u8string(sv));
				}
			}

			/// Called by the search panel when the list of results have changed.
			void on_items_changed() {
				_on_items_changed();
			}
		protected:
			search_panel *_parent = nullptr; ///< The associated \ref search_panel.
		};

		ui::async_task_scheduler::token<_match_task> _task_token; ///< Token for the asynchronous task.
		/// Token of the callback function containing match results.
		ui::scheduler::callback_token _task_result_token;

		std::vector<std::pair<std::size_t, std::size_t>> _results; ///< Current results.
		/// Token for the decoration provider.
		contents_region::view_decoration_provider_list::token _decoration_token;
		/// Used to listen to \ref interpretation::begin_edit.
		info_event<buffer::begin_edit_info>::token _begin_edit_token;
		/// Used to listen to \ref interpretation::end_edit.
		info_event<buffer::end_edit_info>::token _end_edit_token;

		ui::virtual_list_viewport *_result_list = nullptr; ///< The list of results.
		contents_region *_contents = nullptr; ///< The \ref contents_region associated with this panel.
		/// Keeps the interpretation alive until events are properly unregistered.
		std::shared_ptr<interpretation> _interpretation;

		/// Updates the search.
		void _on_input_changed() override;
		/// Called by \ref _match_task to update the results.
		void _update_results(std::vector<std::pair<std::size_t, std::size_t>>);

		/// Clears the current match results.
		void _clear_results();
		/// Cancels the match task.
		void _cancel_task();

		/// Handles \p result_list.
		bool _handle_reference(std::u8string_view name, element *e) override {
			if (name == get_result_list_name()) {
				if (_reference_cast_to(_result_list, e)) {
					auto src = std::make_unique<_match_result_source>(*this);
					_result_list->replace_source(std::move(src));
				}
				return true;
			}
			return input_prompt::_handle_reference(name, e);
		}

		/// Cancels the ongoing search task, if any.
		void _dispose() override;
	};
}
