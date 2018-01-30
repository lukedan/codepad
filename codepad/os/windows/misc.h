#pragma once

#include <wincodec.h>

#include "../../utilities/misc.h"
#include "../renderer.h"
#include "../misc.h"

namespace codepad {
	namespace os {
		template <typename T> inline void winapi_check(T v) {
#ifdef CP_DETECT_SYSTEM_ERRORS
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
#ifdef CP_DETECT_SYSTEM_ERRORS
			if (v != S_OK) {
				logger::get().log_error(CP_HERE, "COM error code ", v);
				assert_true_sys(false, "COM error");
			}
#endif
		}

		namespace input {
			namespace _details {
				extern const int _key_id_mapping[total_num_keys];
				inline bool is_key_down_id(int vk) {
					return (GetAsyncKeyState(vk) & ~1) != 0;
				}
			}

			inline bool is_key_down(key k) {
				return _details::is_key_down_id(_details::_key_id_mapping[static_cast<int>(k)]);
			}
			inline bool is_mouse_button_down(mouse_button mb) {
				if (mb == mouse_button::left || mb == mouse_button::right) {
					if (GetSystemMetrics(SM_SWAPBUTTON) != 0) {
						mb = (mb == mouse_button::left ? mouse_button::right : mouse_button::left);
					}
				}
				switch (mb) {
				case mouse_button::left:
					return _details::is_key_down_id(VK_LBUTTON);
				case mouse_button::right:
					return _details::is_key_down_id(VK_RBUTTON);
				case mouse_button::middle:
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
		}

		struct wic_image_loader {
		public:
			wic_image_loader() {
				com_check(CoCreateInstance(
					CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&_factory)
				));
			}
			~wic_image_loader() {
				_factory->Release();
			}
			texture load_image(renderer_base &r, const std::filesystem::path &filename) {
				IWICBitmapDecoder *decoder = nullptr;
				com_check(_factory->CreateDecoderFromFilename(
					filename.c_str(), nullptr,
					GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder
				));
				IWICBitmapFrameDecode *frame = nullptr;
				IWICBitmapSource *convertedframe = nullptr;
				com_check(decoder->GetFrame(0, &frame));
				com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &convertedframe));
				frame->Release();
				UINT w, h;
				com_check(convertedframe->GetSize(&w, &h));
				size_t bufsize = 4 * w * h;
				void *buffer = std::malloc(bufsize);
				com_check(convertedframe->CopyPixels(
					nullptr, static_cast<UINT>(4 * w), static_cast<UINT>(bufsize),
					static_cast<BYTE*>(buffer))
				);
				texture res = r.new_texture(static_cast<size_t>(w), static_cast<size_t>(h), buffer);
				std::free(buffer);
				convertedframe->Release();
				decoder->Release();
				return res;
			}

			static wic_image_loader &get();
		protected:
			IWICImagingFactory * _factory = nullptr;
			_details::com_usage _uses_com;
		};
		inline texture load_image(renderer_base &r, const std::filesystem::path &filename) {
			return wic_image_loader::get().load_image(r, filename);
		}
	}
}
