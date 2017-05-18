#pragma once

#include <string>
#include <unordered_map>
#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "textconfig.h"
#include "misc.h"
#include "../platform/renderer.h"

namespace codepad {
	class font {
	public:
		struct entry {
			rectd placement;
			double advance;
			platform::texture_id texture;
		};

		font(const std::string &str, size_t sz) {
			_ft_verify(FT_New_Face(_lib.lib, str.c_str(), 0, &_face));
			_ft_verify(FT_Set_Pixel_Sizes(_face, 0, static_cast<FT_UInt>(sz)));
		}
		font(const font&) = delete;
		font(font&&) = delete;
		font &operator =(const font&) = delete;
		font &operator =(font&&) = delete;
		~font() {
			for (auto i = _map.begin(); i != _map.end(); ++i) {
				platform::renderer_base::get().delete_character_texture(i->second.texture);
			}
			_ft_verify(FT_Done_Face(_face));
		}

		const entry &get_char_entry(char_t c) const {
			auto found = _map.find(c);
			if (found == _map.end()) {
				_ft_verify(FT_Load_Char(_face, c, FT_LOAD_DEFAULT | FT_LOAD_RENDER));
				const FT_Bitmap &bmpdata = _face->glyph->bitmap;
				entry &et = _map[c] = entry();
				et.texture = platform::renderer_base::get().new_character_texture(bmpdata.width, bmpdata.rows, bmpdata.buffer);
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
			assert(code == 0);
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
		const font *normal = nullptr, *bold = nullptr, *italic = nullptr, *bold_italic = nullptr;

		double maximum_height() const {
			return std::max({ normal->height(), bold->height(), italic->height(), bold_italic->height() });
		}
	};
}
