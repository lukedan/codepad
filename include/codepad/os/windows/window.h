// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of \ref codepad::ui::_details::window_impl on windows.

#include <cstring>

#include <Windows.h>
#include <windowsx.h>
#include <imm.h>
#include <dwmapi.h>

#include "codepad/ui/manager.h"
#include "codepad/ui/renderer.h"
#include "codepad/ui/window.h"
#include "misc.h"

namespace codepad::os {
	/// Implementation of windows using WinAPI.
	class window_impl : public ui::_details::window_impl {
		friend scheduler_impl;
	public:
		using native_handle_t = HWND; ///< Native handle.

		/// Creates a new window.
		window_impl(ui::window&);
		/// Calls \p DestroyWindow().
		~window_impl();

		/// Returns the \p HWND of this window.
		native_handle_t get_native_handle() const {
			return _hwnd;
		}
	protected:
		// PIMPL functions
		/// Sets the owner of this window using \p SetWindowLongPtr().
		void _set_parent(ui::window *wnd) override;

		/// Sets the caption of this window using \p SetWindowText().
		void _set_caption(const std::u8string &cap) override;

		/// Returns the result of \p ClientToScreen(0, 0).
		vec2d _get_position() const override;
		/// Finds the offset of the client area using \p GetWindowRect() and \p ClientToScreen(), then sets the
		/// position using \p SetWindowPos().
		void _set_position(vec2d) override;
		/// Returns the result of \p GetClientRect() scaled by \ref _physical_to_logical_position().
		vec2d _get_client_size() const override;
		/// Retrieves the offset of the client area using \p GetWindowRect() and \p ClientToScreen(), then sets the
		/// size of the client area using \p SetWindowPos(), without altering the position of the top-left corner of
		/// the window's client area on screen.
		void _set_client_size(vec2d) override;
		/// Returns the cached scaling factor, i.e., \ref _cached_scaling.
		vec2d _get_scaling_factor() const override {
			return _cached_scaling;
		}

		/// Activates this window by calling \p SetWindowPos() with \p HWND_TOP.
		void _activate() override {
			if (_hwnd) {
				_details::winapi_check(SetWindowPos(_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));
			}
		}
		void _prompt_ready() override;

		/// Calls \p ShowWindow() with \p SW_SHOWNA to show the window.
		void _show() override {
			if (_hwnd) {
				ShowWindow(_hwnd, SW_SHOWNA);
			}
		}
		/// Calls \p ShowWindow() with \p SW_SHOWNORMAL to show and activate the window.
		void _show_and_activate() override {
			if (_hwnd) {
				ShowWindow(_hwnd, SW_SHOWNORMAL);
			}
		}
		/// Calls \p ShowWindow() with \p SW_HIDE to hide the window.
		void _hide() override {
			if (_hwnd) {
				ShowWindow(_hwnd, SW_HIDE);
			}
		}

		/// Calls \p InvalidateRect().
		void _invalidate_window_visuals() override {
			if (_hwnd) {
				_details::winapi_check(InvalidateRect(_hwnd, nullptr, false));
			}
		}

