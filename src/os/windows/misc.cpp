// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "misc.h"

/// \file
/// Implementation of miscellaneous functions.

namespace codepad::os {
	namespace _details {
		std::u8string wstring_to_utf8(LPCWSTR str) {
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

		std::basic_string<WCHAR> utf8_to_wstring(std::u8string_view str) {
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


		wic_image_loader::wic_image_loader() {
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

		com_wrapper<IWICBitmapSource> wic_image_loader::load_image(const std::filesystem::path &filename) {
			com_wrapper<IWICBitmapDecoder> decoder;
			com_wrapper<IWICBitmapFrameDecode> frame;
			_details::com_check(_factory->CreateDecoderFromFilename(
				filename.c_str(), nullptr,
				GENERIC_READ, WICDecodeMetadataCacheOnDemand, decoder.get_ref()
			));
			_details::com_check(decoder->GetFrame(0, frame.get_ref()));
			return frame;
		}


		const int key_id_mapping_t::forward[ui::total_num_keys] = {
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

		key_id_mapping_t key_id_mapping_t::backward;


		bool get_key_state(int key) {
			return (GetKeyState(key) & 0x8000) != 0;
		}

		ui::modifier_keys get_modifiers() {
			ui::modifier_keys result = ui::modifier_keys::none;
			if (get_key_state(VK_CONTROL)) {
				result |= ui::modifier_keys::control;
			}
			if (get_key_state(VK_MENU)) {
				result |= ui::modifier_keys::alt;
			}
			if (get_key_state(VK_SHIFT)) {
				result |= ui::modifier_keys::shift;
			}
			if (get_key_state(VK_LWIN) || get_key_state(VK_RWIN)) {
				result |= ui::modifier_keys::super;
			}
			return result;
		}
	}
}
