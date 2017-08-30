#pragma once

#include <Windows.h>
#include <windowsx.h>
#undef min
#undef max
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
				logger::get().log_error(CP_HERE, "COM error code ", GetLastError());
				assert_true_sys(false, "COM error");
			}
#endif
		}

		namespace input {
			extern const int _key_id_mapping[total_num_keys];
			inline bool is_key_down(key k) {
				return (GetAsyncKeyState(_key_id_mapping[static_cast<int>(k)]) & ~1) != 0;
			}
			inline bool is_mouse_button_swapped() {
				return GetSystemMetrics(SM_SWAPBUTTON) != 0;
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

		struct wic_image_loader {
			wic_image_loader() {
				com_check(CoCreateInstance(
					CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
					IID_IWICImagingFactory, reinterpret_cast<LPVOID*>(&factory)
				));
			}
			~wic_image_loader() {
				factory->Release();
			}
			texture_id load_image(renderer_base &r, const str_t &filename) {
				IWICBitmapDecoder *decoder = nullptr;
				std::u16string fn = convert_to_utf16(filename);
				com_check(factory->CreateDecoderFromFilename(
					reinterpret_cast<LPCWSTR>(fn.c_str()), nullptr,
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
				texture_id res = r.new_texture(static_cast<size_t>(w), static_cast<size_t>(h), buffer);
				std::free(buffer);
				convertedframe->Release();
				decoder->Release();
				return res;
			}

			static wic_image_loader &get();

			IWICImagingFactory *factory = nullptr;
		};
		inline texture_id load_image(renderer_base &r, const str_t &filename) {
			return wic_image_loader::get().load_image(r, filename);
		}
	}
}
