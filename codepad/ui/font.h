#pragma once

#include <string>
#include <unordered_map>
#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../utilities/textconfig.h"
#include "../utilities/misc.h"
#include "../os/renderer.h"

namespace codepad {
	namespace ui {
		class font {
		public:
			struct entry {
				rectd placement;
				double advance;
				os::texture_id texture;
			};

			font(const std::string &str, size_t sz) {
				_ft_verify(FT_New_Face(_lib.lib, str.c_str(), 0, &_face));
				_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));
			}
			font(const font&) = delete;
			font(font&&) = delete;
			font &operator=(const font&) = delete;
			font &operator=(font&&) = delete;
			~font() {
				for (auto i = _map.begin(); i != _map.end(); ++i) {
					os::renderer_base::get().delete_character_texture(i->second.texture);
				}
				_ft_verify(FT_Done_Face(_face));
			}

			const entry &get_char_entry(char_t c) const {
				auto found = _map.find(c);
				if (found == _map.end()) {
					_ft_verify(FT_Load_Char(_face, c, FT_LOAD_DEFAULT | FT_LOAD_RENDER));
					const FT_Bitmap &bmpdata = _face->glyph->bitmap;
					entry &et = _map[c] = entry();
					et.texture = os::renderer_base::get().new_character_texture(bmpdata.width, bmpdata.rows, bmpdata.buffer);
					et.advance = _face->glyph->metrics.horiAdvance * _ft_fixed_scale;
					et.placement = rectd::from_xywh(
						_face->glyph->metrics.horiBearingX * _ft_fixed_scale,
						(_face->size->metrics.ascender - _face->glyph->metrics.horiBearingY) * _ft_fixed_scale,
						bmpdata.width,
						bmpdata.rows
					);
					return et;
				}
				return found->second;
			}

			double height() const {
				return _face->size->metrics.height * _ft_fixed_scale;
			}
			double max_width() const {
				return _face->size->metrics.max_advance * _ft_fixed_scale;
			}
			double baseline() const {
				return _face->size->metrics.ascender * _ft_fixed_scale;
			}
			vec2d get_kerning(char_t left, char_t right) const {
				FT_Vector v;
				_ft_verify(FT_Get_Kerning(_face, FT_Get_Char_Index(_face, left), FT_Get_Char_Index(_face, right), FT_KERNING_UNFITTED, &v));
				return vec2d(v.x, v.y) * _ft_fixed_scale;
			}
		protected:
			constexpr static double _ft_fixed_scale = 1.0 / 64.0;

			FT_Face _face = nullptr;
			mutable std::unordered_map<char_t, entry> _map;

			inline static void _ft_verify(FT_Error code) {
				if (code != 0) {
					logger::get().log_error(CP_HERE, "FreeType error code ", code);
					assert_true_sys(false, "FreeType error");
				}
			}

			struct _library {
				_library() {
					_ft_verify(FT_Init_FreeType(&lib));
				}
				~_library() {
					_ft_verify(FT_Done_FreeType(lib));
				}

				FT_Library lib = nullptr;
			};
			static _library _lib;
		};
		enum class font_style {
			normal = 0,
			bold = 1,
			italic = 2,
			bold_italic = bold | italic
		};
		struct font_family {
			struct baseline_info {
				baseline_info() = default;
				baseline_info(double n, double b, double i, double bi) :
					normal_diff(n), bold_diff(b), italic_diff(i), bold_italic_diff(bi) {
				}

				double normal_diff, bold_diff, italic_diff, bold_italic_diff;

				double get(font_style fs) {
					switch (fs) {
					case font_style::normal:
						return normal_diff;
					case font_style::bold:
						return bold_diff;
					case font_style::italic:
						return italic_diff;
					case font_style::bold_italic:
						return bold_italic_diff;
					}
					assert_true_usage(false, "invalid font style encountered");
					return 0.0;
				}
			};

			font_family() = default;
			font_family(font &n, font &b, font &i, font &bi) : normal(&n), bold(&b), italic(&i), bold_italic(&bi) {
			}

			const font *normal = nullptr, *bold = nullptr, *italic = nullptr, *bold_italic = nullptr;

			double maximum_width() const {
				return std::max({normal->max_width(), bold->max_width(), italic->max_width(), bold_italic->max_width()});
			}
			double maximum_height() const {
				return std::max({normal->height(), bold->height(), italic->height(), bold_italic->height()});
			}
			double common_baseline() const {
				return std::max({normal->baseline(), bold->baseline(), italic->baseline(), bold_italic->baseline()});
			}
			baseline_info get_baseline_info() const {
				double bl = common_baseline();
				return baseline_info(
					bl - normal->baseline(), bl - bold->baseline(), bl - italic->baseline(), bl - bold_italic->baseline()
				);
			}
			const font *get_by_style(font_style fs) const {
				switch (fs) {
				case font_style::normal:
					return normal;
				case font_style::bold:
					return bold;
				case font_style::italic:
					return italic;
				case font_style::bold_italic:
					return bold_italic;
				}
				assert_true_usage(false, "invalid font style encountered");
				return nullptr;
			}
		};
	}
}
