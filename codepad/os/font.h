#pragma once

#include <unordered_map>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../utilities/misc.h"
#include "renderer.h"

namespace codepad {
	enum class font_style {
		normal = 0,
		bold = 1,
		italic = 2,
		bold_italic = bold | italic
	};
	namespace os {
		class font {
		public:
			struct entry {
				rectd placement;
				double advance;
				os::texture texture;
			};

			font() = default;
			font(const font&) = delete;
			font(font&&) = delete;
			font &operator=(const font&) = delete;
			font &operator=(font&&) = delete;
			virtual ~font() = default;

			virtual const entry &get_char_entry(char32_t) const = 0;

			double get_max_width_charset(const str_t &s) const {
				double maxw = 0.0;
				for (auto i = s.begin(); i != s.end(); ++i) {
					maxw = std::max(maxw, get_char_entry(*i).advance);
				}
				return maxw;
			}

			virtual double height() const = 0;
			virtual double max_width() const = 0;
			virtual double baseline() const = 0;
			virtual vec2d get_kerning(char32_t, char32_t) const = 0;
		};

		class freetype_font_base : public font {
			friend struct codepad::globals;
		public:
			~freetype_font_base() override {
				for (auto i = _map.begin(); i != _map.end(); ++i) {
					os::renderer_base::get().delete_character_texture(i->second.texture);
				}
				_ft_verify(FT_Done_Face(_face));
				std::free(_data);
			}

			const entry &get_char_entry(char32_t c) const override {
				auto found = _map.find(c);
				if (found == _map.end()) {
					_ft_verify(FT_Load_Char(_face, c, FT_LOAD_DEFAULT | FT_LOAD_RENDER));
					const FT_Bitmap &bmpdata = _face->glyph->bitmap;
					entry &et = _map[c] = entry();
					et.texture = os::renderer_base::get().new_character_texture(bmpdata.width, bmpdata.rows, bmpdata.buffer);
					et.advance = static_cast<double>(_face->glyph->metrics.horiAdvance) * _ft_fixed_scale;
					et.placement = rectd::from_xywh(
						static_cast<double>(_face->glyph->metrics.horiBearingX) * _ft_fixed_scale,
						static_cast<double>(_face->size->metrics.ascender - _face->glyph->metrics.horiBearingY) * _ft_fixed_scale,
						bmpdata.width,
						bmpdata.rows
					);
					return et;
				}
				return found->second;
			}

			double height() const override {
				return static_cast<double>(_face->size->metrics.height) * _ft_fixed_scale;
			}
			double max_width() const override {
				return static_cast<double>(_face->size->metrics.max_advance) * _ft_fixed_scale;
			}
			double baseline() const override {
				return static_cast<double>(_face->size->metrics.ascender) * _ft_fixed_scale;
			}
			vec2d get_kerning(char32_t left, char32_t right) const override {
				FT_Vector v;
				_ft_verify(FT_Get_Kerning(_face, FT_Get_Char_Index(_face, left), FT_Get_Char_Index(_face, right), FT_KERNING_UNFITTED, &v));
				return vec2d(static_cast<double>(v.x), static_cast<double>(v.y)) * _ft_fixed_scale;
			}
		protected:
			constexpr static double _ft_fixed_scale = 1.0 / 64.0;

			FT_Face _face = nullptr;
			mutable std::unordered_map<char32_t, entry> _map;
			void *_data = nullptr;

			inline static void _ft_verify(FT_Error code) {
#ifdef CP_DETECT_SYSTEM_ERRORS
				if (code != 0) {
					logger::get().log_error(CP_HERE, "FreeType error code ", code);
					assert_true_sys(false, "FreeType error");
				}
#endif
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
			static _library &_get_library();
		};
	}
}
