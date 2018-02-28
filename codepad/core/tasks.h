#pragma once

/// \file tasks.h
/// Classes for executing asynchronous tasks. Work in progress.

#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "misc.h"

namespace codepad {
	/// Used to buffer callbacks from other threads and execute them in the main thread when appropriate.
	class callback_buffer {
	public:
		/// Adds a callback that's to be executed.
		template <typename T> void add(T &&func) {
			std::unique_lock<std::mutex> lck(_lock);
			_cbs.emplace_back(std::forward<T>(func));
		}
		/// Executes all callbacks added so far and clear the buffer. This is to be called in the main thread.
		void flush() {
			_lock.lock();
			if (!_cbs.empty()) {
				// swap and then unlock so that execution can be done later without locking
				std::vector<std::function<void()>> list;
				std::swap(list, _cbs);
				_lock.unlock();
				// execute all callbacks
				for (auto &&f : list) {
					f();
				}
			} else {
				_lock.unlock();
			}
		}

		/// Gets the global \ref callback_buffer.
		static callback_buffer &get();
	protected:
		std::mutex _lock; ///< Locks when adding and flushing callbacks.
		std::vector<std::function<void()>> _cbs; ///< Stores all callbacks.
	};

	/// Represents the status of a task.
	/// Possible transitions in the main thread:
	/// <table>
	///   <tr><th>From</th><th>To</th></tr>
	///   <tr><td>not_initiated</td><td>cancel_requested</td></tr>
	///   <tr><td>running</td><td>cancel_requested</td></tr>
	/// </table>
	/// Possible transitions in the task thread:
	/// <table>
	///   <tr><th>From</th><th>To</th></tr>
	///   <tr><td>not_initiated</td><td>running</td></tr>
	///   <tr><td>running</td><td>completed</td></tr>
	///   <tr><td>cancel_requested</td><td>cancelled</td></tr>
	/// </table>
	///
	/// \todo Not sure if state \p not_initiated is useful.
	enum class task_status {
		not_initiated, ///< The task has not yet started.
		running, ///< The task is running.
		cancel_requested, ///< The task has been required to be cancelled, but is still running / pending.
		completed, ///< The task has completed normally.
		cancelled ///< The task has been cancelled and has ended.
	};
	/// Struct that holds an asynchronous task.
	struct async_task {
		friend class async_task_pool;
	public:
		/// The type of operation that it executes:
		/// a function that accepts an \ref async_task and returns nothing.
		using operation_t = std::function<void(async_task&)>;

		/// Constructs a task from a function object.
		explicit async_task(operation_t f) : _op(std::move(f)) {
		}

		/// Returns the current status of the task. Note that the result is only an approximation.
		task_status get_status() const {
			return _st;
		}
		/// Returns true if the task has been requested to be cancelled.
		/// This is useful for the running task to determine if it should stop midway.
		bool is_cancel_requested() const {
			return _st.load() == task_status::cancel_requested;
		}
		/// Returns true if the task has ended (either finished processing or cancelled).
		bool is_finished() const {
			task_status cst = _st.load();
			return cst == task_status::completed || cst == task_status::cancelled;
		}

		/// Used by the task thread to acquire data from the main thread,
		/// since most components of \p codepad are not thread-safe.
		///
		/// \return The value returned by the function.
		template <typename Func> std::invoke_result_t<Func> acquire_data(const Func &f) {
			semaphore sem;
			std::invoke_result_t<Func> res;
			callback_buffer::get().add([&f, &sem, &res]() {
				res = f();
				sem.signal();
				});
			sem.wait();
			return res;
		}
	protected:
		operation_t _op; ///< The operation executed.
		std::atomic<task_status> _st{task_status::not_initiated}; ///< The status of the task.

		/// Runs the task. This function is executed by \ref async_task_pool in another thread.
		void _run() {
			task_status exp = task_status::not_initiated;
			if (_st.compare_exchange_strong(exp, task_status::running)) {
				_op(*this); // run the actual task
				exp = task_status::running;
				if (!_st.compare_exchange_strong(exp, task_status::completed)) { // cancelled
					assert_true_logical(exp == task_status::cancel_requested, "");
					_st = task_status::cancelled;
				}
			} else { // cancelled before starting
				assert_true_logical(exp == task_status::cancel_requested, "");
				_st = task_status::cancelled;
			}
		}
	};
	/// Used to store all running asynchronous tasks.
	class async_task_pool {
	public:
		/// Returned when starting a task, to be used to check its status, cancel it, etc.
		using token = std::list<async_task>::iterator;

		/// Default constructor.
		async_task_pool() : _creator(std::this_thread::get_id()) {
		}

		/// Runs a task and returns the corresponding \ref token.
		///
		/// \param func The function.
		/// \param args Arguments for \p func.
		template <typename T, typename ...Args> token run_task(T &&func, Args &&...args) {
			assert_true_usage(std::this_thread::get_id() == _creator, "cannot run task from other threads");
			_lst.emplace_back(async_task::operation_t(std::bind(
				std::forward<T>(func), std::forward<Args>(args)..., std::placeholders::_1
			)));
			auto tk = --_lst.end();
			std::thread t([tsk = &*tk]() {
				tsk->_run();
			});
			t.detach();
			return tk;
		}
		/// Cancels a task if possible.
		///
		/// \param t The token returned by \ref run_task.
		/// \return \p false if the task has already finished; \p true otherwise.
		bool try_cancel(token t) {
			task_status exp = task_status::not_initiated;
			if (!t->_st.compare_exchange_strong(exp, task_status::cancel_requested)) {
				exp = task_status::running;
				if (!t->_st.compare_exchange_strong(exp, task_status::cancel_requested)) {
					return false;
				}
			}
			return true;
		}
		/// Checks if a task has finished, and if so, removes it from the task pool.
		/// The token is no longer valid after the task's removed.
		///
		/// \param t The token returned by \ref run_task.
		/// \param st Returns the status of the task, regardless of whether it has finished.
		/// \return \p true if the task has finished and has been removed.
		bool check_finish(token t, task_status &st) {
			st = t->_st;
			if (st == task_status::cancelled || st == task_status::completed) {
				_lst.erase(t);
				return true;
			}
			return false;
		}
		/// \overload
		bool check_finish(token t) {
			task_status dummy;
			return check_finish(t, dummy);
		}

		/// Called before closing the application, to cancel all tasks and wait for them to finish.
		///
		/// \todo Each component should manage their own tasks; there should be
		///       no running tasks when the application is about to shut down.
		void shutdown() {
			for (auto i = _lst.begin(); i != _lst.end(); ++i) {
				try_cancel(i);
			}
			while (!_lst.empty()) {
				for (auto i = _lst.begin(); i != _lst.end(); ++i) {
					if (check_finish(i)) {
						break;
					}
				}
			}
		}

		/// Returns a read-only list of all currently registered tasks.
		const std::list<async_task> &tasks() {
			return _lst;
		}

		/// Gets the global \ref async_task_pool.
		static async_task_pool &get();
	protected:
		std::list<async_task> _lst; ///< The list of tasks.
		std::thread::id _creator; ///< The thread that created this task pool.
	};
}
