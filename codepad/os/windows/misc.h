// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <wincodec.h>

#include "../../core/misc.h"
#include "../../ui/renderer.h"
#include "../misc.h"

namespace codepad::os {
	template <typename T> inline void winapi_check(T v) {
#ifdef CP_CHECK_SYSTEM_ERRORS
		if (!v) {
			logger::get().log_error(CP_HERE, "WinAPI error code ", GetLastError());
			assert_true_sys(false, "WinAPI error");
		}
#endif
	}
	inline void gdi_check(DWORD v) {
		assert_true_sys(v != GDI_ERROR, "GDI error");
	}
	inline void gdi_check(HGDIOBJ v) {
		assert_true_sys(v != HGDI_ERROR, "GDI error");
	}
	inline void com_check(HRESULT v) {
#ifdef CP_CHECK_SYSTEM_ERRORS
		if (v != S_OK) {
			logger::get().log_error(CP_HERE, "COM error code ", v);
			assert_true_sys(false, "COM error");
		}
#endif
	}

	namespace _details {
		extern const int _key_id_mapping[ui::total_num_keys];
		inline bool is_key_down_id(int vk) {
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		}
	}

	inline bool is_mouse_button_down(ui::mouse_button mb) {
		if (mb == ui::mouse_button::primary || mb == ui::mouse_button::secondary) {
			if (GetSystemMetrics(SM_SWAPBUTTON) != 0) {
				mb = (mb == ui::mouse_button::primary ? ui::mouse_button::secondary : ui::mouse_button::primary);
			}
		}
		switch (mb) {
		case ui::mouse_button::primary:
			return _details::is_key_down_id(VK_LBUTTON);
		case ui::mouse_button::secondary:
			return _details::is_key_down_id(VK_RBUTTON);
		case ui::mouse_button::tertiary:
			return _details::is_key_down_id(VK_MBUTTON);
		}
		return false; // shouldn't happen
	}

	inline vec2i get_mouse_position() {
		POINT p;
		winapi_check(GetCursorPos(&p));
		return vec2i(p.x, p.y);
	}
	inline void set_mouse_position(vec2i p) {
		winapi_check(SetCursorPos(p.x, p.y));
	}

	namespace _details {
		struct com_usage {
			com_usage() {
				HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				if (hr != S_FALSE) {
					com_check(hr);
				}
			}
			com_usage(const com_usage&) = delete;
			com_usage(com_usage&&) = delete;
			com_usage &operator=(const com_usage&) = delete;
			com_usage &operator=(com_usage&&) = delete;
			~com_usage() {
				CoUninitialize();
			}
		};

		/// Converts the given null-terminated UTF-16 string to UTF-8 with the \p WideCharToMultiByte function.
		inline std::string wstring_to_utf8(LPCWSTR str) {
			int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
			assert_true_sys(len != 0, "failed to convert utf16 string to utf8");
			std::string res;
			res.resize(static_cast<size_t>(len));
			assert_true_sys(
				WideCharToMultiByte(CP_UTF8, 0, str, -1, res.data(), len, nullptr, nullptr) == len,
				"failed to convert utf16 string to utf8"
			);
			return res;
		}
		/// Converts the given null-terminated UTF-8 string to UTF-16 with the \p MultiByteToWideChar function.
		inline std::basic_string<WCHAR> utf8_to_wstring(const char *str) {
			int chars = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
			assert_true_sys(chars != 0, "failed to convert utf8 string to utf16");
			std::basic_string<WCHAR> res;
			res.resize(static_cast<size_t>(chars) * 2); // 2 words per char max
			assert_true_sys(
				MultiByteToWideChar(CP_UTF8, 0, str, -1, res.data(), chars) == chars,
				"failed to convert utf16 string to utf8"
			);
			// find the ending of the string
			auto it = res.begin() + chars;
			for (; it != res.end() && *it != 0; ++it) {
			}
			res.erase(it, res.end());
			return res;
		}
	}

	struct wic_image_loader {
	public:
		wic_image_loader() {
			HRESULT res = CoCreateInstance(
				CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory)
			);
#ifdef _MSC_VER
			if (res == REGDB_E_CLASSNOTREG) { // workaround for missing component in win7
				res = CoCreateInstance(
					CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory)
				);
			}
#endif
			com_check(res);
#ifdef __GNUC__
			// workaround for missing function in mingw libs
			_handle = LoadLibrary(TEXT("WindowsCodecs.dll"));
			_conv_func = reinterpret_cast<PWICConvertBitmapSource>(
				GetProcAddress(_handle, "WICConvertBitmapSource")
				);
#endif
		}
		~wic_image_loader() {
			_factory->Release();
#ifdef __GNUC__
			FreeLibrary(_handle);
#endif
		}
		ui::texture load_image(ui::renderer_base &r, const std::filesystem::path &filename) {
			IWICBitmapDecoder *decoder = nullptr;
			com_check(_factory->CreateDecoderFromFilename(
				filename.c_str(), nullptr,
				GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder
			));
			IWICBitmapFrameDecode *frame = nullptr;
			IWICBitmapSource *convertedframe = nullptr;
			com_check(decoder->GetFrame(0, &frame));
#ifdef __GNUC__
			com_check(_conv_func(GUID_WICPixelFormat32bppRGBA, frame, &convertedframe));
#else
			com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &convertedframe));
#endif
			frame->Release();
			UINT w, h;
			com_check(convertedframe->GetSize(&w, &h));
			size_t bufsize = 4 * w * h;
			void *buffer = std::malloc(bufsize);
			com_check(convertedframe->CopyPixels(
				nullptr, static_cast<UINT>(4 * w), static_cast<UINT>(bufsize),
				static_cast<BYTE*>(buffer))
			);
			ui::texture res = r.new_texture(
				static_cast<size_t>(w), static_cast<size_t>(h), static_cast<std::uint8_t*>(buffer)
			);
			std::free(buffer);
			convertedframe->Release();
			decoder->Release();
			return res;
		}

		static wic_image_loader &get();
	protected:
		IWICImagingFactory * _factory = nullptr;
		_details::com_usage _uses_com;
#ifdef __GNUC__
		using PWICConvertBitmapSource =
			HRESULT(WINAPI*)(REFWICPixelFormatGUID, IWICBitmapSource*, IWICBitmapSource**);
		PWICConvertBitmapSource _conv_func = nullptr;
		HMODULE _handle = nullptr;
#endif
	};
}
