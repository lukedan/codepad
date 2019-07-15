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
	class software_renderer;
	class opengl_renderer;

	class window : public ui::window_base {
		friend class software_renderer;
		friend class opengl_renderer;
		friend ui::element;
		friend ui::scheduler;
	public:
		using native_handle_t = HWND;

		explicit window(window *parent = nullptr) : window(CP_STRLIT("Codepad"), parent) {
		}
		explicit window(const str_t &clsname, window *parent = nullptr) {
			auto u16str = _details::utf8_to_wstring(clsname.c_str());
			_wndclass::get();
			winapi_check(_hwnd = CreateWindowEx(
				WS_EX_ACCEPTFILES, reinterpret_cast<LPCTSTR>(static_cast<size_t>(_wndclass::get().atom)),
				reinterpret_cast<LPCTSTR>(u16str.c_str()), WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				parent ? parent->_hwnd : nullptr, nullptr, GetModuleHandle(nullptr), nullptr
			));
			winapi_check(_dc = GetDC(_hwnd));
		}

		void set_caption(const str_t &cap) override {
			auto u16str = _details::utf8_to_wstring(cap.c_str());
			winapi_check(SetWindowText(_hwnd, reinterpret_cast<LPCWSTR>(u16str.c_str())));
		}
		vec2i get_position() const override {
			POINT tl;
			tl.x = tl.y = 0;
			winapi_check(ClientToScreen(_hwnd, &tl));
			return vec2i(tl.x, tl.y);
		}
		void set_position(vec2i pos) override {
			RECT r;
			POINT tl;
			tl.x = tl.y = 0;
			winapi_check(GetWindowRect(_hwnd, &r));
			winapi_check(ClientToScreen(_hwnd, &tl));
			tl.x -= r.left;
			tl.y -= r.top;
			winapi_check(SetWindowPos(_hwnd, nullptr, pos.x - tl.x, pos.y - tl.y, 0, 0, SWP_NOSIZE));
		}
		vec2i get_client_size() const override {
			RECT r;
			winapi_check(GetClientRect(_hwnd, &r));
			return vec2i(r.right, r.bottom);
		}
		void set_client_size(vec2i sz) override {
			RECT wndrgn, cln;
			winapi_check(GetWindowRect(_hwnd, &wndrgn));
			winapi_check(GetClientRect(_hwnd, &cln));
			winapi_check(SetWindowPos(
				_hwnd, nullptr, 0, 0,
				wndrgn.right - wndrgn.left - cln.right + sz.x,
				wndrgn.bottom - wndrgn.top - cln.bottom + sz.y,
				SWP_NOMOVE
			));
		}

		void activate() override {
			winapi_check(SetWindowPos(_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE));
		}
		void prompt_ready() override {
			FLASHWINFO fwi;
			std::memset(&fwi, 0, sizeof(fwi));
			fwi.cbSize = sizeof(fwi);
			fwi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
			fwi.dwTimeout = 0;
			fwi.hwnd = _hwnd;
			fwi.uCount = 0;
			FlashWindowEx(&fwi);
		}

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
			winapi_check(SetWindowPos(
				_hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE
			));
		}
		/// Calls \ref _set_window_style_bit() to set whether this window has a taskbar icon.
		void set_show_icon(bool show) override {
			_set_window_style_bit(!show, WS_EX_TOOLWINDOW, GWL_EXSTYLE);
		}
		bool hit_test_full_client(vec2i v) const override {
			RECT r;
			winapi_check(GetWindowRect(_hwnd, &r));
			return r.left <= v.x && r.right > v.x && r.top <= v.y && r.bottom > v.y;
		}

		vec2i screen_to_client(vec2i v) const override {
			POINT p;
			p.x = v.x;
			p.y = v.y;
			winapi_check(ScreenToClient(_hwnd, &p));
			return vec2i(p.x, p.y);
		}
		vec2i client_to_screen(vec2i v) const override {
			POINT p;
			p.x = v.x;
			p.y = v.y;
			winapi_check(ClientToScreen(_hwnd, &p));
			return vec2i(p.x, p.y);
		}

		void set_mouse_capture(ui::element &elem) override {
			window_base::set_mouse_capture(elem);
			SetCapture(_hwnd);
		}
		void release_mouse_capture() override {
			window_base::release_mouse_capture();
			winapi_check(ReleaseCapture());
		}

		void set_active_caret_position(rectd pos) override {
			_ime::get().set_caret_region(*this, pos.fit_grid_enlarge<int>());
		}
		void interrupt_input_method() override {
			_ime::get().cancel_composition(*this);
		}

		native_handle_t get_native_handle() const {
			return _hwnd;
		}

		inline static str_view_t get_default_class() {
			return CP_STRLIT("window");
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
			bool get_composition_string(window &wnd, LPARAM lparam, std::wstring &str) {
				if (lparam & GCS_COMPSTR) {
					// TODO caret and underline
					return _get_string(wnd, GCS_COMPSTR, str);
				}
				return false;
			}
			bool get_result(window &wnd, LPARAM lparam, std::wstring &str) {
				if (lparam & GCS_RESULTSTR) {
					return _get_string(wnd, GCS_RESULTSTR, str);
				}
				return false;
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

			void set_caret_region(window &wnd, recti rgn) {
				_caretrgn = rgn;
				_update_caret_position(wnd);
			}

			static _ime &get();
		protected:
			recti _caretrgn;
			LANGID _lang = LANG_USER_DEFAULT;
			bool _compositing = false;

			bool _get_string(window &wnd, DWORD type, std::wstring &res) {
				HIMC context = ImmGetContext(wnd._hwnd);
				if (context) {
					LONG buffersz = ImmGetCompositionString(context, type, nullptr, 0);
					assert_true_sys(buffersz != IMM_ERROR_GENERAL, "general IME error");
					if (buffersz != IMM_ERROR_NODATA) {
						res.resize(buffersz / sizeof(wchar_t));
						assert_true_sys(
							ImmGetCompositionString(context, type, res.data(), buffersz) == buffersz,
							"failed to obtain string from IME"
						);
						return true;
					}
				}
				return false;
			}

			void _update_caret_position(window &wnd) {
				HIMC context = ImmGetContext(wnd._hwnd);
				if (context) {
					CANDIDATEFORM rgn{};
					rgn.dwIndex = 0;
					rgn.dwStyle = CFS_CANDIDATEPOS;
					rgn.ptCurrentPos.x = _caretrgn.xmin;
					rgn.ptCurrentPos.y = _caretrgn.ymax;
					winapi_check(ImmSetCandidateWindow(context, &rgn));

					rgn.dwStyle = CFS_EXCLUDE;
					rgn.ptCurrentPos.x = _caretrgn.xmin;
					rgn.ptCurrentPos.y = _caretrgn.ymin;
					rgn.rcArea.left = _caretrgn.xmin;
					rgn.rcArea.right = _caretrgn.xmax;
					rgn.rcArea.top = _caretrgn.ymin;
					rgn.rcArea.bottom = _caretrgn.ymax;
					winapi_check(ImmSetCandidateWindow(context, &rgn));

					winapi_check(ImmReleaseContext(wnd._hwnd, context));
				}

				if (_compositing) {
					winapi_check(CreateCaret(wnd._hwnd, nullptr, _caretrgn.width(), _caretrgn.height()));
					winapi_check(SetCaretPos(_caretrgn.xmin, _caretrgn.ymin));
				}
			}

			void _end_composition(window &wnd, DWORD signal) {
				if (_compositing) {
					DestroyCaret();
					HIMC context = ImmGetContext(wnd._hwnd);
					if (context) {
						winapi_check(ImmNotifyIME(context, NI_COMPOSITIONSTR, signal, 0));
						winapi_check(ImmReleaseContext(wnd._hwnd, context));
					}
					_compositing = false;
				}
			}
		};

		static LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);

		/// Checks if the given window is managed by codepad and if so, retrieves a pointer to the corresponding
		/// \ref window. If it's not, returns \p nullptr.
		inline static window *_get_associated_window(HWND hwnd) {
			if (hwnd) {
				DWORD atom;
				winapi_check(atom = GetClassLong(hwnd, GCW_ATOM));
				if (atom == _wndclass::get().atom) { // the correct class
					return reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
				}
			}
			return nullptr;
		}

		HWND _hwnd;
		HDC _dc; // TODO is this useful?

		struct _wndclass {
			_wndclass();
			~_wndclass() {
				winapi_check(UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr)));
			}

			ATOM atom;

			static _wndclass &get();
		};

		/// Sets the specified bites to the given value by calling \p SetWindowLong().
		void _set_window_style_bit(bool v, LONG bit, int type) {
			LONG old = GetWindowLong(_hwnd, type);
			SetWindowLong(_hwnd, type, v ? old | bit : old & ~bit);
			winapi_check(SetWindowPos(
				_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
			));
		}

		void _setup_mouse_tracking() {
			TRACKMOUSEEVENT tme;
			std::memset(&tme, 0, sizeof(tme));
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_HOVER | TME_LEAVE;
			tme.dwHoverTime = HOVER_DEFAULT;
			tme.hwndTrack = _hwnd;
			winapi_check(TrackMouseEvent(&tme));
		}

		void _on_mouse_enter() override {
			_setup_mouse_tracking();
			window_base::_on_mouse_enter();
		}

		void _initialize(str_view_t cls, const ui::element_configuration &config) override {
			window_base::_initialize(cls, config);
			SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

			/*DWM_BLURBEHIND bb;
			std::memset(&bb, 0, sizeof(bb));
			bb.dwFlags = DWM_BB_ENABLE;
			bb.fEnable = true;
			bb.hRgnBlur = NULL;
			DwmEnableBlurBehindWindow(_hwnd, &bb);*/
		}
		void _dispose() override {
			winapi_check(DestroyWindow(_hwnd));
			window_base::_dispose();
		}
	};
}
