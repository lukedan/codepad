#pragma once

#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <d2d1.h>
#include <dwrite.h>

#include "../font.h"
#include "misc.h"

namespace codepad {
	namespace os {
		// TODO (not urgent):
		//   windows fonts (CreateFont, GetCharABCWidths, GetKerningPairs, GetGlyphOutline, GetGlyphIndices)
		//   directwrite (IDWriteTextLayout::GetPairKerning)
		class freetype_font : public freetype_font_base {
		public:
			freetype_font(const str_t &str, double sz, font_style style) : freetype_font_base() {
				constexpr size_t maximum_font_name_length = 100;
				constexpr DWORD ttcf = 0x66637474;

				std::u16string utf16string = convert_to_utf16(str);
				HFONT font = CreateFont(
					0, 0, 0, 0,
					test_bit_all(static_cast<unsigned>(style), font_style::bold) ? FW_BOLD : FW_NORMAL,
					test_bit_all(static_cast<unsigned>(style), font_style::italic), false, false,
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
				logger::get().log_info(CP_HERE,
					"font loaded: ", convert_to_utf8(reinterpret_cast<const char16_t*>(buf))
				);
				gdi_check(SelectObject(dc, original));
				winapi_check(DeleteObject(font));
				_ft_verify(FT_New_Memory_Face(_get_library().lib, static_cast<FT_Byte*>(_data), size, 0, &_face));
				_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));
			}
		};
		using default_font = freetype_font;
		//using default_font = backed_up_font<freetype_font, freetype_font>;
	}
}
