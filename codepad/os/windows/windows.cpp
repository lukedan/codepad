#include <ShObjIdl.h>
#include <Shlwapi.h>

#include "../windows.h"

using namespace std;

namespace codepad {
	namespace os {
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

		template <typename Inf, typename ...Args> inline void _form_onevent(
			window &w, void (window::*handle)(Inf&), Args &&...args
		) {
			Inf inf(forward<Args>(args)...);
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

				case WM_SYSKEYDOWN:
					[[fallthrough]]; // same processing
				case WM_KEYDOWN:
					_form_onevent<ui::key_info>(
						*form, &window::_on_key_down, input::_details::_key_id_backmapping.v[wparam]
						);
					return 0;
				case WM_SYSKEYUP:
					[[fallthrough]];
				case WM_KEYUP:
					_form_onevent<ui::key_info>(
						*form, &window::_on_key_up, input::_details::_key_id_backmapping.v[wparam]
						);
					return 0;

				case WM_UNICHAR:
					if (wparam != VK_BACK && wparam != VK_ESCAPE) {
						_form_onevent<ui::text_info>(
							*form, &window::_on_keyboard_text,
							wparam == VK_RETURN ? CP_STRLIT("\n") : str_t({static_cast<char_t>(wparam)})
							);
					}
					return (wparam == UNICODE_NOCHAR ? TRUE : FALSE);
				case WM_CHAR:
					if (wparam != VK_BACK && wparam != VK_ESCAPE) {
						_form_onevent<ui::text_info>(
							*form, &window::_on_keyboard_text,
							wparam == VK_RETURN ? CP_STRLIT("\n") : str_t({static_cast<char_t>(wparam)})
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
					if (!form->is_mouse_over()) {
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
						cursor c = form->get_current_display_cursor();
						if (c == cursor::not_specified) {
							return DefWindowProc(hwnd, msg, wparam, lparam);
						} else if (c == cursor::invisible) {
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
			memset(&wcex, 0, sizeof(wcex));
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
					(msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) &&
					hotkey_manager.on_key_down(input::_details::_key_id_backmapping.v[msg.wParam])
					)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
				return true;
			}
			return false;
		}


#define CP_USE_LEGACY_OPEN_FILE_DIALOG // new open file dialog doesn't work right now

#ifdef CP_USE_LEGACY_OPEN_FILE_DIALOG
		vector<filesystem::path> open_file_dialog(const window_base *parent, file_dialog_type type) {
			const size_t file_buffer_size = 1000;

#	ifdef CP_DETECT_LOGICAL_ERRORS
			const window *wnd = dynamic_cast<const window*>(parent);
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
				} else {
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

#	ifdef CP_DETECT_LOGICAL_ERRORS
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
			HRESULT res = dialog->Show(wnd ? wnd->get_native_handle() : nullptr); // TODO bug here: program hangs
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
	void logger::log_stacktrace() {
		constexpr static DWORD max_frames = 1000;
		constexpr static size_t max_symbol_length = 1000;

		log_custom("STACKTRACE");
		void *frames[max_frames];
		HANDLE proc = GetCurrentProcess();
		unsigned char symmem[sizeof(SYMBOL_INFO) + max_symbol_length * sizeof(TCHAR)];
		PSYMBOL_INFO syminfo = reinterpret_cast<PSYMBOL_INFO>(symmem);
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
			DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
			string func = "??", file = func, line = func;
			if (SymFromAddr(proc, addr, nullptr, syminfo)) {
				const char16_t *funcstr = reinterpret_cast<const char16_t*>(syminfo->Name);
				func = convert_encoding<string>(funcstr, funcstr + get_unit_count(funcstr));
			}
			if (SymGetLineFromAddr64(proc, addr, &line_disp, &lineinfo)) {
				const char16_t *fnstr = reinterpret_cast<const char16_t*>(lineinfo.FileName);
				file = convert_encoding<string>(fnstr, fnstr + get_unit_count(fnstr));
				line = to_string(lineinfo.LineNumber);
			}
			log_custom("    ", func, "(0x", frames[i], ") @", file, ":", line);
		}
		assert_true_sys(SymCleanup(GetCurrentProcess()), "failed to clean up symbols");
		log_custom("STACKTRACE|END");
	}
#endif
}
