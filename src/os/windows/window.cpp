// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "window.h"

/// \file
/// Implementation of windows.

namespace codepad::os {
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

	window::window(const std::u8string &clsname, window *parent) {
		auto u16str = _details::utf8_to_wstring(clsname.c_str());
		_wndclass::get();
		_details::winapi_check(_hwnd = CreateWindowEx(
			WS_EX_ACCEPTFILES, reinterpret_cast<LPCTSTR>(static_cast<std::size_t>(_wndclass::get().atom)),
			reinterpret_cast<LPCTSTR>(u16str.c_str()), WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			parent ? parent->_hwnd : nullptr, nullptr, GetModuleHandle(nullptr), nullptr
		));
	}

	void window::set_caption(const std::u8string &cap) {
		auto u16str = _details::utf8_to_wstring(cap.c_str());
		_details::winapi_check(SetWindowText(_hwnd, reinterpret_cast<LPCWSTR>(u16str.c_str())));
	}

	vec2d window::get_position() const {
		POINT tl;
		tl.x = tl.y = 0;
		_details::winapi_check(ClientToScreen(_hwnd, &tl));
		return vec2d(static_cast<double>(tl.x), static_cast<double>(tl.y));
	}

	void window::set_position(vec2d pos) {
		RECT r;
		POINT tl;
		tl.x = tl.y = 0;
		_details::winapi_check(GetWindowRect(_hwnd, &r));
		_details::winapi_check(ClientToScreen(_hwnd, &tl));
		tl.x -= r.left;
		tl.y -= r.top;
		_details::winapi_check(SetWindowPos(
			_hwnd, nullptr, static_cast<int>(pos.x) - tl.x, static_cast<int>(pos.y) - tl.y, 0, 0, SWP_NOSIZE
		));
	}

	vec2d window::get_client_size() const {
		RECT r;
		_details::winapi_check(GetClientRect(_hwnd, &r));
		return _physical_to_logical_position(vec2d(
			static_cast<double>(r.right), static_cast<double>(r.bottom)
		));
	}

	void window::set_client_size(vec2d sz) {
		sz = _logical_to_physical_position(sz);
		RECT wndrgn, cln;
		_details::winapi_check(GetWindowRect(_hwnd, &wndrgn));
		_details::winapi_check(GetClientRect(_hwnd, &cln));
		_details::winapi_check(SetWindowPos(
			_hwnd, nullptr, 0, 0,
			wndrgn.right - wndrgn.left - cln.right + static_cast<int>(std::round(sz.x)),
			wndrgn.bottom - wndrgn.top - cln.bottom + static_cast<int>(std::round(sz.y)),
			SWP_NOMOVE
		));
	}

	void window::prompt_ready() {
		FLASHWINFO fwi;
		std::memset(&fwi, 0, sizeof(fwi));
		fwi.cbSize = sizeof(fwi);
		fwi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
		fwi.dwTimeout = 0;
		fwi.hwnd = _hwnd;
		fwi.uCount = 0;
		FlashWindowEx(&fwi);
	}

	bool window::hit_test_full_client(vec2d v) const {
		RECT r;
		_details::winapi_check(GetWindowRect(_hwnd, &r));
		return r.left <= v.x && r.right > v.x && r.top <= v.y && r.bottom > v.y;
	}

	vec2d window::screen_to_client(vec2d v) const {
		POINT p;
		p.x = static_cast<int>(std::round(v.x));
		p.y = static_cast<int>(std::round(v.y));
		_details::winapi_check(ScreenToClient(_hwnd, &p));
		return _physical_to_logical_position(vec2d(
			static_cast<double>(p.x), static_cast<double>(p.y)
		));
	}

	vec2d window::client_to_screen(vec2d v) const {
		v = _logical_to_physical_position(v);
		POINT p;
		p.x = static_cast<int>(std::round(v.x));
		p.y = static_cast<int>(std::round(v.y));
		_details::winapi_check(ClientToScreen(_hwnd, &p));
		return vec2d(static_cast<double>(p.x), static_cast<double>(p.y));
	}

