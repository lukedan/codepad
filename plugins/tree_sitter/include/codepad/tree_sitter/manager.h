// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Global configurations, data, and the highlighting thread.

#include <deque>
#include <semaphore>

#include <codepad/editors/code/interpretation.h>

#include "language_configuration.h"
#include "interpretation_tag.h"

namespace codepad::tree_sitter {
	/// Manages languages, the highlight configuration, and the highlighting thread.
	class manager {
	public:
		/// Initializes \ref _scheduler.
		manager(ui::scheduler &sched) : _scheduler(sched) {
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
		const language_configuration *find_lanaguage(const std::u8string &s) const {
			auto it = _languages.find(s);
			if (it == _languages.end()) {
				return nullptr;
			}
			return it->second.get();
		}

		/// Sets \ref _highlight_config and configures all registered languages.
		void set_highlight_configuration(std::shared_ptr<highlight_configuration>);
		/// Returns \ref _highlight_config.
		const std::shared_ptr<highlight_configuration> &get_highlight_config() const {
			return _highlight_config;
		}

		/// Starts the highlighter thread.
		void start_highlighter_thread() {
			_highlighter_thread_status expected = _highlighter_thread_status::stopped;
			assert_true_logical(
				_status.compare_exchange_strong(expected, _highlighter_thread_status::running),
				"highlighter thread is running"
			);
			_highlighter_thread_obj = std::thread(
				[](manager *man) {
					man->_highlighter_thread();
				},
				this
			);
		}
		/// Signals the highlighter thread to stop and waits for it to finish.
		void stop_highlighter_thread() {
			_highlighter_thread_status expected = _highlighter_thread_status::running;
			assert_true_logical(
				_status.compare_exchange_strong(expected, _highlighter_thread_status::stopping),
				"highlighter thread is not running"
			);
			_semaphore.release(); // wake up the highlighter thread if it isn't
			_highlighter_thread_obj.join();
			_status = _highlighter_thread_status::stopped;
		}
		/// Queues the interpretation associated with the given \ref interpretation_tag for highlighting. If
		/// the interpretation is currently being highlighted, the previously queued operation will be cancelled.
		void queue_highlighting(interpretation_tag &interp) {
			std::lock_guard<std::mutex> guard(_lock);
			_cancel_highlighting(interp);
			_queued.emplace_back(&interp);
			_semaphore.release();
		}
		/// Cancels all pending or ongoing highlighting operations for the given interpretation.
		void cancel_highlighting(interpretation_tag &interp) {
			std::lock_guard<std::mutex> guard(_lock);
			_cancel_highlighting(interp);
		}
	protected:
		/// Status of the highlighter thread.
		enum class _highlighter_thread_status : unsigned char {
			running, ///< The thread is running.
			stopping, ///< The thread is running, but the manager has requested it to stop.
			stopped ///< The thread is **not** running.
		};

		/// Mapping between language names and language configurations.
		std::unordered_map<std::u8string, std::shared_ptr<language_configuration>> _languages;
		std::shared_ptr<highlight_configuration> _highlight_config; ///< Highlight configuration.

		/// Used to signal the highlighter thread that a new interpretation has been queued for highlighting, or when
		/// requesting the highlighter thread to shutdown.
		std::counting_semaphore<255> _semaphore{ 0 };
		std::mutex _lock; ///< Protects \ref _queued and \ref _active.
		std::thread _highlighter_thread_obj; ///< The highlighter thread.
		/// Interpretations queued for highlighting. This is protected by \ref _lock.
		std::deque<interpretation_tag*> _queued;
		/// The interpretation that is currently being highlighted. This is protected by \ref _lock.
		interpretation_tag *_active = nullptr;
		/// The status of the highlighter thread.
		std::atomic<_highlighter_thread_status> _status = _highlighter_thread_status::stopped;
		/// The cancellation token for the highlighter thread. This should be accessed through a \p std::atomic_ref.
		alignas(std::atomic_ref<std::size_t>::required_alignment) std::size_t _cancellation_token = 0;
		/// The scheduler. Used for trasnferring highlight results back to the main thread.
		ui::scheduler &_scheduler;

		/// The function executed by the highlighter thread.
		void _highlighter_thread();

		/// Cancels all pending or ongoing highlighting operations for the given interpretation. This function
		/// assumes that the caller already holds \ref _lock.
		void _cancel_highlighting(interpretation_tag &interp) {
			if (_active == &interp) {
				std::atomic_ref<std::size_t> cancel(_cancellation_token);
				cancel = 1;
			}
			// search in _queued
			// we're not expecting a lot of interpretations to be queued, so just go over them one by one
			for (auto iter = _queued.begin(); iter != _queued.end(); ) {
				if (*iter == &interp) {
					iter = _queued.erase(iter);
				} else {
					++iter;
				}
			}
		}
	};
}
