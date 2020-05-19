// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <wincodec.h>

#include "../../core/misc.h"
#include "../../ui/hotkey_registry.h"
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

		/// A wrapper for reference-counted COM objects.
		template <typename T> struct com_wrapper final : public reference_counted_handle<com_wrapper<T>, T> {
		private:
			using _handle_base = reference_counted_handle<com_wrapper<T>, T>;
			friend _handle_base;
		public:
			/// Casting to parent types.
			template <
				typename U, typename = std::enable_if_t<std::is_base_of_v<U, T>, void>
			> operator com_wrapper<U>() {
				auto ptr = static_cast<U*>(this->_handle);
				com_wrapper<U> res;
				res.set_share(ptr);
				return res;
			}

			/// Returns a pointer to the underlying pointer. The existing pointer will be released. This is mainly
			/// useful when creating objects.
			T **get_ref() {
				_handle_base::_check_release();
				return &this->_handle;
			}
		protected:
			/// Releases \ref _handle.
			void _do_release() {
				this->_handle->Release();
			}
			/// Increments the reference count of \ref _handle.
			void _do_add_ref() {
				this->_handle->AddRef();
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
		std::u8string wstring_to_utf8(LPCWSTR);
		/// Converts the given UTF-8 string to UTF-16 with the \p MultiByteToWideChar function.
		std::basic_string<WCHAR> utf8_to_wstring(std::u8string_view);


		/// Loads images using WIC.
		struct wic_image_loader {
		public:
			/// Initializes \ref _factory.
			wic_image_loader();
			/// Loads an image. The pixel foramt of the image is not certain; use \p WICConvertBitmapSource to
			/// convert it to the desired format.
			com_wrapper<IWICBitmapSource> load_image(const std::filesystem::path&);

			static wic_image_loader &get();
		protected:
			com_wrapper<IWICImagingFactory> _factory;
			com_usage _uses_com;
		};


		/// Mapping from virtual key codes to \ref ui::key.
		struct key_id_mapping_t {
			/// Initializer.
			key_id_mapping_t() {
				for (int i = 0; i < static_cast<int>(ui::total_num_keys); ++i) {
					value[forward[i]] = static_cast<ui::key>(i);
				}
			}
			ui::key value[255]{}; ///< All key values.

			static const int forward[ui::total_num_keys]; ///< Forward mapping from \ref ui::key to virtual key codes.
			static key_id_mapping_t backward; ///< The static object.
		};

		/// Determines if the given key is held down by calling \p GetKeyState().
		bool get_key_state(int key);
		/// Gets the set of modifier keys that are held down.
		ui::modifier_keys get_modifiers();
	}
}