	/// Forwards the event to the window.
	template <typename Inf, typename ...Args> void _form_onevent(
		window &w, void (window::*handle)(Inf&), Args && ...args
	) {
		Inf inf(std::forward<Args>(args)...);
		(w.*handle)(inf);
	}
	LRESULT CALLBACK window::_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto *form = window::_get_associated_window(hwnd);
		if (form) {
			switch (msg) {
			case WM_CLOSE:
				// WM_CLOSE is not intercepted by GetMessage or PeekMessage
				// so we need to wake the main thread up first
				form->get_manager().get_scheduler().wake_up();
				form->_on_close_request();
				return 0;

			case WM_SIZE:
				{
					if (wparam != SIZE_MINIMIZED) {
						vec2d size(LOWORD(lparam), HIWORD(lparam));
						size = form->_physical_to_logical_position(size);
						size_changed_info p(size);
						if (p.new_value.x > 0 && p.new_value.y > 0) {
							form->_layout = rectd::from_corners(vec2d(), p.new_value);
							form->_on_size_changed(p);
							form->get_manager().get_scheduler().update_layout_and_visuals();
						}
					}
					return 0;
				}
			case WM_DPICHANGED:
				{
					double dpix = LOWORD(wparam), dpiy = HIWORD(wparam);
					scaling_factor_changed_info p(vec2d(
						dpix / USER_DEFAULT_SCREEN_DPI, dpiy / USER_DEFAULT_SCREEN_DPI
					));
					form->_on_scaling_factor_changed(p);
					RECT* const advised_window_layout = reinterpret_cast<RECT*>(lparam);
					SetWindowPos(
						form->get_native_handle(),
						nullptr,
						advised_window_layout->left,
						advised_window_layout->top,
						advised_window_layout->right - advised_window_layout->left,
						advised_window_layout->bottom - advised_window_layout->top,
						SWP_NOZORDER | SWP_NOACTIVATE
					);
					return 0;
				}

			case WM_SYSKEYDOWN:
				[[fallthrough]]; // same processing
			case WM_KEYDOWN:
				_form_onevent<ui::key_info>(
					*form, &window::_on_key_down, _details::key_id_mapping_t::backward.value[wparam]
					);
				break;
			case WM_SYSKEYUP:
				[[fallthrough]];
			case WM_KEYUP:
				_form_onevent<ui::key_info>(
					*form, &window::_on_key_up, _details::key_id_mapping_t::backward.value[wparam]
					);
				break;

			case WM_UNICHAR:
				if (wparam == UNICODE_NOCHAR) {
					return TRUE;
				}
				if (wparam != VK_BACK && wparam != VK_ESCAPE) {
					std::u8string content;
					if (wparam == VK_RETURN) {
						content = u8"\n";
					} else {
						content = reinterpret_cast<const char8_t*>(
							encodings::utf8::encode_codepoint(static_cast<codepoint>(wparam)).c_str()
							);
					}
					_form_onevent<ui::text_info>(*form, &window::_on_keyboard_text, content);
				}
				return FALSE;
			case WM_CHAR:
				if (wparam != VK_BACK && wparam != VK_ESCAPE) {
					std::u8string content;
					if (wparam == VK_RETURN) {
						content = u8"\n";
					} else {
						auto *ptr = reinterpret_cast<const std::byte*>(&wparam);
						codepoint res;
						if (!encodings::utf16<>::next_codepoint(ptr, ptr + 4, res)) {
							logger::get().log_warning(CP_HERE) <<
								"invalid UTF-16 codepoint, possible faulty windows message handling";
							// TODO check if this will ever be triggered
							return 0;
						}
						content = reinterpret_cast<const char8_t*>(encodings::utf8::encode_codepoint(res).c_str());
					}
					_form_onevent<ui::text_info>(*form, &window::_on_keyboard_text, content);
				}
				return 0;

			case WM_MOUSEWHEEL:
				{
					POINT p;
					p.x = GET_X_LPARAM(lparam);
					p.y = GET_Y_LPARAM(lparam);
					_details::winapi_check(ScreenToClient(form->_hwnd, &p));
					_form_onevent<ui::mouse_scroll_info>(
						*form, &window::_on_mouse_scroll,
						vec2d(0.0, GET_WHEEL_DELTA_WPARAM(wparam) / static_cast<double>(WHEEL_DELTA)),
						form->_update_mouse_position(form->_physical_to_logical_position(vec2d(p.x, p.y)))
						);
					return 0;
				}
			case WM_MOUSEHWHEEL:
				{
					POINT p;
					p.x = GET_X_LPARAM(lparam);
					p.y = GET_Y_LPARAM(lparam);
					_details::winapi_check(ScreenToClient(form->_hwnd, &p));
					_form_onevent<ui::mouse_scroll_info>(
						*form, &window::_on_mouse_scroll,
						vec2d(GET_WHEEL_DELTA_WPARAM(wparam) / static_cast<double>(WHEEL_DELTA), 0.0),
						form->_update_mouse_position(form->_physical_to_logical_position(vec2d(p.x, p.y)))
						);
					return 0;
				}

			case WM_MOUSEMOVE:
				if (!form->is_mouse_over()) {
					form->_on_mouse_enter();
				}
				_form_onevent<ui::mouse_move_info>(
					*form, &window::_on_mouse_move,
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					))
					);
				return 0;
			case WM_MOUSELEAVE:
				form->_on_mouse_leave();
				return 0;