		/// Calls \ref _set_window_style_bit() to set the \p WS_MAXIMIZE bit.
		void _set_display_maximize_button(bool disp) override {
			_set_window_style_bit(disp, WS_MAXIMIZE, GWL_STYLE);
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_MINIMIZE bit.
		void _set_display_minimize_button(bool disp) override {
			_set_window_style_bit(disp, WS_MINIMIZE, GWL_STYLE);
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_DLGFRAME bit.
		void _set_display_caption_bar(bool disp) override {
			_set_window_style_bit(disp, WS_DLGFRAME, GWL_STYLE);
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_BORDER bit.
		void _set_display_border(bool disp) override {
			_set_window_style_bit(disp, WS_BORDER, GWL_STYLE);
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_THICKFRAME bit.
		void _set_sizable(bool size) override {
			_set_window_style_bit(size, WS_THICKFRAME, GWL_STYLE);
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_EX_NOACTIVATE bit.
		void _set_focusable(bool vis) override {
			_focusable = vis;
			// it seems that handling WM_MOUSEACTIVATE is enough to make it non-focusable on mouse click, but WPF
			// uses this as well [maybe for bookkeeping? or maybe other cases (the narrator?) can active the window?]
			// so we're keeping this here
			_set_window_style_bit(!vis, WS_EX_NOACTIVATE, GWL_EXSTYLE);
		}

		/// Calls \p SetWindowPos() to set whether this window is above other normal windows.
		void _set_topmost(bool topmost) override {
			if (_hwnd) {
				_details::winapi_check(SetWindowPos(
					_hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
					SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
				));
			}
		}
		/// Calls \ref _set_window_style_bit() to set the \p WS_EX_TOOLWINDOW and \p WS_EX_APPWINDOW bits.
		void _set_show_icon(bool show) override {
			_set_window_style_bit(!show, WS_EX_TOOLWINDOW, GWL_EXSTYLE);
		}

		/// Converts client positions in logical pixels to screen coordinates using \p ScreenToClient().
		vec2d _screen_to_client(vec2d) const override;
		/// Converts screen coordinates to client positions in logical pixels using \p ClientToScreen().
		vec2d _client_to_screen(vec2d) const override;

		void _set_active_caret_position(rectd pos) override {
			_ime::get().set_caret_region(*this, pos);
		}
		void _interrupt_input_method() override {
			_ime::get().cancel_composition(*this);
		}

		/// Calls \p SetCapture().
		void _set_mouse_capture() override {
			if (_hwnd) {
				SetCapture(_hwnd);
			}
		}
		/// Calls \p ReleaseCapture().
		void _release_mouse_capture() override {
			_details::winapi_check(ReleaseCapture());
		}

		/// Calls \ref _update_managed_window_size().
		void _on_size_policy_changed() override {
			_update_managed_window_size();
		}
		/// Resizes the window to satisfy the width and height requested by the application.
		void _update_managed_window_size() override;

		// other private members
		/// Singleton struct for handling IME events. Largely based on the implementation of chromium.
		struct _ime {
		public:
			void start_composition(window_impl &wnd) {
				_compositing = true;
				_update_caret_position(wnd);
			}
			void update_composition(window_impl &wnd) {
				_update_caret_position(wnd);
			}
			std::optional<std::wstring> get_composition_string(window_impl &wnd, LPARAM lparam) {
				if (lparam & GCS_COMPSTR) {
					// TODO caret and underline
					return _get_string(wnd, GCS_COMPSTR);
				}
				return std::nullopt;
			}
			std::optional<std::wstring> get_result(window_impl &wnd, LPARAM lparam) {
				if (lparam & GCS_RESULTSTR) {
					return _get_string(wnd, GCS_RESULTSTR);
				}
				return std::nullopt;
			}
			void cancel_composition(window_impl &wnd) {
				_end_composition(wnd, CPS_CANCEL);
			}
			void complete_composition(window_impl &wnd) {
				_end_composition(wnd, CPS_COMPLETE);
			}

			void on_input_language_changed() {
				_lang = LOWORD(GetKeyboardLayout(0));
			}

			void set_caret_region(window_impl &wnd, rectd rgn) {
				_caretrgn = rgn;
				_update_caret_position(wnd);
			}

			static _ime &get();
		protected:
			rectd _caretrgn;
			LANGID _lang = LANG_USER_DEFAULT;
			bool _compositing = false;

			std::optional<std::wstring> _get_string(window_impl&, DWORD type);
			void _update_caret_position(window_impl&);
			void _end_composition(window_impl&, DWORD signal);
		};

		/// The window procedure.
		static LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);
		
		/// Checks if the given window is managed by codepad and if so, retrieves a pointer to the corresponding
		/// \ref window_impl. If it's not, returns \p nullptr.
		static window_impl *_get_associated_window_impl(HWND);

		/// Token for window resizing cased by \ref _update_managed_window_size().
		ui::scheduler::callback_token _size_callback;
		vec2d _cached_scaling{1.0, 1.0}; ///< The cached scaling factor.
		/// The handle of this window. All functions of this class are protected against when this is \p nullptr.
		/// This is because these functions can be called due to \p WM_DESTROY marking this window for disposal, at
		/// which point the window handle is no longer valid.
		HWND _hwnd = nullptr;
		bool
			/// Whether mouse movement is being tracked. This is used to record whether \p TrackMouseEvent() has been
			/// called, so that this window receives mouse leave and hover events.
			_mouse_tracked = false,
			/// Whether this window should gain focus when the user clicks on it.
			_focusable = true;

		/// Contains the window class used by all windows.
		struct _wndclass {
			/// Registers the window class by calling \p RegisterClassEx().
			_wndclass();
			/// No copy/move construction.
			_wndclass(const _wndclass&) = delete;
			/// No copy/move assignment.
			_wndclass &operator=(const _wndclass&) = delete;
			/// Unregisters this class using \p UnregisterClass().
			~_wndclass() {
				_details::winapi_check(UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr)));
			}

			ATOM atom; ///< The atom for the window class.

			/// Returns the global \ref _wndclass object.
			static _wndclass &get();
		};

		/// Converts a physical position into a logical position by dividing it with \ref _cached_scaling.
		vec2d _physical_to_logical_position(vec2d pos) const {
			return vec2d(pos.x / _cached_scaling.x, pos.y / _cached_scaling.y);
		}
		/// Converts a logical position into a physical position by scaling it with \ref _cached_scaling.
		vec2d _logical_to_physical_position(vec2d pos) const {
			return vec2d(pos.x * _cached_scaling.x, pos.y * _cached_scaling.y);
		}

		/// Sets the specified bites to the given value by calling \p SetWindowLong().
		void _set_window_style_bit(bool v, LONG bit, int type);

		/// Checks if the mouse position is being tracked. If not, calls \p TrackMouseEvent() so that the window
		/// receives hover and mouse leave events.
		void _setup_mouse_tracking();
	};

	namespace _details {
		/// Casts a \ref ui::window_impl to a \ref window_impl.
		[[nodiscard]] inline window_impl &cast_window_impl(ui::_details::window_impl &w) {
			auto *wnd = dynamic_cast<window_impl*>(&w);
			assert_true_usage(wnd, "invalid window_impl type");
			return *wnd;
		}
	}
}
