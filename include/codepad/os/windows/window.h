// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <cstring>

#include <Windows.h>
#include <windowsx.h>
#include <imm.h>
#include <dwmapi.h>

#include "../../ui/manager.h"
#include "../../ui/renderer.h"
#include "../../ui/window.h"
#include "misc.h"

namespace codepad::os {
	class window : public ui::window_base {
		friend ui::element;
		friend ui::scheduler;
	public:
		using native_handle_t = HWND;

		explicit window(window *parent = nullptr) : window(u8"Codepad", parent) {
		}
		explicit window(const std::u8string &clsname, window *parent = nullptr);

		void set_caption(const std::u8string &cap) override;
		vec2d get_position() const override;
		void set_position(vec2d pos) override;
		vec2d get_client_size() const override;
		void set_client_size(vec2d sz) override;
		/// Returns the cached scaling factor, i.e., \ref _cached_scaling.
		vec2d get_scaling_factor() const override {
			return _cached_scaling;
		}

		void activate() override {
			_details::winapi_check(SetWindowPos(_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));
		}
		void prompt_ready() override;

		/// Calls \p ShowWindow() to show the window.
		void show() override {
			ShowWindow(_hwnd, SW_SHOWNA);
		}
		/// Calls \p ShowWindow() to show and activate the window.
		void show_and_activate() override {
			ShowWindow(_hwnd, SW_SHOWNORMAL);
		}
		/// Calls \p ShowWindow() to hide the window.
		void hide() override {
			ShowWindow(_hwnd, SW_HIDE);
		}

		void set_display_maximize_button(bool disp) override {
			_set_window_style_bit(disp, WS_MAXIMIZE, GWL_STYLE);
		}
		void set_display_minimize_button(bool disp) override {
			_set_window_style_bit(disp, WS_MINIMIZE, GWL_STYLE);
		}
		void set_display_caption_bar(bool disp) override {
			_set_window_style_bit(disp, WS_CAPTION ^ WS_BORDER, GWL_STYLE);
		}
		void set_display_border(bool disp) override {
			_set_window_style_bit(disp, WS_BORDER, GWL_STYLE);
		}
		void set_sizable(bool size) override {
			_set_window_style_bit(size, WS_THICKFRAME, GWL_STYLE);
		}
		/// Calls \p SetWindowPos() to set whether this window is above other normal windows.
		void set_topmost(bool topmost) override {
			_details::winapi_check(SetWindowPos(
				_hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE
			));
		}
		/// Calls \ref _set_window_style_bit() to set whether this window has a taskbar icon.
		void set_show_icon(bool show) override {
			_set_window_style_bit(!show, WS_EX_TOOLWINDOW, GWL_EXSTYLE);
		}

		bool hit_test_full_client(vec2d) const override;

		vec2d screen_to_client(vec2d) const override;
		vec2d client_to_screen(vec2d) const override;

		void set_mouse_capture(ui::element &elem) override {
			window_base::set_mouse_capture(elem);
			SetCapture(_hwnd);
		}
		void release_mouse_capture() override {
			window_base::release_mouse_capture();
			_details::winapi_check(ReleaseCapture());
		}

		void set_active_caret_position(rectd pos) override {
			_ime::get().set_caret_region(*this, pos);
		}
		void interrupt_input_method() override {
			_ime::get().cancel_composition(*this);
		}

		native_handle_t get_native_handle() const {
			return _hwnd;
		}

		inline static std::u8string_view get_default_class() {
			return u8"window";
		}
	protected:
		/// Singleton struct for handling IME events. Largely based on the implementation of chromium.
		struct _ime {
		public:
			void start_composition(window &wnd) {
				_compositing = true;
				_update_caret_position(wnd);
			}
			void update_composition(window &wnd) {
				_update_caret_position(wnd);
			}
			std::optional<std::wstring> get_composition_string(window &wnd, LPARAM lparam) {
				if (lparam & GCS_COMPSTR) {
					// TODO caret and underline
					return _get_string(wnd, GCS_COMPSTR);
				}
				return std::nullopt;
			}
			std::optional<std::wstring> get_result(window &wnd, LPARAM lparam) {
				if (lparam & GCS_RESULTSTR) {
					return _get_string(wnd, GCS_RESULTSTR);
				}
				return std::nullopt;
			}
			void cancel_composition(window &wnd) {
				_end_composition(wnd, CPS_CANCEL);
			}
			void complete_composition(window &wnd) {
				_end_composition(wnd, CPS_COMPLETE);
			}

			void on_input_language_changed() {
				_lang = LOWORD(GetKeyboardLayout(0));
			}

			void set_caret_region(window &wnd, rectd rgn) {
				_caretrgn = rgn;
				_update_caret_position(wnd);
			}

			static _ime &get();
		protected:
			rectd _caretrgn;
			LANGID _lang = LANG_USER_DEFAULT;
			bool _compositing = false;

			std::optional<std::wstring> _get_string(window&, DWORD type);
			void _update_caret_position(window&);
			void _end_composition(window&, DWORD signal);
		};

		static LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);

		/// Checks if the given window is managed by codepad and if so, retrieves a pointer to the corresponding
		/// \ref window. If it's not, returns \p nullptr.
		static window *_get_associated_window(HWND);

		vec2d _cached_scaling{1.0, 1.0}; ///< The cached scaling factor.
		HWND _hwnd; ///< The handle of this window.

		struct _wndclass {
			_wndclass();
			~_wndclass() {
				_details::winapi_check(UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr)));
			}

			ATOM atom;

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

		void _setup_mouse_tracking();

		void _on_mouse_enter() override {
			_setup_mouse_tracking();
			window_base::_on_mouse_enter();
		}

		/// Updates \ref _cached_scaling before calling the function in the base class.
		void _on_scaling_factor_changed(scaling_factor_changed_info &p) override {
			_cached_scaling = p.new_value;
			window_base::_on_scaling_factor_changed(p);
		}

		void _initialize(std::u8string_view cls) override;
		void _dispose() override;
	};

	namespace _details {
		/// Casts a \ref ui::window_base to a \ref window.
		inline window &cast_window(ui::window_base &w) {
			auto *wnd = dynamic_cast<window*>(&w);
			assert_true_usage(wnd, "invalid window type");
			return *wnd;
		}
	}
}
