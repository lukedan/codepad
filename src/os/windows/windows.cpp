// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <ShObjIdl.h>
#include <Shlwapi.h>
#include <processthreadsapi.h>

#include "../../core/logger_sinks.h"
#include "../windows.h"

using namespace std;

using namespace codepad::ui;
using namespace codepad::os;

#ifdef _MSC_VER
#	define CP_CAN_DETECT_MEMORY_LEAKS
#	define _CRTDBG_MAP_ALLOC
#	include <crtdbg.h>
namespace codepad::os {
	namespace _details {
		/// Enables the detection of memory leaks under Windows when compiled with MSVC.
		void _enable_mem_leak_detection() {
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
		}
	}
}
#endif

namespace codepad::os {
	void initialize(int, char**) {
#if defined(CP_CHECK_USAGE_ERRORS) && defined(CP_CAN_DETECT_MEMORY_LEAKS)
		_details::_enable_mem_leak_detection();
#endif

		// set DPI awareness
		// TODO will this work under win7?
		_details::winapi_check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

#ifdef ENABLE_VIRTUAL_TERMINAL_PROCESSING // enable console output coloring
		HANDLE hstderr = GetStdHandle(STD_ERROR_HANDLE);
		_details::winapi_check(hstderr != INVALID_HANDLE_VALUE);
		DWORD mode;
		_details::winapi_check(GetConsoleMode(hstderr, &mode));
		_details::winapi_check(SetConsoleMode(hstderr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
#endif
	}

	namespace _details {
		const int _key_id_mapping[total_num_keys] = {
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
			'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
			'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
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
			VK_LMENU, VK_RMENU
		};
		struct _key_id_backmapping_t {
			_key_id_backmapping_t() {
				for (int i = 0; i < static_cast<int>(total_num_keys); ++i) {
					v[_key_id_mapping[i]] = static_cast<key>(i);
				}
			}
			key v[255]{};
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

	inline bool _get_key_state(int key) {
		return (GetKeyState(key) & 0x8000) != 0;
	}
	inline modifier_keys _get_modifiers() {
		modifier_keys result = modifier_keys::none;
		if (_get_key_state(VK_CONTROL)) {
			result |= modifier_keys::control;
		}
		if (_get_key_state(VK_MENU)) {
			result |= modifier_keys::alt;
		}
		if (_get_key_state(VK_SHIFT)) {
			result |= modifier_keys::shift;
		}
		if (_get_key_state(VK_LWIN) || _get_key_state(VK_RWIN)) {
			result |= modifier_keys::super;
		}
		return result;
	}

	template <typename Inf, typename ...Args> inline void _form_onevent(
		window &w, void (window::*handle)(Inf&), Args && ...args
	) {
		Inf inf(forward<Args>(args)...);
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
					*form, &window::_on_key_down, _details::_key_id_backmapping.v[wparam]
					);
				break;
			case WM_SYSKEYUP:
				[[fallthrough]];
			case WM_KEYUP:
				_form_onevent<ui::key_info>(
					*form, &window::_on_key_up, _details::_key_id_backmapping.v[wparam]
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
					mouse_button::primary, _get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_LBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					mouse_button::primary, _get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_RBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					mouse_button::secondary, _get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_RBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					mouse_button::secondary, _get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_MBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					mouse_button::tertiary, _get_modifiers(),
					form->_update_mouse_position(form->_physical_to_logical_position(
						vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)))
					));
				return 0;
			case WM_MBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					mouse_button::tertiary, _get_modifiers(),
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
					cursor c = form->get_current_display_cursor();
					if (c == cursor::not_specified) {
						return DefWindowProc(hwnd, msg, wparam, lparam);
					}
					if (c == cursor::invisible) {
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
					wstring str;
					if (window::_ime::get().get_composition_string(*form, lparam, str)) {
						if (!str.empty()) {
							_form_onevent<ui::composition_info>(
								*form, &window::_on_composition,
								_details::wstring_to_utf8(str.c_str())
								);
						}
					}
					if (window::_ime::get().get_result(*form, lparam, str)) {
						if (!str.empty()) {
							_form_onevent<ui::text_info>(
								*form, &window::_on_keyboard_text,
								_details::wstring_to_utf8(str.c_str())
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


#define CP_USE_LEGACY_OPEN_FILE_DIALOG // new open file dialog doesn't work right now

#ifdef CP_USE_LEGACY_OPEN_FILE_DIALOG
	vector<filesystem::path> file_dialog::show_open_dialog(const window_base *parent, type type) {
		const std::size_t file_buffer_size = 1000;

#	ifdef CP_CHECK_LOGICAL_ERRORS
		auto *wnd = dynamic_cast<const window*>(parent);
		assert_true_logical((wnd != nullptr) == (parent != nullptr), "invalid window type");
#	else
		const window *wnd = static_cast<const window*>(parent);
#	endif
		OPENFILENAME ofn;
		TCHAR file[file_buffer_size];

		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = wnd ? wnd->get_native_handle() : nullptr;
		ofn.lpstrFile = file;
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = file_buffer_size;
		ofn.lpstrFilter = TEXT("All files\0*.*\0");
		ofn.nFilterIndex = 0;
		ofn.lpstrFileTitle = nullptr;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = nullptr;
		if (type == type::multiple_selection) {
			ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		}

		if (GetOpenFileName(&ofn) == TRUE) {
			if (
				ofn.nFileOffset == 0 ||
				ofn.lpstrFile[ofn.nFileOffset - 1] != '\0' ||
				type == type::single_selection
				) {
				return { filesystem::path(ofn.lpstrFile) };
			}
			filesystem::path wd = filesystem::path(ofn.lpstrFile);
			vector<filesystem::path> paths;
			const TCHAR *cur = ofn.lpstrFile + ofn.nFileOffset;
			for (; *cur != 0; ++cur) {
				paths.push_back(wd / cur);
				for (; *cur != 0; ++cur) {
				}
			}
			return paths;
		}
		return {};
	}
#else
	class dialog_event_handler : public IFileDialogEvents, public IFileDialogControlEvents {
	public:
		// IUnknown methods
		IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) {
			static const QITAB qit[] = {
				QITABENT(dialog_event_handler, IFileDialogEvents),
				QITABENT(dialog_event_handler, IFileDialogControlEvents),
			{ 0 },
			};
			return QISearch(this, qit, riid, ppv);
		}

		IFACEMETHODIMP_(ULONG) AddRef() {
			return InterlockedIncrement(&_cRef);
		}

		IFACEMETHODIMP_(ULONG) Release() {
			long cRef = InterlockedDecrement(&_cRef);
			if (!cRef) {
				delete this;
			}
			return cRef;
		}

		// IFileDialogEvents methods
		IFACEMETHODIMP OnFileOk(IFileDialog*) {
			return S_OK;
		};
		IFACEMETHODIMP OnFolderChange(IFileDialog*) {
			return S_OK;
		};
		IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) {
			return S_OK;
		};
		IFACEMETHODIMP OnHelp(IFileDialog*) {
			return S_OK;
		};
		IFACEMETHODIMP OnSelectionChange(IFileDialog*) {
			return S_OK;
		};
		IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) {
			return S_OK;
		};
		IFACEMETHODIMP OnTypeChange(IFileDialog*) {
			return S_OK;
		}
		IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) {
			return S_OK;
		};

		// IFileDialogControlEvents methods
		IFACEMETHODIMP OnItemSelected(IFileDialogCustomize*, DWORD, DWORD) {
			return S_OK;
		}
		IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) {
			return S_OK;
		};
		IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) {
			return S_OK;
		};
		IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) {
			return S_OK;
		};
	private:
		long _cRef = 1;
	};
	void create_dialog_event_handler(REFIID riid, void **ppv) {
		*ppv = nullptr;
		dialog_event_handler *pDialogEventHandler = new (std::nothrow) dialog_event_handler();
		_details::com_check(pDialogEventHandler->QueryInterface(riid, ppv));
		pDialogEventHandler->Release();
	}
	vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
		const COMDLG_FILTERSPEC file_types = { L"All files", L"*.*" };

