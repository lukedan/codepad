// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to schedule the updating and rendering of elements.

#include <set>
#include <list>
#include <deque>
#include <list>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

#include "codepad/core/profiling.h"
#include "codepad/core/intrusive_priority_queue.h"
#include "commands.h"
#include "animation.h"

namespace codepad {
	namespace ui {
		class element;
		class element_collection;
		class panel;
		class window;
		class scheduler;
		namespace _details {
			class scheduler_impl;
		}
	}
	namespace os {
		class scheduler_impl;

		/// Creates a \ref scheduler_impl for the given \ref ui::scheduler object.
		std::unique_ptr<ui::_details::scheduler_impl> create_scheduler_impl(ui::scheduler&);
	}
}

namespace codepad::ui {
	/// Schedules the updating and rendering of all elements. There should at most be one active object of this type
	/// per thread.
	class scheduler {
		friend window;
		friend element_collection;
	public:
		constexpr static std::chrono::duration<double>
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline{ 0.01 },
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline{ 0.04 };
		/// The maximum number of system messages that can be processed between two updates.
		constexpr static std::size_t maximum_messages_per_update = 20;

		using clock_t = std::chrono::high_resolution_clock;
		/// Specifies if an operation should be blocking (synchronous) or non-blocking (asynchronous).
		enum class wait_type {
			blocking, ///< This operation may stall.
			non_blocking ///< This operation returns immediately.
		};


		// synchronous tasks
		/// Function type for element update tasks. These tasks are executed synchronously after a specified time
		/// point has been reached. The return value is either empty if this task should only be executed once, or
		/// a time point when this task will be executed again. These tasks are mainly used for animations.
		using sync_task_function = std::function<std::optional<clock_t::time_point>(element*)>;
	protected:
		/// Stores information about a synchronous task, including the function to execute and the position of it in
		/// the priority queue.
		struct _sync_task_info {
			/// Default constructor.
			_sync_task_info() = default;
			/// Initializes \ref task and \ref scheduled.
			_sync_task_info(
				clock_t::time_point time, sync_task_function f,
				std::map<element*, std::list<_sync_task_info>>::iterator it
			) : task(std::move(f)), scheduled(time), iter(it) {
			}

			sync_task_function task; ///< The function to execute.
			clock_t::time_point scheduled; ///< The scheduled time of execution.
			std::size_t queue_index = 0; ///< The index of this task in the priority queue.
			/// Iterator to the entry about this element in \ref _sync_tasks.
			std::map<element*, std::list<_sync_task_info>>::iterator iter;
		};
		/// An entry in the priority queue that references the related \ref _sync_task_info.
		struct _sync_task_ref {
			/// Default constructor.
			_sync_task_ref() = default;
			/// Initializes \ref info.
			explicit _sync_task_ref(std::list<_sync_task_info>::iterator i) : info(i) {
			}

			std::list<_sync_task_info>::iterator info; ///< The associated \ref _sync_task_info.

			/// Updates \ref _sync_task_info::queue_index.
			void _on_position_changed(std::size_t new_pos) {
				info->queue_index = new_pos;
			}
		};
		/// Compares \ref _sync_task_info::scheduled so that the earliest scheduled task is considered largest.
		struct _sync_task_compare {
			/// Compares \ref _sync_task_info::scheduled.
			bool operator()(const _sync_task_ref &lhs, const _sync_task_ref &rhs) const {
				return lhs.info->scheduled > rhs.info->scheduled;
			}
		};
	public:
		/// A token that can be used to cancel synchronous tasks.
		struct sync_task_token {
			friend scheduler;
		public:
			/// Default constructor.
			sync_task_token() = default;

			/// Returns \p true if this refers to an actual task. Note that the tokens are not cleared when a task is
			/// removed either after finishing execution or manually.
			bool empty() const {
				return _task == nullptr;
			}
		private:
			/// Initializes \ref _task.
			explicit sync_task_token(_sync_task_info &t) : _task(&t) {
			}

