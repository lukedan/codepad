// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <ShObjIdl.h>
#include <Shlwapi.h>

#include "../windows.h"

using namespace std;

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

#ifdef ENABLE_VIRTUAL_TERMINAL_PROCESSING // enable console output coloring
		HANDLE hstderr = GetStdHandle(STD_ERROR_HANDLE);
		winapi_check(hstderr != INVALID_HANDLE_VALUE);
		DWORD mode;
		winapi_check(GetConsoleMode(hstderr, &mode));
		winapi_check(SetConsoleMode(hstderr, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
#endif
	}

	namespace input {
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
				key v[255];
			} _key_id_backmapping;
		}
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
		window &w, void (window::*handle)(Inf&), Args &&...args
	) {
		Inf inf(forward<Args>(args)...);
		(w.*handle)(inf);
	}
	LRESULT CALLBACK _wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
		auto *form = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
		if (form) {
			switch (msg) {
			case WM_CLOSE:
				form->_on_close_request();
				return 0;

			case WM_SIZE:
				{
					if (wparam != SIZE_MINIMIZED) {
						size_t w = LOWORD(lparam), h = HIWORD(lparam);
						form->_layout = rectd(0.0, static_cast<double>(w), 0.0, static_cast<double>(h));
						size_changed_info p(vec2i(static_cast<int>(w), static_cast<int>(h)));
						if (p.new_size.x > 0 && p.new_size.y > 0) {
							form->_on_size_changed(p);
							ui::manager::get().update_layout_and_visual();
						}
					}
				}
				return 0;

			case WM_SYSKEYDOWN:
				[[fallthrough]]; // same processing
			case WM_KEYDOWN:
				_form_onevent<ui::key_info>(
					*form, &window::_on_key_down, input::_details::_key_id_backmapping.v[wparam]
					);
				break;
			case WM_SYSKEYUP:
				[[fallthrough]];
			case WM_KEYUP:
				_form_onevent<ui::key_info>(
					*form, &window::_on_key_up, input::_details::_key_id_backmapping.v[wparam]
					);
				break;

			case WM_UNICHAR:
				if (wparam == UNICODE_NOCHAR) {
					return TRUE;
				}
				if (wparam != VK_BACK && wparam != VK_ESCAPE) {
					str_t content;
					if (wparam == VK_RETURN) {
						content = CP_STRLIT("\n");
					} else {
						content = reinterpret_cast<const char*>(
							encodings::utf8::encode_codepoint(static_cast<codepoint>(wparam)).c_str()
							);
					}
					_form_onevent<ui::text_info>(*form, &window::_on_keyboard_text, content);
				}
				return FALSE;
			case WM_CHAR:
				if (wparam != VK_BACK && wparam != VK_ESCAPE) {
					str_t content;
					if (wparam == VK_RETURN) {
						content = CP_STRLIT("\n");
					} else {
						auto *ptr = reinterpret_cast<const std::byte*>(&wparam);
						codepoint res;
						if (!encodings::utf16<>::next_codepoint(ptr, ptr + 4, res)) {
							logger::get().log_warning( // TODO check if this will ever be triggered
								CP_HERE, "invalid UTF-16 codepoint, possible faulty windows message handling"
							);
							return 0;
						}
						content = reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(res).c_str());
					}
					_form_onevent<ui::text_info>(*form, &window::_on_keyboard_text, content);
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
				if (!form->is_mouse_over()) {
					form->_on_mouse_enter();
				}
				_form_onevent<ui::mouse_move_info>(
					*form, &window::_on_mouse_move, vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_MOUSELEAVE:
				form->_on_mouse_leave();
				return 0;

			case WM_LBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					input::mouse_button::primary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_LBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					input::mouse_button::primary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_RBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					input::mouse_button::secondary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_RBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					input::mouse_button::secondary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_MBUTTONDOWN:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_down,
					input::mouse_button::tertiary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;
			case WM_MBUTTONUP:
				_form_onevent<ui::mouse_button_info>(
					*form, &window::_on_mouse_up,
					input::mouse_button::tertiary, _get_modifiers(),
					vec2d(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam))
					);
				return 0;

			case WM_SETFOCUS:
				form->_on_got_window_focus();
				return 0;
			case WM_KILLFOCUS:
				form->_on_lost_window_focus();
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
						winapi_check(img);
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
				}
				return 0;
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
		winapi_check(wcex.hCursor = LoadCursor(nullptr, IDC_ARROW));
		wcex.cbSize = sizeof(wcex);
		wcex.lpfnWndProc = _wndproc;
		wcex.lpszClassName = L"Codepad";
		winapi_check(atom = RegisterClassEx(&wcex));
	}


	/// \todo What if some other window has GWLP_USERDATA?
	bool window::_idle() {
		MSG msg;
		// using _hwnd here will cause IMs to malfunction
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
				auto *form = reinterpret_cast<window*>(GetWindowLongPtr(msg.hwnd, GWLP_USERDATA));
				if (form && form->hotkey_manager.on_key_down(key_gesture(
					input::_details::_key_id_backmapping.v[msg.wParam], _get_modifiers()
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


#define CP_USE_LEGACY_OPEN_FILE_DIALOG // new open file dialog doesn't work right now

#ifdef CP_USE_LEGACY_OPEN_FILE_DIALOG
	vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
		const size_t file_buffer_size = 1000;

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
		if (type == file_dialog_type::multiple_selection) {
			ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
		}

		if (GetOpenFileName(&ofn) == TRUE) {
			if (
				ofn.nFileOffset == 0 ||
				ofn.lpstrFile[ofn.nFileOffset - 1] != '\0' ||
				type == file_dialog_type::single_selection
				) {
				return {filesystem::path(ofn.lpstrFile)};
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
			{0},
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
		com_check(pDialogEventHandler->QueryInterface(riid, ppv));
		pDialogEventHandler->Release();
	}
	vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
		const COMDLG_FILTERSPEC file_types = {L"All files", L"*.*"};

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
		com_check(CoCreateInstance(
			CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)
		));
		create_dialog_event_handler(IID_PPV_ARGS(&devents));
		com_check(dialog->Advise(devents, &cookie));
		com_check(dialog->GetOptions(&options));
		options |= FOS_FORCEFILESYSTEM;
		if (type == file_dialog_type::multiple_selection) {
			options |= FOS_ALLOWMULTISELECT;
		} else {
			options &= ~FOS_ALLOWMULTISELECT;
		}
		com_check(dialog->SetOptions(options));
		com_check(dialog->SetFileTypes(1, &file_types));
		com_check(dialog->SetFileTypeIndex(1));
		// TODO bug here: program hangs
		// also no problem if parent is nullptr
		HRESULT res = dialog->Show(wnd ? wnd->get_native_handle() : nullptr);
		if (res == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			return {};
		}
		com_check(res);
		IShellItemArray *files = nullptr;
		DWORD count;
		com_check(dialog->GetResults(&files));
		com_check(files->GetCount(&count));
		vector<filesystem::path> result;
		result.reserve(count);
		for (DWORD i = 0; i < count; ++i) {
			IShellItem *item;
			com_check(files->GetItemAt(i, &item));
			LPWSTR path = nullptr;
			com_check(item->GetDisplayName(SIGDN_FILESYSPATH, &path));
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
}

#if defined(CP_LOG_STACKTRACE) && defined(_MSC_VER)
#	pragma comment(lib, "dbghelp.lib")
#	ifdef UNICODE
#		define DBGHELP_TRANSLATE_TCHAR
#	endif
#	include <DbgHelp.h>
namespace codepad {
	void logger::log_stacktrace() {
		constexpr static DWORD max_frames = 1000;
		constexpr static size_t max_symbol_length = 1000;

		log_custom("STACKTRACE");
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
		for (WORD i = 0; i < numframes; ++i) {
			auto addr = reinterpret_cast<DWORD64>(frames[i]);
			string func = "??", file = func, line = func;
			if (SymFromAddr(proc, addr, nullptr, syminfo)) {
				func = os::_details::wstring_to_utf8(syminfo->Name);
			}
			if (SymGetLineFromAddr64(proc, addr, &line_disp, &lineinfo)) {
				file = os::_details::wstring_to_utf8(lineinfo.FileName);
				line = to_string(lineinfo.LineNumber);
			}
			log_custom("    ", func, "(0x", frames[i], ") @", file, ":", line);
		}
		assert_true_sys(SymCleanup(GetCurrentProcess()), "failed to clean up symbols");
		log_custom("STACKTRACE|END");
	}
}
#endif
