// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the \ref codepad::tree_sitter::interpretation_tag class.

#include <tree_sitter/api.h>

#include <codepad/ui/async_task.h>
#include <codepad/editors/code/interpretation.h>

#include "language_configuration.h"
#include "highlight_iterator.h"

namespace codepad::tree_sitter {
	class manager;
	class interpretation_tag;

	/// Provides debug information for tree-sitter highlighting.
	class highlight_debug_tooltip_provider : public editors::code::tooltip_provider {
	public:
		/// Initializes \ref _parent.
		explicit highlight_debug_tooltip_provider(interpretation_tag &p) : _parent(&p) {
		}

		/// Lists all active captures at the given location.
		std::unique_ptr<editors::code::tooltip> request_tooltip(std::size_t) override;
	protected:
		interpretation_tag *_parent = nullptr; ///< The \ref interpretation_tag that created this provider.
	};

	/// Interface between the editor and \p tree-sitter.
	class interpretation_tag {
	public:
		/// Creates a new parser, registers to \ref editors::buffer::end_edit, and starts highlighting for this
		/// interpretation.
		interpretation_tag(editors::code::interpretation&, const language_configuration*, manager&);
		/// Assert during copy construction.
		interpretation_tag(const interpretation_tag&) {
			assert_true_logical(false, "interpretation_tag cannot be copied");
		}
		/// Assert during copy assignment.
		interpretation_tag &operator=(const interpretation_tag&) {
			assert_true_logical(false, "interpretation_tag cannot be copied");
			return *this;
		}
		/// Unregisters from \ref editors::buffer::end_edit.
		~interpretation_tag() {
			if (auto task = _task_token.get_task()) {
				// this happens when the plugin is disabled manually, in which case we just cancel the task and wait
				// for it to finish
				task->cancel();
				task->wait_finish();
			}

			_interp->get_buffer().begin_edit -= _begin_edit_token;
			_interp->get_buffer().end_edit -= _end_edit_token;
			_interp->get_theme_providers().remove_provider(_theme_token);
			_interp->remove_tooltip_provider(_debug_tooltip_provider_token);
		}

		/// Computes and returns the new highlight for the document. This function does not create a
		/// \ref editors::buffer::async_reader_lock - it is the responsibility of the caller to do so when necessary.
		[[nodiscard]] editors::code::text_theme_data compute_highlight(std::size_t *cancellation_token);

		/// Starts a new highlight task and updates \ref _task_token.
		void start_highlight_task();
		/// Cancels the ongoing highlight task, if there is one, and returns immediately.
		void cancel_highlight_task() {
			if (auto task = _task_token.get_task()) {
				task->cancel();
			}
		}
		/// Waits for the currently ongoing highlight task to finish, if there is one.
		void wait_for_highlight_task() {
			if (auto task = _task_token.get_task()) {
				task->wait_finish();
			}
		}

		/// Returns the readonly highlight data.
		[[nodiscard]] const editors::code::text_theme_data &get_highlight() const {
			return _theme_token.get_readonly();
		}

		/// Returns the current \ref language_configuration.
		[[nodiscard]] const language_configuration &get_language_configuration() const {
			return *_lang;
		}
		/// Returns the \ref editors::code::interpretation associated with this object.
		[[nodiscard]] editors::code::interpretation &get_interpretation() const {
			return *_interp;
		}
		/// Returns \ref _manager.
		[[nodiscard]] manager &get_manager() const {
			return *_manager;
		}
	protected:
		/// Contains information used in a \p TSInput.
		struct _payload {
			editors::byte_string read_buffer; ///< Intermediate buffer.
			const editors::code::interpretation &interpretation; ///< Used to read the buffer.
		};

		/// A task used for highlighting an \ref editors::code::interpretation.
		class _highlight_task : public ui::async_task_base {
		public:
			/// Initializes \ref _interp and \ref _tag.
			_highlight_task(interpretation_tag &tag) : _tag(tag) {
				_interp = _tag.get_interpretation().shared_from_this();
			}

			/// Highlights the given interpretation.
			status execute() override;
			/// Sets \ref _cancellation_token.
			void cancel() {
				std::atomic_ref<std::size_t> cancel(_cancellation_token);
				cancel = 1;
			}
		protected:
			/// The \ref editors::code::interpretation. This is here so that the interpretation (and consequently
			/// `_tag`) will not be destroyed while this task is running.
			std::shared_ptr<editors::code::interpretation> _interp;
			interpretation_tag &_tag; ///< The tag associated with \ref _interp.
			/// The cancellation token for this task. This should be accessed through a \p std::atomic_ref.
			alignas(std::atomic_ref<std::size_t>::required_alignment) std::size_t _cancellation_token = 0;
		};


		parser_ptr _parser; ///< The parser.
		const language_configuration *_lang = nullptr; ///< The language configuration.
		editors::code::interpretation *_interp = nullptr; ///< The associated interpretation.
		manager *_manager = nullptr; ///< The \ref manager that holds pointers to the \ref ui::manager.

		/// Token for \ref editors::buffer::begin_edit.
		info_event<editors::buffer::begin_edit_info>::token _begin_edit_token;
		/// Token for \ref editors::buffer::end_edit.
		info_event<editors::buffer::end_edit_info>::token _end_edit_token;

		editors::code::text_theme_provider_registry::token _theme_token; ///< Token for the theme provider.
		/// Token for the tooltip provider.
		editors::code::interpretation::tooltip_provider_token _debug_tooltip_provider_token;
		ui::async_task_scheduler::token<_highlight_task> _task_token; ///< Token for the highlight task.
	};
}
