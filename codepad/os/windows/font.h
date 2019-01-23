// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <d2d1.h>
#include <dwrite.h>

#include "../../ui/font.h"
#include "../freetype_font_base.h"
#include "misc.h"

namespace codepad::os {
	// TODO (not urgent):
	//   windows fonts (CreateFont, GetCharABCWidths, GetKerningPairs, GetGlyphOutline, GetGlyphIndices)
	//   directwrite (IDWriteTextLayout::GetPairKerning)
	class freetype_font : public freetype_font_base {
	public:
		freetype_font(ui::font_manager &man, str_view_t str, double sz, ui::font_style style) :
			freetype_font_base(man) {

			constexpr size_t maximum_font_name_length = 100;
			constexpr DWORD ttcf = 0x66637474;

			std::wstring utf16string = _details::utf8_to_wstring(str.data());
			HFONT font = CreateFont(
				0, 0, 0, 0,
				(style & ui::font_style::bold) != ui::font_style::normal ? FW_BOLD : FW_NORMAL,
				(style & ui::font_style::italic) != ui::font_style::normal, false, false,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE,
				reinterpret_cast<const TCHAR*>(utf16string.c_str())
			);
			winapi_check(font);
			HDC dc = GetDC(nullptr);
			winapi_check(dc);
			HGDIOBJ original = SelectObject(dc, font);
			gdi_check(original);
			DWORD table = ttcf, size = GetFontData(dc, ttcf, 0, nullptr, 0);
			if (size == GDI_ERROR) {
				table = 0;
				size = GetFontData(dc, 0, 0, nullptr, 0);
			}
			gdi_check(size);
			_data = std::malloc(size);
			assert_true_sys(GetFontData(dc, table, 0, _data, size) == size, "error getting font data");
			TCHAR buf[maximum_font_name_length];
			GetTextFace(dc, maximum_font_name_length, buf);
			logger::get().log_info(CP_HERE, "font loaded: ", _details::wstring_to_utf8(buf));
			gdi_check(SelectObject(dc, original));
			winapi_check(DeleteObject(font));
			_ft_verify(FT_New_Memory_Face(_library::get().lib, static_cast<FT_Byte*>(_data), size, 0, &_face));
			_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));

			_cache_kerning();
		}
		~freetype_font() override {
			_ft_verify(FT_Done_Face(_face));
			std::free(_data);
		}
	protected:
		void *_data = nullptr;
	};
}
