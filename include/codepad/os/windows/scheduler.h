// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Scheduler implementation on Windows.

#include <Windows.h>

#include "codepad/ui/scheduler.h"
#include "codepad/os/windows/window.h"

namespace codepad::os {
	/// Scheduler implementation for Windows.
	class scheduler_impl : public ui::_details::scheduler_impl {
	public:
		/// Initializes \ref _thread_id.
		scheduler_impl(ui::scheduler &s) : ui::_details::scheduler_impl(s), _thread_id(GetCurrentThreadId()) {
		}

		/// Handles a message using either \ref GetMessage() or \ref PeekMessage().
		bool handle_event(ui::scheduler::wait_type ty) override {
			MSG msg;
			BOOL res;
			if (ty == ui::scheduler::wait_type::blocking) {
				res = GetMessage(&msg, nullptr, 0, 0);
				assert_true_sys(res != -1, "GetMessage error");
			} else {
				res = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
			}
			if (res != 0) {
				if (msg.message == WM_APP) { // message by scheduler::execute_callback()
					auto *func = reinterpret_cast<std::function<void()>*>(msg.lParam);
					(*func)();
					delete func;
					return true;
				}

				if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) { // handle hotkeys
					auto *form = window_impl::_get_associated_window_impl(msg.hwnd);
					if (form && owner.get_hotkey_listener().on_key_down(ui::key_gesture(
						_details::key_id_mapping_t::backward.value[msg.wParam], _details::get_modifiers()
					))) {
						return true;
					}
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				return true;
			}
			return false;
		}

		/// Sets a timer using \p SetTimer().
		void set_timer(ui::scheduler::clock_t::duration duration) {
			UINT timeout = std::chrono::duration_cast<std::chrono::duration<UINT, std::milli>>(duration).count();
			_timer_handle = SetTimer(nullptr, _timer_handle, timeout, nullptr);
			assert_true_sys(_timer_handle != 0, "failed to register timer");
		}

		/// Posts a \p WM_NULL message to wake up the main thread.
		void wake_up() {
			_details::winapi_check(PostThreadMessage(_thread_id, WM_NULL, 0, 0));
		}
	protected:
		DWORD _thread_id; ///< The thread that created this object.
		UINT_PTR _timer_handle = 0; ///< Handle of the timer.
	};
}