			_sync_task_info *_task = nullptr; ///< Pointer to the \ref _sync_task_info.
		};
		/// Token for a callback. Can be used to cancel the callback.
		struct callback_token {
			friend scheduler;
		public:
			/// Default constructor.
			callback_token() {
			}

			/// Attempts to cancel the callback. Regardless of whether this function is able to cancel the callback,
			/// this token will be reset.
			///
			/// \return Whether it was successfully cancelled.
			bool cancel();

			/// Tests whether this token is empty.
			[[nodiscard]] bool is_empty() const {
				return _scheduler == nullptr;
			}
			/// Tests whether this token is non-empty.
			[[nodiscard]] operator bool() const {
				return _scheduler != nullptr;
			}
		protected:
			std::list<std::function<void()>>::const_iterator _iter; ///< Iterator to the callback function.
			std::size_t _timestamp = 0; ///< Timestamp.
			scheduler *_scheduler = nullptr; ///< The \ref scheduler that created this object.

			/// Initializes all fields of the struct.
			callback_token(
				scheduler &sched, std::list<std::function<void()>>::const_iterator it, std::size_t stamp
			) : _iter(it), _timestamp(stamp), _scheduler(&sched) {
			}
		};

		/// Initializes \ref _impl.
		scheduler() {
			_impl = os::create_scheduler_impl(*this);
		}

		// layout & visual
		/// Invalidates the layout of an element. Its parent will be notified to recalculate its layout.
		void invalidate_layout(element&);
		/// Invalidates the layout of all children of a \ref panel.
		void invalidate_children_layout(panel &p) {
			_children_layout_scheduled.emplace(&p);
		}
		/// Marks the element for layout validation, meaning that its layout is valid but
		/// \ref element::_on_layout_changed() has not been called.
		void notify_layout_change(element &e) {
			assert_true_logical(!_layouting, "layout notifications are handled automatically");
			_layout_notify.emplace(&e);
		}
		/// Calculates the layout of all elements with invalidated layout.
		/// The calculation is recursive; that is, after a parent's layout has been changed,
		/// all its children are automatically marked for layout calculation.
		void update_invalid_layout();
		/// Immediately updates the layout of the given element and its descendents.
		void update_element_layout_immediate(element&);

		/// Marks the given element for re-rendering. This will re-render the whole window,
		/// but even if the visual of multiple elements in the window is invalidated,
		/// the window is still rendered once.
		void invalidate_visual(element &e) {
			_dirty.insert(&e);
		}
		/// Re-renders the windows that contain elements whose visuals are invalidated.
		void update_invalid_visuals();


		// synchronous tasks
		/// Registers a new synchronous task.
		sync_task_token register_synchronous_task(
			clock_t::time_point scheduled, element *e, sync_task_function func
		) {
			auto [it, inserted] = _sync_tasks.emplace(e, std::list<_sync_task_info>());
			_sync_task_info &task = it->second.emplace_back(scheduled, std::move(func), it);
			_sync_task_queue.emplace(--it->second.end());
			return sync_task_token(task);
		}
		/// Executes temporary and non-temporary synchronous tasks.
		void update_synchronous_tasks();
		/// Cancels the given synchronous task.
		void cancel_synchronous_task(sync_task_token &tok) {
			assert_true_usage(!tok.empty(), "empty task token");
			_cancel_sync_task(tok._task);
			tok = sync_task_token();
		}


		// focus
		/// Sets the currently focused element. When called, this function also interrupts any ongoing composition.
		/// The element must belong to a window. This function should not be called recursively.
		void set_focused_element(element*);
		/// Returns the focused element.
		[[nodiscard]] element *get_focused_element() const {
			return _focus;
		}

		/// Marks the given element for disposal. The element is only disposed when \ref dispose_marked_elements()
		/// is called. It is safe to call this multiple times before the element's actually disposed.
		void mark_for_disposal(element &e) {
			_to_delete.insert(&e);
		}
		/// Disposes all elements that has been marked for disposal. Other elements that are not marked
		/// previously but are marked for disposal during the process are also disposed.
		void dispose_marked_elements();

