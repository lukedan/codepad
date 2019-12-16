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

	namespace _details {
		extern const int _key_id_mapping[ui::total_num_keys];
		inline bool is_key_down_id(int vk) {
			return (GetAsyncKeyState(vk) & 0x8000) != 0;
		}

		/// A wrapper for reference-counted COM objects.
		template <typename T> struct com_wrapper {
		public:
			/// Default constructor.
			com_wrapper() = default;
			/// Copy constructor.
			com_wrapper(const com_wrapper &src) : _ptr(src._ptr) {
				_check_add_ref();
			}
			/// Move constructor.
			com_wrapper(com_wrapper &&src) noexcept : _ptr(src._ptr) {
				src._ptr = nullptr;
			}
			/// Copy assignment.
			com_wrapper &operator=(const com_wrapper &src) {
				set_share(src._ptr);
				return *this;
			}
			/// Move assignment.
			com_wrapper &operator=(com_wrapper &&src) noexcept {
				set_give(src._ptr);
				src._ptr = nullptr;
				return *this;
			}
			/// Releases the poitner.
			~com_wrapper() {
				_check_release();
			}

			/// Casting to parent types.
			template <
				typename U, typename = std::enable_if_t<std::is_base_of_v<U, T>, void>
			> operator com_wrapper<U>() {
				auto ptr = static_cast<U*>(_ptr);
				com_wrapper<U> res;
				res.set_share(ptr);
				return res;
			}

			/// Sets the underlying pointer and increment the reference count.
			com_wrapper &set_share(T *ptr) {
				_check_release();
				_ptr = ptr;
				_check_add_ref();
				return *this;
			}
			/// Sets the underlying pointer without incrementing the reference count.
			com_wrapper &set_give(T *ptr) {
				_check_release();
				_ptr = ptr;
				return *this;
			}
			/// Releases the currently holding object.
			com_wrapper &reset() {
				return set_give(nullptr);
			}

			/// Returns the underlying pointer.
			T *get() const {
				return _ptr;
			}
			/// Returns a pointer to the underlying pointer. The existing pointer will be released. This is mainly
			/// useful when creating objects.
			T **get_ref() {
				_check_release();
				_ptr = nullptr;
				return &_ptr;
			}
			/// Convenience operator.
			T *operator->() const {
				return get();
			}

			/// Returns whether this wrapper holds no objects.
			bool empty() const {
				return _ptr == nullptr;
			}
			/// Returns whether this brush has any content.
			explicit operator bool() const {
				return !empty();
			}
		protected:
			T *_ptr = nullptr; ///< The pointer.

			/// Checks and releases \ref _ptr.
			void _check_release() {
				if (_ptr) {
					_ptr->Release();
				}
			}
			/// Checks and increments the reference count of \ref _ptr.
			void _check_add_ref() {
				if (_ptr) {
					_ptr->AddRef();
				}
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
			res.resize(static_cast<std::size_t>(len));
			assert_true_sys(
				WideCharToMultiByte(CP_UTF8, 0, str, -1, res.data(), len, nullptr, nullptr) == len,
				"failed to convert utf16 string to utf8"
			);
			return res;
		}
		/// Converts the given null-terminated UTF-8 string to UTF-16 with the \p MultiByteToWideChar function.
		inline std::basic_string<WCHAR> utf8_to_wstring(std::string_view str) { // TODO use u8string_view
			if (str.size() == 0) {
				return L"";
			}
			int chars = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.length()), nullptr, 0);
			assert_true_sys(chars != 0, "failed to convert utf8 string to utf16");
			std::basic_string<WCHAR> res;
			res.resize(static_cast<std::size_t>(chars) * 2); // 2 words per char max
			assert_true_sys(MultiByteToWideChar(
				CP_UTF8, 0, str.data(), static_cast<int>(str.length()), res.data(), chars
			) == chars, "failed to convert utf16 string to utf8");
			// find the ending of the string
			auto it = res.begin() + chars;
			for (; it != res.end() && *it != 0; ++it) {
			}
			res.erase(it, res.end());
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
				com_check(res);
			}
			/// Loads an image. The pixel foramt of the image is not certain; use \p WICConvertBitmapSource to convert
			/// it to the desired format.
			com_wrapper<IWICBitmapSource> load_image(const std::filesystem::path &filename) {
				com_wrapper<IWICBitmapDecoder> decoder;
				com_wrapper<IWICBitmapFrameDecode> frame;
				com_check(_factory->CreateDecoderFromFilename(
					filename.c_str(), nullptr,
					GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.get_ref()
				));
				com_check(decoder->GetFrame(0, frame.get_ref()));
				return frame;
			}

			static wic_image_loader &get();
		protected:
			com_wrapper<IWICImagingFactory> _factory;
			com_usage _uses_com;
		};
	}
}
