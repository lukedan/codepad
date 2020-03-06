// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <wincodec.h>

#include "../../core/misc.h"
#include "../../ui/renderer.h"
#include "../misc.h"

namespace codepad::os {
	namespace _details {
		template <typename T> inline void winapi_check(T v) {
#ifdef CP_CHECK_SYSTEM_ERRORS
			if (!v) {
				logger::get().log_error(CP_HERE) << "WinAPI error code " << GetLastError();
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
				logger::get().log_error(CP_HERE) << "COM error code " << v;
				assert_true_sys(false, "COM error");
			}
#endif
		}

		extern const int _key_id_mapping[ui::total_num_keys];
		inline bool is_key_down_id(int vk) {
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		}

		/// A wrapper for reference-counted COM objects.
		template <typename T> struct com_wrapper final :
			public reference_counted_handle<com_wrapper<T>, T> {
			friend reference_counted_handle<com_wrapper<T>, T>;
		public:
			/// Casting to parent types.
			template <
				typename U, typename = std::enable_if_t<std::is_base_of_v<U, T>, void>
			> operator com_wrapper<U>() {
				auto ptr = static_cast<U*>(_handle);
				com_wrapper<U> res;
				res.set_share(ptr);
				return res;
			}

			/// Returns a pointer to the underlying pointer. The existing pointer will be released. This is mainly
			/// useful when creating objects.
			T **get_ref() {
				_check_release();
				return &_handle;
			}
		protected:
			/// Releases \ref _handle.
			void _do_release() {
				_handle->Release();
			}
			/// Increments the reference count of \ref _handle.
			void _do_add_ref() {
				_handle->AddRef();
			}
		};
		/// Creates a new \ref com_wrapper, and calls \ref com_wrapper::set_share() to share the given pointer.
		template <typename T> inline com_wrapper<T> make_com_wrapper_share(T *ptr) {
			com_wrapper<T> res;
			res.set_share(ptr);
			return res;
		}
		/// Creates a new \ref com_wrapper, and calls \ref com_wrapper::set_give() to give the given pointer to it.
		template <typename T> inline com_wrapper<T> make_com_wrapper_give(T *ptr) {
			com_wrapper<T> res;
			res.set_give(ptr);
			return res;
		}

		struct com_usage {
			com_usage() {
				HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
				if (hr != S_FALSE) {
					_details::com_check(hr);
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
		inline std::u8string wstring_to_utf8(LPCWSTR str) {
			int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
			assert_true_sys(len != 0, "failed to convert utf16 string to utf8");
			std::u8string res;
			res.resize(static_cast<std::size_t>(len));
			assert_true_sys(
				WideCharToMultiByte(
					CP_UTF8, 0, str, -1, reinterpret_cast<char*>(res.data()), len, nullptr, nullptr
				) == len,
				"failed to convert utf16 string to utf8"
			);
			// remove duplicate null terminator that WideCharToMultiByte introduces when cchWideChar is -1
			res.pop_back();
			return res;
		}
		/// Converts the given UTF-8 string to UTF-16 with the \p MultiByteToWideChar function.
		inline std::basic_string<WCHAR> utf8_to_wstring(std::u8string_view str) {
			if (str.size() == 0) { // MultiByteToWideChar doesn't deal with zero-length strings
				return L"";
			}
			int chars = MultiByteToWideChar(
				CP_UTF8, 0, reinterpret_cast<const char*>(str.data()), static_cast<int>(str.length()), nullptr, 0
			);
			assert_true_sys(chars != 0, "failed to convert utf8 string to utf16");
			std::basic_string<WCHAR> res;
			// here we assume that by "character" the documentation means a single uint16
			res.resize(static_cast<std::size_t>(chars));
			assert_true_sys(MultiByteToWideChar(
				CP_UTF8, 0, reinterpret_cast<const char*>(str.data()), static_cast<int>(str.length()),
				res.data(), chars
			) == chars, "failed to convert utf8 string to utf16");
			return res;
		}


		/// Loads images using WIC.
		struct wic_image_loader {
		public:
			/// Initializes \ref _factory.
			wic_image_loader() {
				HRESULT res = CoCreateInstance(
					CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_factory.get_ref())
				);
				if (res == REGDB_E_CLASSNOTREG) { // workaround for missing component in win7
					res = CoCreateInstance(
						CLSID_WICImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(_factory.get_ref())
					);
				}
				_details::com_check(res);
			}
			/// Loads an image. The pixel foramt of the image is not certain; use \p WICConvertBitmapSource to
			/// convert it to the desired format.
			com_wrapper<IWICBitmapSource> load_image(const std::filesystem::path &filename) {
				com_wrapper<IWICBitmapDecoder> decoder;
				com_wrapper<IWICBitmapFrameDecode> frame;
				_details::com_check(_factory->CreateDecoderFromFilename(
					filename.c_str(), nullptr,
					GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.get_ref()
				));
				_details::com_check(decoder->GetFrame(0, frame.get_ref()));
				return frame;
			}

			static wic_image_loader &get();
		protected:
			com_wrapper<IWICImagingFactory> _factory;
			com_usage _uses_com;
		};
	}
}
