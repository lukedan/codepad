// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of miscellaneous platform-specific functions.

#include <ShObjIdl.h>
#include <Shlwapi.h>
#include <processthreadsapi.h>
#include <commdlg.h>

#include "codepad/ui/manager.h"
#include "codepad/os/windows/misc.h"
#include "codepad/os/windows/window.h"

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


#ifdef CP_USE_LEGACY_OPEN_FILE_DIALOG
	std::vector<std::filesystem::path> file_dialog::show_open_dialog(const ui::window *parent, type type) {
		const std::size_t file_buffer_size = 1000;

		OPENFILENAME ofn;
		TCHAR file[file_buffer_size];

		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = parent ? _details::cast_window_impl(parent->get_impl()).get_native_handle() : nullptr;
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
				return { std::filesystem::path(ofn.lpstrFile) };
			}
			std::filesystem::path wd(ofn.lpstrFile);
			std::vector<std::filesystem::path> paths;
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
		IFACEMETHODIMP OnFileOk(IFileDialog*) override {
			return S_OK;
		};
		IFACEMETHODIMP OnFolderChange(IFileDialog*) override {
			return S_OK;
		};
		IFACEMETHODIMP OnFolderChanging(IFileDialog*, IShellItem*) override {
			return S_OK;
		};
		IFACEMETHODIMP OnSelectionChange(IFileDialog*) override {
			return S_OK;
		};
		IFACEMETHODIMP OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) override {
			return S_OK;
		};
		IFACEMETHODIMP OnTypeChange(IFileDialog*) override {
			return S_OK;
		}
		IFACEMETHODIMP OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) override {
			return S_OK;
		};

		// IFileDialogControlEvents methods
		IFACEMETHODIMP OnItemSelected(IFileDialogCustomize*, DWORD, DWORD) override {
			return S_OK;
		}
		IFACEMETHODIMP OnButtonClicked(IFileDialogCustomize*, DWORD) override {
			return S_OK;
		};
		IFACEMETHODIMP OnCheckButtonToggled(IFileDialogCustomize*, DWORD, BOOL) override {
			return S_OK;
		};
		IFACEMETHODIMP OnControlActivating(IFileDialogCustomize*, DWORD) override {
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
	std::vector<std::filesystem::path> file_dialog::show_open_dialog(const ui::window *parent, type type) {
		const COMDLG_FILTERSPEC file_types = { TEXT("All files"), TEXT("*.*") };

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
		if (type == type::multiple_selection) {
			options |= FOS_ALLOWMULTISELECT;
		} else {
			options &= ~FOS_ALLOWMULTISELECT;
		}
		_details::com_check(dialog->SetOptions(options));
		_details::com_check(dialog->SetFileTypes(1, &file_types));
		_details::com_check(dialog->SetFileTypeIndex(1)); // this index is one-based
		HRESULT res = dialog->Show(
			parent ? _details::cast_window_impl(parent->get_impl()).get_native_handle() : nullptr
		);

		// gather result
		std::vector<std::filesystem::path> result;
		if (res != HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
			_details::com_check(res);
			IShellItemArray *files = nullptr;
			DWORD count;
			_details::com_check(dialog->GetResults(&files));
			_details::com_check(files->GetCount(&count));
			result.reserve(count);
			for (DWORD i = 0; i < count; ++i) {
				IShellItem *item;
				_details::com_check(files->GetItemAt(i, &item));
				LPWSTR path = nullptr;
				_details::com_check(item->GetDisplayName(SIGDN_FILESYSPATH, &path));
				result.push_back(std::filesystem::path(path));
				CoTaskMemFree(path);
			}
			files->Release();
		}
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


	double system_parameters::get_drag_deadzone_radius() {
		return std::abs(GetSystemMetrics(SM_CXDRAG));
	}

	std::size_t system_parameters::get_console_width() {
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
		os::_details::winapi_check(out != nullptr);
		os::_details::winapi_check(out != INVALID_HANDLE_VALUE);
		CONSOLE_SCREEN_BUFFER_INFO info;
		os::_details::winapi_check(GetConsoleScreenBufferInfo(out, &info));
		return static_cast<std::size_t>(info.srWindow.Right - info.srWindow.Left + 1);
	}

	namespace _details {
		void quote_cmd_arg(std::u8string_view arg, std::wstring &append_to, bool force) {
			bool has_meta = force;

			// first decode the argument
			std::u32string decoded;
			decoded.reserve(arg.size());
			for (auto it = arg.begin(); it != arg.end(); ) {
				codepoint cp;
				if (!encodings::utf8::next_codepoint(it, arg.end(), cp)) {
					cp = unicode::replacement_character;
				}
				has_meta = has_meta || (cp == U' ' || cp == U'\t' || cp == U'\n' || cp == U'\v' || cp == U'"');
				decoded.push_back(static_cast<char32_t>(cp));
			}

			// no meta characters, append directly
			if (!has_meta) {
				for (char32_t c : decoded) {
					auto str = encodings::utf16<>::encode_codepoint(c);
					append_to.append(std::wstring_view(
						reinterpret_cast<const wchar_t*>(str.data()), str.size() / 2
					));
				}
				return;
			}

			append_to.push_back(L'"');
			for (auto it = decoded.begin(); ; ++it) {
				std::size_t num_backslashes = 0;
				while (it != decoded.end() && *it == U'\\') {
					++it;
					++num_backslashes;
				}
				if (it == decoded.end()) {
					// escape all backslashes, but let the terminating double quotation mark we add below be
					// interpreted as a metacharacter
					append_to.append(num_backslashes * 2, L'\\');
					break;
				} else if (*it == U'"') {
					// escape all backslashes and the following double quotation mark
					append_to.append(num_backslashes * 2 + 1, L'\\');
					append_to.push_back(L'"');
				} else {
					// backslashes aren't special here
					append_to.append(num_backslashes, L'\\');
					auto str = encodings::utf16<>::encode_codepoint(*it);
					append_to.append(std::wstring_view(
						reinterpret_cast<const wchar_t*>(str.data()), str.size() / 2
					));
				}
			}
			append_to.push_back(L'"');
		}
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
			std::u8string func = u8"??", file = func;
			std::string line = "??"; // FIXME this is really ugly
			if (SymFromAddr(proc, addr, nullptr, syminfo)) {
				func = os::_details::wstring_to_utf8(syminfo->Name);
			}
			if (SymGetLineFromAddr64(proc, addr, &line_disp, &lineinfo)) {
				file = os::_details::wstring_to_utf8(lineinfo.FileName);
				line = std::to_string(lineinfo.LineNumber);
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
}