		/// Returns \ref _update_wait_threshold.
		[[nodiscard]] clock_t::duration get_update_waiting_threshold() const {
			return _update_wait_threshold;
		}
		/// Sets the minimum time to wait instead of 
		void set_update_waiting_threshold(clock_t::duration d) {
			_update_wait_threshold = d;
		}

		/// Simply calls \ref update_invalid_layout() and \ref update_invalid_visuals().
		void update_layout_and_visuals() {
			update_invalid_layout();
			update_invalid_visuals();
		}
		/// Calls \ref update_synchronous_tasks(), \ref dispose_marked_elements(), \ref update_scheduled_elements(),
		/// and \ref update_layout_and_visuals().
		void update() {
			performance_monitor mon(u8"Update");
			dispose_marked_elements();
			update_synchronous_tasks();
			update_layout_and_visuals();
		}

		/// Returns whether \ref update() needs to be called right now.
		[[nodiscard]] bool needs_update() const {
			return
				_num_callbacks > 0 || // callbacks
				_earliest_sync_task() <= clock_t::now() + _update_wait_threshold || // tasks
				!_to_delete.empty() || // element disposal
				!_children_layout_scheduled.empty() || !_layout_notify.empty() || // layout
				!_dirty.empty(); // visual
		}

		/// If any internal update is necessary, calls \ref update(), then calls \ref _main_iteration_system() with
		/// \ref wait_type::non_blocking until no more messages can be processed. Otherwise, waits and handles a
		/// single message from the system by calling \ref _main_iteration_system() with \ref wait_type::blocking.
		void main_iteration();

		/// Wakes the main thread up from the idle state by calling \ref _wake_up(). This function does not block.
		/// This function can be called from other threads as long as this \ref scheduler object has finished
		/// construction.
		void wake_up();
		/// Wakes the main thread up and executes the given callback function. This function does not block. This
		/// function can be called from other threads.
		callback_token execute_callback(std::function<void()> func) {
			std::size_t timestamp;
			std::list<std::function<void()>>::const_iterator it;
			{
				std::scoped_lock<std::mutex> lock(_callback_mutex);
				it = _callbacks.insert(_callbacks.end(), std::move(func));
				timestamp = _callback_timestamp;
				++_num_callbacks;
			}
			wake_up();
			return callback_token(*this, it, timestamp);
		}

		/// Returns the \ref hotkey_listener.
		[[nodiscard]] hotkey_listener &get_hotkey_listener() {
			return _hotkeys;
		}
		/// \overload
		[[nodiscard]] const hotkey_listener &get_hotkey_listener() const {
			return _hotkeys;
		}

	protected:
		hotkey_listener _hotkeys; ///< Handles hotkeys.

		/// Stores the elements whose \ref element::_on_layout_changed() need to be called.
		std::set<element*> _layout_notify;
		/// Stores the panels whose children's layout need computing.
		std::set<panel*> _children_layout_scheduled;

		std::set<element*> _dirty; ///< Stores all elements whose visuals need updating.
		std::set<element*> _to_delete; ///< Stores all elements that are to be disposed of.

		/// Keeps track of all tasks and their respective associated elements.
		std::map<element*, std::list<_sync_task_info>> _sync_tasks;
		/// Used to keep track the tasks to execute first.
		intrusive_priority_queue<_sync_task_ref, _sync_task_compare> _sync_task_queue;

		std::list<std::function<void()>> _callbacks; ///< Callback functions queued for execution.
		std::mutex _callback_mutex;///< Protects \ref _callbacks, \ref _callback_timestamp, and \ref _num_callbacks.
		/// Timestamp. This is incremented every time a `batch' of callbacks are executed, and used for cancelling
		/// callbacks.
		std::atomic_size_t _callback_timestamp = 1;
		/// The number of entries in \ref _callbacks. This allows the main thread to check for callbacks without
		/// locking. Note that while this can be read without locking, writing must be protected by
		/// \ref _callback_mutex.
		std::atomic_size_t _num_callbacks = 0;