#	ifdef CP_CHECK_LOGICAL_ERRORS
		const window *wnd = dynamic_cast<const window*>(parent);
		assert_true_logical((wnd != nullptr) == (parent != nullptr), "invalid window type");
#	else
		const window *wnd = static_cast<const window*>(parent);
#	endif
		_details::com_usage uses_com;
		IFileOpenDialog *dialog = nullptr;
		IFileDialogEvents *devents = nullptr;
		DWORD cookie, options;
		_details::com_check(CoCreateInstance(
			CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)
		));
		create_dialog_event_handler(IID_PPV_ARGS(&devents));
		_details::com_check(dialog->Advise(devents, &cookie));
		_details::com_check(dialog->GetOptions(&options));
		options |= FOS_FORCEFILESYSTEM;
		if (type == file_dialog_type::multiple_selection) {
			options |= FOS_ALLOWMULTISELECT;
		} else {
			options &= ~FOS_ALLOWMULTISELECT;
		}
		_details::com_check(dialog->SetOptions(options));
		_details::com_check(dialog->SetFileTypes(1, &file_types));
		_details::com_check(dialog->SetFileTypeIndex(1));
		// TODO bug here: program hangs
		// also no problem if parent is nullptr
		HRESULT res = dialog->Show(wnd ? wnd->get_native_handle() : nullptr);
		if (res == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			return {};
		}
		_details::com_check(res);
		IShellItemArray *files = nullptr;
		DWORD count;
		_details::com_check(dialog->GetResults(&files));
		_details::com_check(files->GetCount(&count));
		vector<filesystem::path> result;
		result.reserve(count);
		for (DWORD i = 0; i < count; ++i) {
			IShellItem *item;
			_details::com_check(files->GetItemAt(i, &item));
			LPWSTR path = nullptr;
			_details::com_check(item->GetDisplayName(SIGDN_FILESYSPATH, &path));
			result.push_back(filesystem::path(path));
			CoTaskMemFree(path);
		}
		files->Release();
		dialog->Unadvise(cookie);
		devents->Release();
		dialog->Release();
		return result;
	}