			case WM_LBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					ui::mouse_button::primary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_LBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					ui::mouse_button::primary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_RBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					ui::mouse_button::secondary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_RBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					ui::mouse_button::secondary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_MBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					ui::mouse_button::tertiary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_MBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					ui::mouse_button::tertiary, _details::get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;

			case WM_SETFOCUS:
				form->get_manager().get_scheduler().set_focused_element(form);
				return 0;
			case WM_KILLFOCUS:
				form->get_manager().get_scheduler().set_focused_element(nullptr);
				return 0;

			case WM_CANCELMODE:
				form->_on_lost_window_capture();
				return 0;

			case WM_SETCURSOR:
				{
					if (!form->is_mouse_over()) {
						return DefWindowProc(hwnd, msg, wparam, lparam);
					}
					ui::cursor c = form->get_current_display_cursor();
					if (c == ui::cursor::not_specified) {
						return DefWindowProc(hwnd, msg, wparam, lparam);
					}
					if (c == ui::cursor::invisible) {
						SetCursor(nullptr);
					} else {
						HANDLE img = LoadImage(
							nullptr, MAKEINTRESOURCE(_cursor_id_mapping[static_cast<int>(c)]),
							IMAGE_CURSOR, 0, 0, LR_SHARED | LR_DEFAULTSIZE
						);
						_details::winapi_check(img);
						SetCursor(static_cast<HCURSOR>(img));
					}
					return TRUE;
				}

			// ime-related messages
			case WM_IME_SETCONTEXT:
				window::_ime::get().complete_composition(*form);
				lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
				return DefWindowProc(hwnd, msg, wparam, lparam);
			case WM_IME_STARTCOMPOSITION:
				window::_ime::get().start_composition(*form);
				return 0;
			case WM_IME_COMPOSITION:
				{
					window::_ime::get().update_composition(*form);
					if (auto str = window::_ime::get().get_composition_string(*form, lparam)) {
						if (!str->empty()) {
							_form_onevent<ui::composition_info>(
								*form, &window::_on_composition,
								_details::wstring_to_utf8(str->c_str())
								);
						}
					}
					if (auto str = window::_ime::get().get_result(*form, lparam)) {
						if (!str->empty()) {
							_form_onevent<ui::text_info>(
								*form, &window::_on_keyboard_text,
								_details::wstring_to_utf8(str->c_str())
								);
						}
					}
					return 0;
				}
			case WM_IME_ENDCOMPOSITION:
				form->_on_composition_finished();
				window::_ime::get().complete_composition(*form);
				break;
			case WM_INPUTLANGCHANGE:
				window::_ime::get().on_input_language_changed();
				break;
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	window *window::_get_associated_window(HWND hwnd) {
		if (hwnd) {
			DWORD atom;
			_details::winapi_check(atom = GetClassLong(hwnd, GCW_ATOM));
			if (atom == _wndclass::get().atom) { // the correct class
				return reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			}
		}
		return nullptr;
	}

	void window::_set_window_style_bit(bool v, LONG bit, int type) {
		LONG old = GetWindowLong(_hwnd, type);
		SetWindowLong(_hwnd, type, v ? old | bit : old & ~bit);
		_details::winapi_check(SetWindowPos(
			_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
		));
	}

