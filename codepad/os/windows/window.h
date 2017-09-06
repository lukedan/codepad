#pragma once

#include <cstring>

#include "../window.h"
#include "misc.h"

namespace codepad {
	namespace ui {
		class element;
	}
	namespace os {
		class software_renderer;
		class opengl_renderer;

		class window : public window_base {
			friend LRESULT CALLBACK _wndproc(HWND, UINT, WPARAM, LPARAM);
			friend class software_renderer;
			friend class opengl_renderer;
			friend class ui::element;
		public:
			void set_caption(const str_t &cap) override {
				auto u16str = convert_to_utf16(cap);
				winapi_check(SetWindowText(_hwnd, reinterpret_cast<LPCWSTR>(u16str.c_str())));
			}
			vec2i get_position() const override {
				POINT tl;
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

			void set_display_maximize_button(bool disp) override {
				_set_window_style_bit(disp, WS_MAXIMIZE);
			}
			void set_display_minimize_button(bool disp) override {
				_set_window_style_bit(disp, WS_MINIMIZE);
			}
			void set_display_caption_bar(bool disp) override {
				_set_window_style_bit(disp, WS_CAPTION ^ WS_BORDER);
			}
			void set_display_border(bool disp) override {
				_set_window_style_bit(disp, WS_BORDER);
			}
			void set_sizable(bool size) override {
				_set_window_style_bit(size, WS_THICKFRAME);
			}

			bool hit_test_full_client(vec2i v) const override {
				RECT r;
				winapi_check(GetWindowRect(_hwnd, &r));
				return r.left <= v.x && r.right > v.x && r.top <= v.y && r.bottom > v.y;
			}

			ui::cursor get_current_display_cursor() const override {
				if (_capture) {
					return _capture->get_current_display_cursor();
				}
				return window_base::get_current_display_cursor();
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
			void release_mouse_capture() {
				window_base::release_mouse_capture();
				winapi_check(ReleaseCapture());
			}

			inline static str_t get_default_class() {
				return U"window";
			}
		protected:
			window() : window(U"Codepad") {
			}
			explicit window(const str_t &clsname) {
				auto u16str = convert_to_utf16(clsname);
				winapi_check(_hwnd = CreateWindowEx(
					0, reinterpret_cast<LPCWSTR>(static_cast<size_t>(_class.atom)),
					reinterpret_cast<LPCWSTR>(u16str.c_str()), WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
					nullptr, nullptr, GetModuleHandle(nullptr), nullptr
				));
				winapi_check(_dc = GetDC(_hwnd));
			}

			HWND _hwnd;
			HDC _dc;

			struct _wndclass {
				_wndclass();
				~_wndclass() {
					UnregisterClass(reinterpret_cast<LPCTSTR>(atom), GetModuleHandle(nullptr));
				}

				ATOM atom;
			};
			static _wndclass _class;

			void _set_window_style_bit(bool v, LONG bit) {
				LONG old = GetWindowLong(_hwnd, GWL_STYLE);
				SetWindowLong(_hwnd, GWL_STYLE, v ? old | bit : old & ~bit);
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

			void _on_resize() {
				RECT cln;
				winapi_check(GetClientRect(_hwnd, &cln));
				_layout = rectd::from_xywh(0.0, 0.0, cln.right, cln.bottom);
				size_changed_info p(vec2i(cln.right, cln.bottom));
				if (p.new_size.x > 0 && p.new_size.y > 0) {
					_on_size_changed(p);
					ui::manager::get().update_layout_and_visual();
				}
			}

			bool _idle();
			void _on_update() override {
				while (_idle()) {
				}
				_update_drag();
				ui::manager::get().schedule_update(*this);
			}
			void _on_prerender() override {
				ShowWindow(_hwnd, SW_SHOW);
				window_base::_on_prerender();
			}

			void _initialize() override {
				window_base::_initialize();
				SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
				ui::manager::get().schedule_update(*this);
			}
			void _dispose() override {
				winapi_check(DestroyWindow(_hwnd));
				window_base::_dispose();
			}
		};
	}
}