#endif


	void clipboard::set_text(std::u8string_view text) {
		// see also utf8_to_wstring
		// allocate global memory for text
		std::size_t utf16_length = 1; // the number of uint16_t's to allocate for the buffer
		if (text.size() > 0) { // MultiByteToWideChar doesn't deal with zero-length strings
			int nchars = MultiByteToWideChar(
				CP_UTF8, 0, reinterpret_cast<const char*>(text.data()),
				static_cast<int>(text.size()), nullptr, 0
			);
			_details::winapi_check(nchars);
			utf16_length = static_cast<std::size_t>(nchars) + 1;
		}
		HGLOBAL buffer = GlobalAlloc(GMEM_MOVEABLE, utf16_length * sizeof(std::uint16_t));
		_details::winapi_check(buffer);
		_details::winapi_check(GlobalLock(buffer));
		{
			if (text.size() > 0) { // convert to utf16
				assert_true_sys(
					MultiByteToWideChar(
						CP_UTF8, 0, reinterpret_cast<const char*>(text.data()), static_cast<int>(text.size()),
						static_cast<LPWSTR>(buffer), static_cast<int>(utf16_length)
					) == utf16_length - 1,
					"failed to convert string from utf8 to utf16 for clipboard"
				);
			}
			static_cast<std::uint16_t*>(buffer)[utf16_length - 1] = 0; // null terminator
		}
		assert_true_sys(GlobalUnlock(buffer) == 0, "failed to unlock memory");
		assert_true_sys(GetLastError() != NO_ERROR, "failed to unlock memory");

		_details::winapi_check(OpenClipboard(nullptr));
		_details::winapi_check(SetClipboardData(CF_UNICODETEXT, buffer));
		_details::winapi_check(CloseClipboard());
		// chrome didn't free the buffer
	}

	bool clipboard::is_text_available() {
		return IsClipboardFormatAvailable(CF_UNICODETEXT);
	}

	std::optional<std::u8string> clipboard::get_text() {
		_details::winapi_check(OpenClipboard(nullptr));
		HANDLE handle = GetClipboardData(CF_UNICODETEXT);
		if (handle == nullptr) { // failed to get data
			_details::winapi_check(CloseClipboard());
			return std::nullopt;
		}
		_details::winapi_check(GlobalLock(handle));
		std::u8string result = _details::wstring_to_utf8(static_cast<LPCWSTR>(handle));
		_details::winapi_check(GlobalUnlock(handle));
		_details::winapi_check(CloseClipboard());
		return result;
	}
}


