#pragma once

#include <fontconfig/fontconfig.h>

#include "../font.h"

namespace codepad {
	namespace os {
		class freetype_font : public freetype_font_base {
			friend struct codepad::globals;
		public:
			freetype_font(const str_t &str, double sz, font_style style) : freetype_font_base() {
				FcConfig *config = _get_config().refresh_and_get();
				FcPattern *pat = FcNameParse(convert_to_utf8(str).c_str());
				FcPatternAddInteger(pat, FC_SLANT, test_bit_any(
					style, font_style::italic
				) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
				FcPatternAddInteger(pat, FC_WEIGHT, test_bit_any(
					style, font_style::bold
				) ? FC_WEIGHT_BOLD : FC_WEIGHT_NORMAL);
				assert_true_sys(FcConfigSubstitute(config, pat, FcMatchPattern) != FcFalse, "cannot set pattern");
				FcDefaultSubstitute(pat);
				FcResult res;
				FcPattern *font = FcFontMatch(config, pat, &res);
				FcPatternDestroy(pat);
				assert_true_sys(font != nullptr, "cannot find matching font");
				FcChar8 *file;
				assert_true_sys(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch, "cannot get font file name");
				_ft_verify(FT_New_Face(_get_library().lib, reinterpret_cast<const char*>(file), 0, &_face));
				_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));
				FcPatternDestroy(font);

				_cache_kerning();
			}
		protected:
			struct _font_config {
				_font_config() {
					config = FcInitLoadConfigAndFonts();
				}
				~_font_config() {
					FcConfigDestroy(config);
					FcFini();
				}

				FcConfig *config = nullptr;
				FcConfig *refresh_and_get() {
					assert_true_sys(FcInitBringUptoDate() == FcTrue, "cannot refresh font library");
					return config;
				}
			};
			static _font_config &_get_config();
		};
		using default_font = freetype_font;
	}
}
