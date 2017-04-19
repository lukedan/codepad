#include "windows.h"

namespace codepad {
	namespace platform {
		template <typename Inf, typename ...Args> inline void _form_onevent(
			window &w, void (window::*handle)(Inf&), Args &&...args
		) {
			Inf inf(std::forward<Args>(args)...);
			(w.*handle)(inf);
		}

		LRESULT CALLBACK _wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
			window *form = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (form) {
				switch (msg) {
				case WM_CLOSE:
					_form_onevent<void_info>(*form, &window::_on_close_request);
					return 0;

				case WM_SIZE:
				{
					form->_recalc_layout();
					vec2i newsz(LOWORD(lparam), HIWORD(lparam));
					if (newsz.x > 0 && newsz.y > 0) {
						_form_onevent<size_changed_info>(*form, &window::_on_size_changed, newsz);
					}
					return 0;
				}

				case WM_KEYDOWN:
					_form_onevent<ui::key_info>(*form, &window::_on_key_up, static_cast<int>(wparam));
					return 0;
				case WM_KEYUP:
					_form_onevent<ui::key_info>(*form, &window::_on_key_down, static_cast<int>(wparam));
					return 0;

				case WM_UNICHAR:
					_form_onevent<ui::text_info>(*form, &window::_on_keyboard_text, static_cast<wchar_t>(wparam));
					return (wparam == UNICODE_NOCHAR ? TRUE : FALSE);

				case WM_MOUSEWHEEL:
					_form_onevent<ui::mouse_scroll_info>(
						*form, &window::_on_mouse_scroll, GET_WHEEL_DELTA_WPARAM(wparam) / static_cast<double>(WHEEL_DELTA)
					);
					return 0;

				case WM_MOUSEMOVE:
					if (!form->_mouse_over) {
						_form_onevent<void_info>(*form, &window::_on_mouse_enter);
					}
					_form_onevent<ui::mouse_move_info>(*form, &window::_on_mouse_move, vec2i(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
					return 0;
				case WM_MOUSELEAVE:
					_form_onevent<void_info>(*form, &window::_on_mouse_leave);
					return 0;

				case WM_LBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_down, ui::mouse_button::left);
					return 0;
				case WM_LBUTTONUP:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_up, ui::mouse_button::left);
					return 0;
				case WM_RBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_down, ui::mouse_button::right);
					return 0;
				case WM_RBUTTONUP:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_up, ui::mouse_button::right);
					return 0;
				case WM_MBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_down, ui::mouse_button::middle);
					return 0;
				case WM_MBUTTONUP:
					_form_onevent<ui::mouse_button_info>(*form, &window::_on_mouse_up, ui::mouse_button::middle);
					return 0;

				case WM_SETFOCUS:
					_form_onevent<void_info>(*form, &window::_on_got_focus);
					return 0;
				case WM_KILLFOCUS:
					_form_onevent<void_info>(*form, &window::_on_lost_focus);
					return 0;
				}
			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		window::window(const str_t &clsname) {
			HINSTANCE hinst = GetModuleHandle(nullptr);
			winapi_check(hinst);
			WNDCLASSEX wcex;
			std::memset(&wcex, 0, sizeof(wcex));
			wcex.style = CS_OWNDC;
			wcex.hInstance = hinst;
			winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
			wcex.cbSize = sizeof(wcex);
			wcex.lpfnWndProc = _wndproc;
			wcex.lpszClassName = clsname.c_str();
			winapi_check(_wndatom = RegisterClassEx(&wcex));
			winapi_check(_hwnd = CreateWindowEx(
				0, clsname.c_str(), clsname.c_str(), WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				nullptr, nullptr, hinst, nullptr
			));
			winapi_check(_dc = GetDC(_hwnd));
			SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
			ShowWindow(_hwnd, SW_SHOW);
		}
	}
}