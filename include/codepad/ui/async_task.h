// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Task scheduling.

#include <deque>
#include <functional>
#include <thread>
#include <memory>
#include <mutex>
#include <atomic>
#include <variant>
#include <semaphore>

#include "codepad/core/logging.h"

namespace codepad::ui {
	class async_task_scheduler;

	/// Base class of async tasks.
	class async_task_base {
		friend async_task_scheduler;
	public:
		///< The status of a task.
		enum class status {
			queued, ///< The task hasn't started.
			running, ///< The task has been started but hasn't finished.
			cancelled, ///< The task was cancelled midway.
			finished, ///< The task finished successfully. This doesn't mean that no cancel attempts have been made.
		};

		/// Default virtual destructor.
		virtual ~async_task_base() = default;

		/// Runs this task.
		[[nodiscard]] virtual status execute() = 0;

		/// Waits until \ref _status becomes \ref status::finished or \ref status::cancelled.
		void wait_finish() {
			_status.wait(status::queued);
			_status.wait(status::running);
		}

		/// Returns the status of this task.
		[[nodiscard]] status get_status() const {
			return _status;
		}
	private:
		std::atomic<status> _status = status::queued; ///< The status of this task.
	};

	/// Task scheduler that contains a thread pool that takes tasks from a queue. This class provides basic
	/// functionalities for starting and cancelling tasks. All tasks are assumed to be independent of one another.
	/// Unless otherwise specified, all methods should only be called from the main thread.
	class async_task_scheduler {
	public:
		/// A token that can be used to query task status or cancel a task.
		template <typename Task = async_task_base> struct token {
			friend async_task_scheduler;
		public:
			/// Default constructor.
			token() = default;
			/// Default move constructor.
			token(token&&) = default;
			/// Default copy constructor.
			token(const token&) = default;
			/// Default move assignment.
			token &operator=(token&&) = default;
			/// Default copy assignment.
			token &operator=(const token&) = default;

			/// Returns a pointer to the associated task. Returns \p nullptr if the task has already been destroyed.
			[[nodiscard]] std::shared_ptr<Task> get_task() const {
				if (std::holds_alternative<std::weak_ptr<Task>>(_task)) {
					return std::get<std::weak_ptr<Task>>(_task).lock();
				}
				return std::get<std::shared_ptr<Task>>(_task);
			}
			/// Degrades the \p std::shared_ptr in \ref _task to a \p std::weak_ptr if possible.
			void weaken() {
				if (std::holds_alternative<std::shared_ptr<Task>>(_task)) {
					auto task = std::move(std::get<std::shared_ptr<Task>>(_task));
					_task.template emplace<std::weak_ptr<Task>>(task);
				}
			}

			/// Returns whether this token contains a weak reference.
			[[nodiscard]] bool is_weak() const {
				return std::holds_alternative<std::weak_ptr<Task>>(_task);
			}
		protected:
			/// Reference to the task. If this contains an empty \p std::shared_ptr, then this token is considered
			/// empty. Otherwise this token must contain a valid reference to a task that may or may not have been
			/// destroyed. By default \ref start_task() returns a token with a strong ref; the user can opt to
			/// degrade it to a weak reference using \ref weaken(), so that the task will be destroyed immediately
			/// after it finishes. The user can then check if the task has been destroyed using \ref get_task().
			std::variant<std::shared_ptr<Task>, std::weak_ptr<Task>> _task;

			/// Initializes this token with a strong reference.
			explicit token(std::shared_ptr<Task> tsk) :
				_task(std::in_place_type<std::shared_ptr<Task>>, std::move(tsk)) {
			}
		};


		/// Initializes the thread pool with the given number of threads.
		explicit async_task_scheduler(std::size_t num_threads = std::thread::hardware_concurrency() - 1) :
			_threads(num_threads), _semaphore(0) {

			for (auto &worker : _threads) {
				worker = std::thread(
					[](async_task_scheduler &sched) {
						sched._task_thread();
					},
					std::ref(*this)
				);
			}
		}
		/// Calls \ref shutdown_and_wait().
		~async_task_scheduler() {
			shutdown_and_wait();
		}

		/// Shuts down this scheduler and waits for all worker threads to exit.
		void shutdown_and_wait() {
			if (!_shutdown) {
				// set `_shutdown` and clear the queue. `_shutdown` is set when the lock is held to avoid race
				// conditions and ensure that the queue is empty
				{
					std::lock_guard<std::mutex> lock(_lock);
					_shutdown = true;
					_queued_tasks.clear();
				}
				// wake up all threads before joining
				_semaphore.release(_threads.size());
				for (auto &worker : _threads) {
					worker.join();
				}
			}
		}

		/// Starts a new task. This function can be called from any thread - if the task scheduler has been shut
		/// down, the new task will simply be discarded.
		template <typename Task> token<Task> start_task(std::shared_ptr<Task> task) {
			// enqueue the new task
			{
				std::lock_guard<std::mutex> lock(_lock);
				// check if the scheduler has shut down to make sure the queue is always empty after shutdown.
				// this is checked here when the lock is held to avoid any possible race conditions
				if (_shutdown) {
					logger::get().log_warning(CP_HERE) <<
						"attempting to start task after shutdown; task is discarded";
					return token<Task>();
				}
				_queued_tasks.emplace_back(task);
			}
			_semaphore.release(); // wake up a worker thread
			return token<Task>(std::move(task));
		}
	protected:
		std::vector<std::thread> _threads; ///< The thread pool.

		/// The semaphore used to notify worker threads of new tasks. Its value should be the number of tasks in
		/// \ref _queued_tasks.
		std::counting_semaphore<> _semaphore;
		std::mutex _lock; ///< Protects \ref _queued_tasks.
		std::deque<std::shared_ptr<async_task_base>> _queued_tasks; ///< Queued tasks.

		std::atomic_bool _shutdown = false; ///< Whether this scheduler is shutting down.

		/// The function executed by all task threads.
		void _task_thread() {
			while (!_shutdown) {
				// wait until a new task is started
				_semaphore.acquire();
				// acquire a task
				std::shared_ptr<async_task_base> task;
				{
					std::lock_guard<std::mutex> lock(_lock);
					if (_queued_tasks.empty()) {
						continue;
					}
					task = _queued_tasks.front();
					_queued_tasks.pop_front();
				}
				// execute the task
				task->_status = async_task_base::status::running;
				task->_status.notify_all();
				auto result = task->execute();
				task->_status = result;
				task->_status.notify_all();
				// the task is then discarded; if no token with strong references are present then it'll be destoyed
			}
		}
	};
}
