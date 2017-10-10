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
		class gdi_font : public font {
			// TODO
		};
		class directwrite_font : public font {
			friend struct codepad::globals;
		public:
			directwrite_font(const str_t &s, size_t sz, font_style style) : font() {
				std::u16string utf16str = convert_to_utf16(s);
				_factory &f = _get_factory();
				com_check(f.dwrite_factory->CreateTextFormat(
					reinterpret_cast<const TCHAR*>(utf16str.c_str()), nullptr, (
						test_bit_all(static_cast<unsigned>(style), font_style::bold) ?
						DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_REGULAR
						), (
							test_bit_all(static_cast<unsigned>(style), font_style::italic) ?
							DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL
							), DWRITE_FONT_STRETCH_NORMAL, static_cast<FLOAT>(sz), TEXT(""), &_format
				));
				com_check(_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
				com_check(_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
			}
			~directwrite_font() override {
				_format->Release();
			}

			const entry &get_char_entry(char_t) const override {
				// TODO
			}

			double height() const override {
				// TODO
			}
			double max_width() const override {
				// TODO
			}
			double baseline() const override {
				// TODO
			}
			vec2d get_kerning(char_t, char_t) const override {
				return vec2d(); // FIXME directwrite doesn't support getting kerning directly
			}
		protected:
			IDWriteTextFormat *_format = nullptr;

			struct _factory {
				_factory() {
					com_check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2d1_factory));
					com_check(DWriteCreateFactory(
						DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
						reinterpret_cast<IUnknown**>(&dwrite_factory)
					));
				}
				~_factory() {
					d2d1_factory->Release();
					dwrite_factory->Release();
				}

				ID2D1Factory *d2d1_factory = nullptr;
				IDWriteFactory *dwrite_factory = nullptr;
			};
			static _factory &_get_factory();
		};
		using default_font = freetype_font;
	}
}