		/// If the next update is more than this amount of time away, then set the timer and yield control to reduce
		/// resource consumption.
		clock_t::duration _update_wait_threshold{ std::chrono::microseconds(100) };

		window *_focused_window = nullptr; ///< The window that currently have system keyboard focus.
		element *_focus = nullptr; ///< Pointer to the currently focused \ref element.
		bool _layouting = false; ///< Specifies whether layout calculation is underway.

		std::unique_ptr<_details::scheduler_impl> _impl; ///< Platform-specific implementation of the scheduler.


		/// Finds the focus scope that the given \ref element is in. The element itself is not taken into account.
		/// Returns \p nullptr if the element is not in any scope (which should only happen for windows).
		[[nodiscard]] static panel *_find_focus_scope(element&);
		/// Called by \ref element_collection when an element is about to be removed from it. This function updates
		/// the innermost focus scopes, the global focus, and the capture state of the window.
		void _on_removing_element(element&);
		/// Called when a window has got or lost system keyboard focus. This function updates \ref _focused_window
		/// and calls \ref set_focused_element().
		void _on_system_focus_changed(window*);

		/// Updates the layout of the children of all given panels.
		void _update_layout(const std::set<panel*>&, std::deque<element*> notify);

		/// Cancels the given task.
		void _cancel_sync_task(_sync_task_info *tsk) {
			std::map<element*, std::list<_sync_task_info>>::iterator it = tsk->iter;
			std::size_t queue_index = tsk->queue_index;
			it->second.erase(_sync_task_queue.get_container()[queue_index].info);
			if (it->second.empty()) {
				_sync_tasks.erase(it);
			}
			if (queue_index > 0) {
				_sync_task_queue.erase(queue_index);
			} else {
				_sync_task_queue.pop();
			}
		}
		/// Returns the earliest scheduled task in \ref _sync_task_queue.
		[[nodiscard]] clock_t::time_point _earliest_sync_task() const {
			if (_sync_task_queue.empty()) {
				return clock_t::time_point::max();
			}
			return _sync_task_queue.top().info->scheduled;
		}

		/// Checks and executes all queued callbacks.
		///
		/// \return Whether any callbacks are executed.
		bool _check_and_execute_callbacks();
	};

	namespace _details {
		/// Virtual base class of platform-specific scheduler implementations.
		class scheduler_impl {
		public:
			/// Initializes \ref owner.
			scheduler_impl(scheduler &s) : owner(s) {
			}
			/// No copy & move construction.
			scheduler_impl(const scheduler_impl&) = delete;
			/// Default virtual destructor.
			virtual ~scheduler_impl() = default;

			/// Handles one event from the system message queue. The event could be a message from the system or an
			/// event generated by a filesystem watcher. If the given \ref scheduler::wait_type is
			/// \ref scheduler::wait_type::blocking, this function waits until an event has occurred, handles it, and
			/// returns. Otherwise, It checks if an event is present, and if not, immediately returns \p false.
			///
			/// \return Whether an event has been handled.
			virtual bool handle_event(scheduler::wait_type) = 0;
			/// Sets a timer that will be activated after the given amount of time. When the timer expires, this
			/// program will regain control as if calling \ref wake_up(). If this is called when a timer has
			/// previously been set and hasn't triggered yet, there's no guarantee whether it will be cancelled.
			virtual void set_timer(scheduler::clock_t::duration) = 0;
			/// Wakes the main thread up from the wait state cased by \ref handle_event(). This function can be
			/// called from other threads as long as this \ref scheduler object has finished construction.
			virtual void wake_up() = 0;

			scheduler &owner; ///< The \ref scheduler that uses this implementation.
		};
	}
}