	void window::_setup_mouse_tracking() {
		TRACKMOUSEEVENT tme;
		std::memset(&tme, 0, sizeof(tme));
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_HOVER | TME_LEAVE;
		tme.dwHoverTime = HOVER_DEFAULT;
		tme.hwndTrack = _hwnd;
		_details::winapi_check(TrackMouseEvent(&tme));
	}

	void window::_initialize(std::u8string_view cls, const ui::element_configuration &config) {
		SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		window_base::_initialize(cls, config);

		// update cached dpi, because the WM_DPICHANGED message is not sent after creating the window
		// FIXME this function only returns one value which we now use for both x and y
		UINT dpi = GetDpiForWindow(_hwnd);
		double scaling = dpi / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
		_cached_scaling = vec2d(scaling, scaling);
	}

	void window::_dispose() {
		_details::winapi_check(DestroyWindow(_hwnd));
		window_base::_dispose();
	}


	std::optional<std::wstring> window::_ime::_get_string(window &wnd, DWORD type) {
		HIMC context = ImmGetContext(wnd._hwnd);
		if (context) {
			LONG buffersz = ImmGetCompositionString(context, type, nullptr, 0);
			assert_true_sys(buffersz != IMM_ERROR_GENERAL, "general IME error");
			if (buffersz != IMM_ERROR_NODATA) {
				std::wstring result(buffersz / sizeof(wchar_t), 0);
				assert_true_sys(
					ImmGetCompositionString(context, type, result.data(), buffersz) == buffersz,
					"failed to obtain string from IME"
				);
				return result;
			}
		}
		return std::nullopt;
	}

	void window::_ime::_update_caret_position(window &wnd) {
		recti scaled_caret = rectd::from_corners(
			wnd._logical_to_physical_position(_caretrgn.xmin_ymin()),
			wnd._logical_to_physical_position(_caretrgn.xmax_ymax())
		).fit_grid_enlarge<int>();

		HIMC context = ImmGetContext(wnd._hwnd);
		if (context) {
			CANDIDATEFORM rgn{};
			rgn.dwIndex = 0;
			rgn.dwStyle = CFS_CANDIDATEPOS;
			rgn.ptCurrentPos.x = scaled_caret.xmin;
			rgn.ptCurrentPos.y = scaled_caret.ymax;
			_details::winapi_check(ImmSetCandidateWindow(context, &rgn));

			rgn.dwStyle = CFS_EXCLUDE;
			rgn.ptCurrentPos.x = scaled_caret.xmin;
			rgn.ptCurrentPos.y = scaled_caret.ymin;
			rgn.rcArea.left = scaled_caret.xmin;
			rgn.rcArea.right = scaled_caret.xmax;
			rgn.rcArea.top = scaled_caret.ymin;
			rgn.rcArea.bottom = scaled_caret.ymax;
			_details::winapi_check(ImmSetCandidateWindow(context, &rgn));

			_details::winapi_check(ImmReleaseContext(wnd._hwnd, context));
		}

		if (_compositing) {
			_details::winapi_check(
				CreateCaret(wnd._hwnd, nullptr, scaled_caret.width(), scaled_caret.height())
			);
			_details::winapi_check(SetCaretPos(scaled_caret.xmin, scaled_caret.ymin));
		}
	}

	void window::_ime::_end_composition(window &wnd, DWORD signal) {
		if (_compositing) {
			DestroyCaret();
			HIMC context = ImmGetContext(wnd._hwnd);
			if (context) {
				_details::winapi_check(ImmNotifyIME(context, NI_COMPOSITIONSTR, signal, 0));
				_details::winapi_check(ImmReleaseContext(wnd._hwnd, context));
			}
			_compositing = false;
		}
	}


	window::_wndclass::_wndclass() {
		WNDCLASSEX wcex;
		memset(&wcex, 0, sizeof(wcex));
		wcex.style = CS_OWNDC;
		wcex.hInstance = GetModuleHandle(nullptr);
		_details::winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
		wcex.cbSize = sizeof(wcex);
		wcex.lpfnWndProc = _wndproc;
		wcex.lpszClassName = L"Codepad";
		_details::winapi_check(atom = RegisterClassEx(&wcex));
	}
}
