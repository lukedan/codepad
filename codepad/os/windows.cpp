#define OEMRESOURCE
#include <Windows.h>

#include "windows.h"

namespace codepad {
	namespace os {
		namespace input {
			const int _key_id_mapping[static_cast<int>(total_num_keys)] = {
				VK_LBUTTON, VK_RBUTTON, VK_MBUTTON,
				VK_CANCEL,
				VK_XBUTTON1, VK_XBUTTON2,
				VK_BACK,
				VK_TAB,
				VK_CLEAR,
				VK_RETURN,
				VK_SHIFT, VK_CONTROL, VK_MENU,
				VK_PAUSE,
				VK_CAPITAL,
				VK_ESCAPE,
				VK_CONVERT,
				VK_NONCONVERT,
				VK_SPACE,
				VK_PRIOR,
				VK_NEXT,
				VK_END,
				VK_HOME,
				VK_LEFT, VK_UP, VK_RIGHT, VK_DOWN,
				VK_SELECT,
				VK_PRINT,
				VK_EXECUTE,
				VK_SNAPSHOT,
				VK_INSERT,
				VK_DELETE,
				VK_HELP,
				VK_LWIN,
				VK_RWIN,
				VK_APPS,
				VK_SLEEP,
				VK_MULTIPLY,
				VK_ADD,
				VK_SEPARATOR,
				VK_SUBTRACT,
				VK_DECIMAL,
				VK_DIVIDE,
				VK_F1, VK_F2, VK_F3, VK_F4,
				VK_F5, VK_F6, VK_F7, VK_F8,
				VK_F9, VK_F10, VK_F11, VK_F12,
				VK_NUMLOCK,
				VK_SCROLL,
				VK_LSHIFT, VK_RSHIFT,
				VK_LCONTROL, VK_RCONTROL,
				VK_LMENU, VK_RMENU,
				'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
				'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
			};
			struct _key_id_backmapping_t {
				_key_id_backmapping_t() {
					for (int i = 0; i < static_cast<int>(total_num_keys); ++i) {
						v[_key_id_mapping[i]] = i;
					}
				}
				int v[255];
			} _key_id_backmapping;
		}

		const int _cursor_id_mapping[] = {
			OCR_NORMAL,
			OCR_WAIT,
			OCR_CROSS,
			OCR_HAND,
			OCR_NORMAL, // TODO OCR_HELP not in header?
			OCR_IBEAM,
			OCR_NO,
			OCR_SIZEALL,
			OCR_SIZENESW,
			OCR_SIZENS,
			OCR_SIZENWSE,
			OCR_SIZEWE
		};

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
					form->_on_close_request();
					return 0;

				case WM_SIZE:
					form->_on_resize();
					return 0;

				case WM_KEYDOWN:
					_form_onevent<ui::key_info>(
						*form, &window::_on_key_down, static_cast<input::key>(input::_key_id_backmapping.v[wparam])
						);
					return 0;
				case WM_KEYUP:
					_form_onevent<ui::key_info>(
						*form, &window::_on_key_up, static_cast<input::key>(input::_key_id_backmapping.v[wparam])
						);
					return 0;

				case WM_UNICHAR:
					if (wparam != VK_BACK && wparam != VK_ESCAPE) {
						_form_onevent<ui::text_info>(
							*form, &window::_on_keyboard_text,
							wparam == VK_RETURN ? U'\n' : static_cast<char_t>(wparam)
							);
					}
					return (wparam == UNICODE_NOCHAR ? TRUE : FALSE);
				case WM_CHAR:
					if (wparam != VK_BACK && wparam != VK_ESCAPE) {
						_form_onevent<ui::text_info>(
							*form, &window::_on_keyboard_text,
							wparam == VK_RETURN ? U'\n' : static_cast<char_t>(wparam)
							);
					}
					return 0;

				case WM_MOUSEWHEEL:
					{
						POINT p;
						p.x = GET_X_LPARAM(lparam);
						p.y = GET_Y_LPARAM(lparam);
						winapi_check(ScreenToClient(form->_hwnd, &p));
						_form_onevent<ui::mouse_scroll_info>(
							*form, &window::_on_mouse_scroll,
							GET_WHEEL_DELTA_WPARAM(wparam) / static_cast<double>(WHEEL_DELTA), vec2d(p.x, p.y)
							);
						return 0;
					}

				case WM_MOUSEMOVE:
					if (!form->_mouse_over) {
						form->_setup_mouse_tracking();
						form->_on_mouse_enter();
					}
					_form_onevent<ui::mouse_move_info>(*form, &window::_on_mouse_move, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
					return 0;
				case WM_MOUSELEAVE:
					form->_on_mouse_leave();
					return 0;

				case WM_LBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_down,
						input::mouse_button::left, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;
				case WM_LBUTTONUP:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_up,
						input::mouse_button::left, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;
				case WM_RBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_down,
						input::mouse_button::right, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;
				case WM_RBUTTONUP:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_up,
						input::mouse_button::right, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;
				case WM_MBUTTONDOWN:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_down,
						input::mouse_button::middle, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;
				case WM_MBUTTONUP:
					_form_onevent<ui::mouse_button_info>(
						*form, &window::_on_mouse_up,
						input::mouse_button::middle, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
						);
					return 0;

				case WM_SETFOCUS:
					form->_on_got_window_focus();
					return 0;
				case WM_KILLFOCUS:
					form->_on_lost_window_focus();
					return 0;

				case WM_SETCURSOR:
					{
						if (!form->is_mouse_over()) {
							return DefWindowProc(hwnd, msg, wparam, lparam);
						}
						ui::cursor c = form->get_current_display_cursor();
						if (c == ui::cursor::not_specified) {
							return DefWindowProc(hwnd, msg, wparam, lparam);
						} else if (c == ui::cursor::invisible) {
							SetCursor(nullptr);
						} else {
							HANDLE img = LoadImage(
								nullptr, MAKEINTRESOURCE(_cursor_id_mapping[static_cast<int>(c)]),
								IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE
							);
							winapi_check(img);
							SetCursor(static_cast<HCURSOR>(img));
						}
						return TRUE;
					}
				}
			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		window::_wndclass window::_class;
		window::_wndclass::_wndclass() {
			WNDCLASSEX wcex;
			std::memset(&wcex, 0, sizeof(wcex));
			wcex.style = CS_OWNDC;
			wcex.hInstance = GetModuleHandle(nullptr);
			winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
			wcex.cbSize = sizeof(wcex);
			wcex.lpfnWndProc = _wndproc;
			wcex.lpszClassName = L"Codepad";
			winapi_check(atom = RegisterClassEx(&wcex));
		}

		bool window::_idle() {
			MSG msg;
			if (PeekMessage(&msg, _hwnd, 0, 0, PM_REMOVE)) {
				if (!(
					msg.message == WM_KEYDOWN &&
					hotkey_manager.on_key_down(static_cast<input::key>(input::_key_id_backmapping.v[msg.wParam]))
					)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				return true;
			}
			return false;
		}
	}
}