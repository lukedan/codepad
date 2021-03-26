// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/windows/window.h"

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


	/// Forwards the event to the window.
	template <typename Inf, typename ...Args> void _wnd_onevent(
		ui::window &w, void (ui::window::*handle)(Inf&), Args && ...args
	) {
		Inf inf(std::forward<Args>(args)...);
		(w.*handle)(inf);
	}
	LRESULT CALLBACK window_impl::_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto *wnd_impl = window_impl::_get_associated_window_impl(hwnd);
		if (wnd_impl) {
			auto &wnd = wnd_impl->_window;
			switch (msg) {
			case WM_CLOSE:
				wnd._on_close_request();
				// WM_CLOSE is not intercepted by GetMessage or PeekMessage, so we need to
				// wake the main thread up to run the message loop and update everything
				wnd.get_manager().get_scheduler().wake_up();
				return 0;
			case WM_DESTROY:
				// a child window is being destroyed; normally we won't receive this as GWLP_USREDATA is reset before
				// calling DestroyWindow() in ~window_impl()
				SetWindowLongPtr(wnd_impl->_hwnd, GWLP_USERDATA, NULL);
				wnd_impl->_hwnd = nullptr;
				wnd.get_manager().get_scheduler().mark_for_disposal(wnd);
				return 0;

			case WM_SIZING:
				{
					// prevent resizing the window if the size is controlled by the application
					bool
						manage_width = wnd.get_width_size_policy() == ui::window::size_policy::application,
						manage_height = wnd.get_height_size_policy() == ui::window::size_policy::application;
					if (!manage_width && !manage_height) {
						break; // no need to handle this message
					}
					RECT window_rect;
					_details::winapi_check(GetWindowRect(wnd_impl->get_native_handle(), &window_rect));
					RECT *rect = reinterpret_cast<RECT*>(lparam);
					// adjust the rectangle to maintain the original size
					if (manage_width) {
						switch (wparam) {
						case WMSZ_TOPLEFT:
							[[fallthrough]];
						case WMSZ_BOTTOMLEFT:
							[[fallthrough]];
						case WMSZ_LEFT:
							rect->left = rect->right - (window_rect.right - window_rect.left);
							break;
						case WMSZ_TOPRIGHT:
							[[fallthrough]];
						case WMSZ_BOTTOMRIGHT:
							[[fallthrough]];
						case WMSZ_RIGHT:
							rect->right = rect->left + (window_rect.right - window_rect.left);
							break;
						}
					}
					if (manage_height) {
						switch (wparam) {
						case WMSZ_TOPLEFT:
							[[fallthrough]];
						case WMSZ_TOPRIGHT:
							[[fallthrough]];
						case WMSZ_TOP:
							rect->top = rect->bottom - (window_rect.bottom - window_rect.top);
							break;
						case WMSZ_BOTTOMLEFT:
							[[fallthrough]];
						case WMSZ_BOTTOMRIGHT:
							[[fallthrough]];
						case WMSZ_BOTTOM:
							rect->bottom = rect->top + (window_rect.bottom - window_rect.top);
							break;
						}
					}
					return TRUE;
				}
			case WM_SIZE:
				{
					if (wparam != SIZE_MINIMIZED) {
						vec2d size(LOWORD(lparam), HIWORD(lparam));
						size = wnd_impl->_physical_to_logical_position(size);
						ui::window::size_changed_info p(size);
						if (p.new_value.x > 0 && p.new_value.y > 0) {
							wnd._layout = rectd::from_corners(vec2d(), p.new_value);
							wnd._on_size_changed(p);
							wnd.get_manager().get_scheduler().update_layout_and_visuals();
						}
					}
					return 0;
				}
			case WM_DPICHANGED:
				{
					double dpix = LOWORD(wparam), dpiy = HIWORD(wparam);
					ui::window::scaling_factor_changed_info p(vec2d(
						dpix / USER_DEFAULT_SCREEN_DPI, dpiy / USER_DEFAULT_SCREEN_DPI
					));
					wnd_impl->_cached_scaling = p.new_value;
					wnd._on_scaling_factor_changed(p);
					RECT* const advised_window_layout = reinterpret_cast<RECT*>(lparam);
					SetWindowPos(
						wnd_impl->get_native_handle(),
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
				_wnd_onevent<ui::key_info>(
					wnd, &ui::window::_on_key_down, _details::key_id_mapping_t::backward.value[wparam]
					);
				break;
			case WM_SYSKEYUP:
				[[fallthrough]];
			case WM_KEYUP:
				_wnd_onevent<ui::key_info>(
					wnd, &ui::window::_on_key_up, _details::key_id_mapping_t::backward.value[wparam]
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
					_wnd_onevent<ui::text_info>(wnd, &ui::window::_on_keyboard_text, content);
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
					_wnd_onevent<ui::text_info>(wnd, &ui::window::_on_keyboard_text, content);
				}
				return 0;

			case WM_MOUSEWHEEL:
				{
					POINT p;
					p.x = GET_X_LPARAM(lparam);
					p.y = GET_Y_LPARAM(lparam);
					_details::winapi_check(ScreenToClient(wnd_impl->get_native_handle(), &p));
					int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
					_wnd_onevent<ui::mouse_scroll_info>(
						wnd, &ui::window::_on_mouse_scroll,
						// here the sign is inverted so that we add the value
						// to obtain the new position instead of subtracting
						vec2d(0.0, -wheel_delta / static_cast<double>(WHEEL_DELTA)),
						wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(vec2d(p.x, p.y))),
						// FIXME here and in WM_MOUSEHWHEEL this is used to determine if the scroll event is smooth
						//       scrolling, which may cause false positives, but we aren't aware of a better way
						wheel_delta % WHEEL_DELTA != 0
					);
					return 0;
				}
			case WM_MOUSEHWHEEL:
				{
					POINT p;
					p.x = GET_X_LPARAM(lparam);
					p.y = GET_Y_LPARAM(lparam);
					_details::winapi_check(ScreenToClient(wnd_impl->get_native_handle(), &p));
					int wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);
					_wnd_onevent<ui::mouse_scroll_info>(
						wnd, &ui::window::_on_mouse_scroll,
						vec2d(wheel_delta / -static_cast<double>(WHEEL_DELTA), 0.0),
						wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(vec2d(p.x, p.y))),
						wheel_delta % WHEEL_DELTA != 0
					);
					return 0;
				}

			case WM_MOUSEMOVE:
				// this could be either the first time (on mouse enter), or when the mouse is moved after a hover
				// event is received
				wnd_impl->_setup_mouse_tracking();
				if (!wnd.is_mouse_over()) {
					wnd._on_mouse_enter();
				}
				_wnd_onevent<ui::mouse_move_info>(
					wnd, &ui::window::_on_mouse_move,
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					))
				);
				return 0;
			case WM_MOUSEHOVER:
				wnd_impl->_mouse_tracked = false;
				_wnd_onevent<ui::mouse_hover_info>(
					wnd, &ui::window::_on_mouse_hover,
					_details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					))
				);
				return 0;
			case WM_MOUSELEAVE:
				wnd_impl->_mouse_tracked = false;
				wnd._on_mouse_leave();
				return 0;

			case WM_LBUTTONDOWN:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_down,
					ui::mouse_button::primary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;
			case WM_LBUTTONUP:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_up,
					ui::mouse_button::primary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;
			case WM_RBUTTONDOWN:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_down,
					ui::mouse_button::secondary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;
			case WM_RBUTTONUP:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_up,
					ui::mouse_button::secondary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;
			case WM_MBUTTONDOWN:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_down,
					ui::mouse_button::tertiary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;
			case WM_MBUTTONUP:
				_wnd_onevent<ui::mouse_button_info>(
					wnd, &ui::window::_on_mouse_up,
					ui::mouse_button::tertiary, _details::get_modifiers(),
					wnd._update_mouse_position(wnd_impl->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					)
				);
				return 0;

			case WM_MOUSEACTIVATE:
				// apparently WS_EX_NOACTIVATE by itself is not enough
				if (!wnd_impl->_focusable) {
					return MA_NOACTIVATE;
				}
				return DefWindowProc(hwnd, msg, wparam, lparam);
			case WM_SETFOCUS:
				wnd._on_got_system_focus();
				return 0;
			case WM_KILLFOCUS:
				wnd._on_lost_system_focus();
				return 0;

			case WM_CANCELMODE:
				wnd._on_lost_window_capture();
				// MSDN says DefWindowProc releases mouse capture but does not say if we should too
				// if we don't it seems that this message can be sent again and again so we might as well
				_details::winapi_check(ReleaseCapture());
				return 0;

			case WM_SETCURSOR:
				{
					if (!wnd.is_mouse_over()) {
						return DefWindowProc(hwnd, msg, wparam, lparam);
					}
					ui::cursor c = wnd.get_current_display_cursor();
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
				window_impl::_ime::get().complete_composition(*wnd_impl);
				lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
				return DefWindowProc(hwnd, msg, wparam, lparam);
			case WM_IME_STARTCOMPOSITION:
				window_impl::_ime::get().start_composition(*wnd_impl);
				return 0;
			case WM_IME_COMPOSITION:
				{
					window_impl::_ime::get().update_composition(*wnd_impl);
					if (auto str = window_impl::_ime::get().get_composition_string(*wnd_impl, lparam)) {
						if (!str->empty()) {
							_wnd_onevent<ui::composition_info>(
								wnd, &ui::window::_on_composition,
								_details::wstring_to_utf8(str->c_str())
								);
						}
					}
					if (auto str = window_impl::_ime::get().get_result(*wnd_impl, lparam)) {
						if (!str->empty()) {
							_wnd_onevent<ui::text_info>(
								wnd, &ui::window::_on_keyboard_text,
								_details::wstring_to_utf8(str->c_str())
								);
						}
					}
					return 0;
				}
			case WM_IME_ENDCOMPOSITION:
				wnd._on_composition_finished();
				window_impl::_ime::get().complete_composition(*wnd_impl);
				break;
			case WM_INPUTLANGCHANGE:
				window_impl::_ime::get().on_input_language_changed();
				return TRUE;

			case WM_PAINT:
				wnd._on_render();
				_details::winapi_check(ValidateRect(hwnd, nullptr));
				return 0;
			}
		}
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}


	window_impl::window_impl(ui::window &wnd) : ui::_details::window_impl(wnd) {
		_wndclass::get();
		_details::winapi_check(_hwnd = CreateWindowEx(
			WS_EX_ACCEPTFILES,
			reinterpret_cast<LPCTSTR>(static_cast<std::size_t>(_wndclass::get().atom)), TEXT("Codepad"),
			WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			nullptr, nullptr, GetModuleHandle(nullptr), nullptr
		));
		// sets the user data of the window to this impl
		SetWindowLongPtr(_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		// update cached dpi, because the WM_DPICHANGED message is not sent after creating the window
		// FIXME this function only returns one value which we now use for both x and y
		UINT dpi = GetDpiForWindow(_hwnd);
		double scaling = dpi / static_cast<double>(USER_DEFAULT_SCREEN_DPI);
		_cached_scaling = vec2d(scaling, scaling);
	}

	window_impl::~window_impl() {
		// when a window is destroyed, its children are also destroyed. the application is notified of the
		// destruction via the WM_DESTROY message. however, another approach to destroy a window is marking the
		// element for destruction. to handle both these scenarios, we reset the GWLP_USERDATA pointer to null before
		// calling DestroyWindow(), and mark the window for destruction and reset _hwnd in _wndproc()
		if (_hwnd) {
			// although it seems fine to also call DestroyWindow() here when the window has already been destroyed by
			// the system, we're gonna play it safe here
			SetWindowLongPtr(_hwnd, GWLP_USERDATA, NULL);
			_details::winapi_check(DestroyWindow(_hwnd));
		}
	}

	void window_impl::_set_parent(ui::window *wnd) {
		if (_hwnd) {
			// what we're really setting here is ownership
			// TODO is a single SetWindowLongPtr enough? not according to https://devblogs.microsoft.com/oldnewthing/20100315-00/?p=14613
			HWND parent = wnd ? _details::cast_window_impl(*wnd->_impl).get_native_handle() : nullptr;
			SetLastError(0);
			if (SetWindowLongPtr(_hwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(parent)) == 0) {
				if (GetLastError() != 0) {
					_details::winapi_check(GetLastError());
				}
			}
		}
	}

	void window_impl::_set_caption(const std::u8string &cap) {
		if (_hwnd) {
			auto u16str = _details::utf8_to_wstring(cap.c_str());
			_details::winapi_check(SetWindowText(_hwnd, reinterpret_cast<LPCWSTR>(u16str.c_str())));
		}
	}

	vec2d window_impl::_get_position() const {
		if (_hwnd) {
			POINT tl;
			tl.x = tl.y = 0;
			_details::winapi_check(ClientToScreen(_hwnd, &tl));
			return vec2d(static_cast<double>(tl.x), static_cast<double>(tl.y));
		}
		return vec2d();
	}

	void window_impl::_set_position(vec2d pos) {
		if (_hwnd) {
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
	}

	vec2d window_impl::_get_client_size() const {
		if (_hwnd) {
			RECT r;
			_details::winapi_check(GetClientRect(_hwnd, &r));
			return _physical_to_logical_position(vec2d(
				static_cast<double>(r.right), static_cast<double>(r.bottom)
			));
		}
		return vec2d();
	}

	void window_impl::_set_client_size(vec2d sz) {
		if (_hwnd) {
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
	}

	void window_impl::_prompt_ready() {
		if (_hwnd) {
			FLASHWINFO fwi;
			std::memset(&fwi, 0, sizeof(fwi));
			fwi.cbSize = sizeof(fwi);
			fwi.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
			fwi.dwTimeout = 0;
			fwi.hwnd = _hwnd;
			fwi.uCount = 0;
			FlashWindowEx(&fwi);
		}
	}

	vec2d window_impl::_screen_to_client(vec2d v) const {
		if (_hwnd) {
			POINT p;
			p.x = static_cast<int>(std::round(v.x));
			p.y = static_cast<int>(std::round(v.y));
			_details::winapi_check(ScreenToClient(_hwnd, &p));
			return _physical_to_logical_position(vec2d(
				static_cast<double>(p.x), static_cast<double>(p.y)
			));
		}
		return vec2d();
	}

	vec2d window_impl::_client_to_screen(vec2d v) const {
		if (_hwnd) {
			v = _logical_to_physical_position(v);
			POINT p;
			p.x = static_cast<int>(std::round(v.x));
			p.y = static_cast<int>(std::round(v.y));
			_details::winapi_check(ClientToScreen(_hwnd, &p));
			return vec2d(static_cast<double>(p.x), static_cast<double>(p.y));
		}
		return vec2d();
	}

	void window_impl::_update_managed_window_size() {
		if (_hwnd) {
			bool
				manage_width = _window.get_width_size_policy() == ui::window::size_policy::application,
				manage_height = _window.get_width_size_policy() == ui::window::size_policy::application;
			if (!manage_width && !manage_height) {
				return;
			}
			RECT client, window_rect;
			_details::winapi_check(GetClientRect(_hwnd, &client));
			_details::winapi_check(GetWindowRect(_hwnd, &window_rect));
			LONG
				new_width = window_rect.right - window_rect.left,
				new_height = window_rect.bottom - window_rect.top;
			// compute new size
			bool has_proportion = false;
			if (manage_width) {
				auto width = _window.get_layout_width();
				has_proportion = has_proportion || !width.is_pixels;
				LONG border_width = new_width - (client.right - client.left);
				new_width = border_width + std::max<LONG>(static_cast<LONG>(width.value * _cached_scaling.x), 0);
			}
			if (manage_height) {
				auto height = _window.get_layout_height();
				has_proportion = has_proportion || !height.is_pixels;
				LONG border_height = new_height - (client.bottom - client.top);
				new_height = border_height + std::max<LONG>(static_cast<LONG>(height.value * _cached_scaling.y), 0);
			}
			if (has_proportion) {
				logger::get().log_warning(CP_HERE) <<
					"window size managed by application, but has a proportional value; "
					"it'll be treated as a pixel value instead";
			}
			// set window size
			_details::winapi_check(SetWindowPos(
				_hwnd, nullptr, 0, 0, new_width, new_height,
				SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER
			));
		}
	}

	window_impl *window_impl::_get_associated_window_impl(HWND hwnd) {
		if (hwnd) {
			DWORD atom;
			_details::winapi_check(atom = GetClassLong(hwnd, GCW_ATOM));
			if (atom == _wndclass::get().atom) { // the correct class
				return reinterpret_cast<window_impl*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			}
		}
		return nullptr;
	}

	void window_impl::_set_window_style_bit(bool v, LONG bit, int type) {
		if (_hwnd) {
			LONG old = GetWindowLong(_hwnd, type);
			SetWindowLongPtr(_hwnd, type, v ? old | bit : old & ~bit);
			_details::winapi_check(SetWindowPos(
				_hwnd, nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
			));
		}
	}

	void window_impl::_setup_mouse_tracking() {
		if (_hwnd && !_mouse_tracked) {
			TRACKMOUSEEVENT tme;
			std::memset(&tme, 0, sizeof(tme));
			tme.cbSize = sizeof(tme);
			tme.dwFlags = TME_HOVER | TME_LEAVE;
			tme.dwHoverTime = HOVER_DEFAULT;
			tme.hwndTrack = _hwnd;
			_details::winapi_check(TrackMouseEvent(&tme));
			_mouse_tracked = true;
		}
	}


	std::optional<std::wstring> window_impl::_ime::_get_string(window_impl &wnd, DWORD type) {
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

	void window_impl::_ime::_update_caret_position(window_impl &wnd) {
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

	void window_impl::_ime::_end_composition(window_impl &wnd, DWORD signal) {
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

	window_impl::_ime &window_impl::_ime::get() {
		static _ime object;
		return object;
	}


	window_impl::_wndclass::_wndclass() {
		WNDCLASSEX wcex;
		memset(&wcex, 0, sizeof(wcex));
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.hInstance = GetModuleHandle(nullptr);
		_details::winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
		wcex.cbSize = sizeof(wcex);
		wcex.lpfnWndProc = _wndproc;
		wcex.lpszClassName = L"Codepad";
		_details::winapi_check(atom = RegisterClassEx(&wcex));
	}

	window_impl::_wndclass &window_impl::_wndclass::get() {
		static _wndclass object;
		return object;
	}
}

namespace codepad::ui {
	std::unique_ptr<_details::window_impl> window::_create_impl(window &wnd) {
		return std::make_unique<os::window_impl>(wnd);
	}
}
