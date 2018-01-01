#pragma once

#include <list>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "misc.h"

namespace codepad {
	class callback_buffer {
	public:
		template <typename T> void add(T &&func) {
			std::lock_guard<std::mutex> guard(_lock);
			_cbs.emplace_back(std::forward<T>(func));
		}
		void flush() {
			_lock.lock();
			if (!_cbs.empty()) {
				std::vector<std::function<void()>> list = std::move(_cbs);
				_cbs.clear();
				_lock.unlock();
				for (auto &&f : list) {
					f();
				}
			} else {
				_lock.unlock();
			}
		}

		static callback_buffer &get();
	protected:
		std::mutex _lock;
		std::vector<std::function<void()>> _cbs;
	};

	/* pool thread:
	 *   not_initiated -> cancel_requested
	 *   running       -> cancel_requested
	 *
	 * task thread:
	 *   not_initiated    -> running
	 *   running          -> completed
	 *   cancel_requested -> cancelled
	 */
	enum class task_status {
		not_initiated, // TODO useful?
		running,
		cancel_requested,
		completed,
		cancelled
	};
	class async_task_pool {
	public:
#ifdef CP_DETECT_USAGE_ERRORS
		async_task_pool() : _creator(std::this_thread::get_id()) {
		}
#endif

		struct async_task {
			friend class async_task_pool;
		public:
			using operation_t = std::function<void(async_task&)>;

			explicit async_task(operation_t f) : operation(std::move(f)) {
			}

			task_status get_status() const {
				return _st;
			}
			bool is_cancel_requested() const {
				return _st.load() == task_status::cancel_requested;
			}
			bool is_finished() const {
				task_status cst = _st.load();
				return cst == task_status::completed || cst == task_status::cancelled;
			}

			template <typename Func> func_invoke_result_t<Func()> acquire_data(Func &&f) {
				semaphore sem;
				func_invoke_result_t<Func()> res;
				callback_buffer::get().add([&f, &sem, &res]() {
					res = f();
					sem.signal();
					});
				sem.wait();
				return res;
			}

			const operation_t operation;
		protected:
			std::atomic<task_status> _st{task_status::not_initiated};

			void _run() {
				task_status exp = task_status::not_initiated;
				if (_st.compare_exchange_strong(exp, task_status::running)) {
					operation(*this);
					exp = task_status::running;
					if (!_st.compare_exchange_strong(exp, task_status::completed)) {
						_st = task_status::cancelled;
					}
				} else {
					assert_true_logical(exp == task_status::cancel_requested, "");
					_st = task_status::cancelled;
				}
			}
		};
		using token = std::list<async_task>::iterator;

		template <typename T, typename ...Args> token run_task(T &&func, Args &&...args) {
			assert_true_usage(std::this_thread::get_id() == _creator, "cannot run task from other threads");
			_lst.emplace_back(async_task::operation_t(std::bind(
				std::forward<T>(func), std::forward<Args>(args)..., std::placeholders::_1
			)));
			token tk = --_lst.end();
			std::thread t(std::bind(&async_task::_run, &*tk));
			t.detach();
			return tk;
		}
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
		bool try_finish(token t) {
			task_status dummy;
			return try_finish(t, dummy);
		}
		bool try_finish(token t, task_status &st) {
			st = t->_st;
			if (st == task_status::cancelled || st == task_status::completed) {
				_lst.erase(t);
				return true;
			}
			return false;
		}
		task_status wait_finish(token t) {
			while (!t->is_finished()) {
			}
			task_status fstat = t->get_status();
			_lst.erase(t);
			return fstat;
		}

		std::list<async_task> &tasks() {
			return _lst;
		}

		static async_task_pool &get();
	protected:
		std::list<async_task> _lst;
#ifdef CP_DETECT_USAGE_ERRORS
		std::thread::id _creator;
#endif
	};
}