#ifdef CP_LOG_STACKTRACE
#	ifdef _MSC_VER
#		pragma comment(lib, "dbghelp.lib")
#		ifdef UNICODE
#			define DBGHELP_TRANSLATE_TCHAR
#		endif
#		include <DbgHelp.h>
namespace codepad {
	void logger::log_entry::append_stacktrace() {
		constexpr static DWORD max_frames = 1000;
		constexpr static std::size_t max_symbol_length = 1000;

		void *frames[max_frames];
		HANDLE proc = GetCurrentProcess();
		unsigned char symmem[sizeof(SYMBOL_INFO) + max_symbol_length * sizeof(TCHAR)];
		auto syminfo = reinterpret_cast<PSYMBOL_INFO>(symmem);
		syminfo->MaxNameLen = max_symbol_length;
		syminfo->SizeOfStruct = sizeof(SYMBOL_INFO);
		IMAGEHLP_LINE64 lineinfo;
		lineinfo.SizeOfStruct = sizeof(lineinfo);
		DWORD line_disp;
		assert_true_sys(
			SymInitialize(GetCurrentProcess(), nullptr, true),
			"failed to initialize symbols"
		);
		WORD numframes = CaptureStackBackTrace(0, max_frames, frames, nullptr);

		_contents << "\n-- stacktrace --\n";
		for (WORD i = 0; i < numframes; ++i) {
			auto addr = reinterpret_cast<DWORD64>(frames[i]);
			u8string func = u8"??", file = func;
			string line = "??"; // FIXME this is really ugly
			if (SymFromAddr(proc, addr, nullptr, syminfo)) {
				func = os::_details::wstring_to_utf8(syminfo->Name);
			}
			if (SymGetLineFromAddr64(proc, addr, &line_disp, &lineinfo)) {
				file = os::_details::wstring_to_utf8(lineinfo.FileName);
				line = to_string(lineinfo.LineNumber);
			}
			*this << "  " << func << "(0x" << frames[i] << ") @" << file << ":" << line << "\n";
		}
		_contents << "\n-- stacktrace --\n";

		assert_true_sys(SymCleanup(GetCurrentProcess()), "failed to clean up symbols");
	}
}
#	else
namespace codepad {
	void logger::log_entry::append_stacktrace() {
		_contents << "\n-- [stacktrace not supported] --\n";
	}
}
#	endif
#endif


namespace codepad::logger_sinks {
	std::size_t console_sink::_get_console_width() {
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		_details::winapi_check(out != INVALID_HANDLE_VALUE);
		CONSOLE_SCREEN_BUFFER_INFO info;
		_details::winapi_check(GetConsoleScreenBufferInfo(out, &info));
		return static_cast<std::size_t>(info.srWindow.Right - info.srWindow.Left + 1);
	}
}


namespace codepad::ui {
	/*font_parameters font_manager::get_default_ui_font_parameters() {
		NONCLIENTMETRICS ncmetrics;
		ncmetrics.cbSize = sizeof(ncmetrics);
		_details::winapi_check(SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncmetrics), &ncmetrics, 0));

		const LOGFONT &logfnt = ncmetrics.lfMenuFont;
		HFONT fnt = CreateFontIndirect(&logfnt);
		_details::winapi_check(fnt);
		HDC dc = GetDC(nullptr);
		_details::winapi_check(dc);
		HGDIOBJ original = SelectObject(dc, fnt);
		_details::gdi_check(original);

		TEXTMETRIC metrics;
		_details::winapi_check(GetTextMetrics(dc, &metrics));
		POINT pts[2];
		pts[0].y = 0;
		pts[1].y = metrics.tmHeight - metrics.tmInternalLeading;
		_details::winapi_check(LPtoDP(dc, pts, 2));

		_details::gdi_check(SelectObject(dc, original));
		_details::winapi_check(DeleteObject(fnt));

		return font_parameters(
			os::_details::wstring_to_utf8(logfnt.lfFaceName),
			static_cast<std::size_t>(pts[1].y - pts[0].y),
			(logfnt.lfWeight > FW_REGULAR ? font_style::bold : font_style::normal) |
			(logfnt.lfItalic ? font_style::italic : font_style::normal)
		);
	}*/


	double manager::get_drag_deadzone_radius() {
		return std::abs(GetSystemMetrics(SM_CXDRAG));
	}


	bool scheduler::_main_iteration_system_impl(wait_type ty) {
		MSG msg;
		BOOL res;
		if (ty == wait_type::blocking) {
			res = GetMessage(&msg, nullptr, 0, 0);
			assert_true_sys(res != -1, "GetMessage error");
		} else {
			res = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
		}
		if (res != 0) {
			if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) { // handle hotkeys
				auto *form = os::window::_get_associated_window(msg.hwnd);
				if (form && _hotkeys.on_key_down(key_gesture(
					_details::_key_id_backmapping.v[msg.wParam], _get_modifiers()
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

	void scheduler::_set_timer(std::chrono::high_resolution_clock::duration duration) {
		thread_local static UINT_PTR _timer_handle = 0;

		UINT timeout = std::chrono::duration_cast<std::chrono::duration<UINT, std::milli>>(duration).count();
		_timer_handle = SetTimer(nullptr, _timer_handle, timeout, nullptr);
		assert_true_sys(_timer_handle != 0, "failed to register timer");
	}

	void scheduler::_wake_up() {
		_details::winapi_check(PostThreadMessage(_thread_id, WM_NULL, 0, 0));
	}

	scheduler::thread_id_t scheduler::_get_thread_id() {
		return GetCurrentThreadId();
	}
}
