#pragma once

#include <string>
#include <map>
#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "textconfig.h"
#include "misc.h"
#include "../platform/interface.h"

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
		font(font &&movefrom) : _face(movefrom._face), _map(std::move(movefrom._map)) {
			movefrom._face = nullptr;
		}
		font &operator =(const font&) = delete;
		font &operator =(font&&) = delete;
		~font() {
			if (_face) {
				_ft_verify(FT_Done_Face(_face));
			}
		}

		const entry &get_char_entry(char_t c, platform::renderer_base &rend) {
			auto found = _map.find(c);
			if (found == _map.end()) {
				_ft_verify(FT_Load_Char(_face, c, FT_LOAD_DEFAULT | FT_LOAD_RENDER));
				const FT_Bitmap &bmpdata = _face->glyph->bitmap;
				entry &et = _map[c] = entry();
				et.texture = rend.new_texture_grayscale(bmpdata.width, bmpdata.rows, bmpdata.buffer);
				et.advance = _face->glyph->metrics.horiAdvance * _ft_fixed_scale;
				et.placement = rectd::from_xywh(
					_face->glyph->metrics.horiBearingX * _ft_fixed_scale,
					(_face->glyph->metrics.horiBearingY - _face->glyph->metrics.height - _face->size->metrics.ascender) * _ft_fixed_scale,
					bmpdata.width,
					bmpdata.rows
				);
				return et;
			}
			return found->second;
		}

		double get_height() const {
			return _face->size->metrics.height * _ft_fixed_scale;
		}
		vec2d get_kerning(char_t left, char_t right) const {
			FT_Vector v;
			// TODO strange behavior, different from doc
			_ft_verify(FT_Get_Kerning(_face, FT_Get_Char_Index(_face, left), FT_Get_Char_Index(_face, right), FT_KERNING_UNFITTED, &v));
			return vec2d(v.x, v.y) * _ft_fixed_scale;
		}
	protected:
		constexpr static double _ft_fixed_scale = 1.0 / 64.0;

		FT_Face _face = nullptr;
		std::map<char_t, entry> _map;

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
}
