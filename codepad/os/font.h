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
		template <typename Prim, typename Bkup> class backed_up_font;
		class font {
			template <typename Prim, typename Bkup> friend class backed_up_font;
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

			virtual bool has_valid_char_entry(char32_t) const = 0;
			virtual const entry &get_char_entry(char32_t c) const {
				return _get_modify_char_entry(c);
			}

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
		protected:
			virtual entry &_get_modify_char_entry(char32_t) const = 0;
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

			bool has_valid_char_entry(char32_t c) const override {
				return FT_Get_Char_Index(_face, static_cast<FT_ULong>(c)) != 0;
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
				auto cached = _kerncache.find(std::make_pair(left, right));
				if (cached != _kerncache.end()) {
					return cached->second;
				}
				FT_Vector v;
				_ft_verify(FT_Get_Kerning(_face, FT_Get_Char_Index(_face, left), FT_Get_Char_Index(_face, right), FT_KERNING_UNFITTED, &v));
				vec2d res(static_cast<double>(v.x), static_cast<double>(v.y));
				res *= _ft_fixed_scale;
				_kerncache.insert(std::make_pair(std::make_pair(left, right), res));
				return res;
			}
		protected:
			constexpr static double _ft_fixed_scale = 1.0 / 64.0;

			struct _kern_pair_hash {
			public:
				template <typename T, typename U> std::size_t operator()(const std::pair<T, U> &x) const {
					size_t lhs = std::hash<U>()(x.first), rhs = std::hash<U>()(x.second);
					return lhs ^ (rhs + 0x9e3779b9 + (lhs << 6) + (lhs >> 2));
				}
			};

			template <FT_Pixel_Mode V> struct _helper {
			};
			inline static void _copy_image(unsigned char *dst, const FT_Bitmap &src) {
				size_t stride = static_cast<size_t>(std::abs(src.pitch));
				switch (src.pixel_mode) {
				case FT_PIXEL_MODE_MONO:
					_copy_image<FT_PIXEL_MODE_MONO>(dst, src.buffer, src.width, src.rows, stride);
					break;
				case FT_PIXEL_MODE_GRAY:
					_copy_image<FT_PIXEL_MODE_GRAY>(dst, src.buffer, src.width, src.rows, stride);
					break;
				case FT_PIXEL_MODE_BGRA:
					_copy_image<FT_PIXEL_MODE_BGRA>(dst, src.buffer, src.width, src.rows, stride);
					break;
				}
			}
			template <FT_Pixel_Mode Mode> inline static void _copy_image(
				unsigned char *dst, const unsigned char *src, size_t w, size_t h, size_t stride
			) {
				for (size_t y = 0; y < h; ++y, src += stride) {
					for (size_t x = 0; x < w; ++x, dst += 4) {
						_copy_pixel(dst, src, x, _helper<Mode>());
					}
				}
			}
			inline static void _copy_pixel(
				unsigned char *dst, const unsigned char *srcrow, size_t x, _helper<FT_PIXEL_MODE_MONO>
			) {
				dst[0] = dst[1] = dst[2] = 255;
				dst[3] = ((srcrow[x / 8] & (128 >> (x % 8))) != 0) ? 255 : 0;
			}
			inline static void _copy_pixel(
				unsigned char *dst, const unsigned char *srcrow, size_t x, _helper<FT_PIXEL_MODE_GRAY>
			) {
				dst[0] = dst[1] = dst[2] = 255;
				dst[3] = srcrow[x];
			}
			inline static void _copy_pixel(
				unsigned char *dst, const unsigned char *srcrow, size_t x, _helper<FT_PIXEL_MODE_BGRA>
			) { // also rid of pre-multiplied alpha
				const unsigned char *xptr = srcrow + x * 4;
				dst[0] = static_cast<unsigned char>(std::min(255, (255 * xptr[2]) / xptr[3]));
				dst[1] = static_cast<unsigned char>(std::min(255, (255 * xptr[1]) / xptr[3]));
				dst[2] = static_cast<unsigned char>(std::min(255, (255 * xptr[0]) / xptr[3]));
				dst[3] = xptr[3];
			}

			entry &_get_modify_char_entry(char32_t c) const override {
				auto found = _map.find(c);
				if (found == _map.end()) {
					_ft_verify(FT_Load_Char(_face, c, FT_LOAD_DEFAULT | FT_LOAD_RENDER));
					const FT_Bitmap &bmpdata = _face->glyph->bitmap;
					entry &et = _map[c] = entry();
					{
						unsigned char *buf = static_cast<unsigned char*>(std::malloc(4 * bmpdata.width * bmpdata.rows));
						_copy_image(buf, bmpdata);
						if (bmpdata.pitch < 0) {
							for (size_t y = 0, invy = bmpdata.rows - 1; y < invy; ++y, --invy) {
								unsigned char *s1 = buf + y * bmpdata.width * 4, *s2 = buf + y * bmpdata.width * 4;
								for (size_t x = 0; x < bmpdata.width; ++x, s1 += 4, s2 += 4) {
									std::swap(s1[0], s2[0]);
									std::swap(s1[1], s2[1]);
									std::swap(s1[2], s2[2]);
									std::swap(s1[3], s2[3]);
								}
							}
						}
						et.texture = os::renderer_base::get().new_character_texture(bmpdata.width, bmpdata.rows, buf);
						std::free(buf);
					}
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

			FT_Face _face = nullptr;
			mutable std::unordered_map<char32_t, entry> _map;
			mutable std::unordered_map<std::pair<char32_t, char32_t>, vec2d, _kern_pair_hash> _kerncache;
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

		template <typename Primary, typename Backup> class backed_up_font : public font { // TODO excessive resource consumption
		public:
			backed_up_font(const str_t &str, double sz, font_style style) :
				_prim(str, sz, style), _bkup(U"", sz, style) {
				static_assert(std::is_base_of<font, Primary>::value, "Primary must be a font type");
				static_assert(std::is_base_of<font, Backup>::value, "Backup must be a font type");
			}

			virtual bool has_valid_char_entry(char32_t c) const override {
				return _prim.has_valid_char_entry(c) || _bkup.has_valid_char_entry(c);
			}

			virtual double height() const override {
				return std::max(_prim.height(), _bkup.height());
			}
			virtual double max_width() const override {
				return std::max(_prim.max_width(), _bkup.max_width());
			}
			virtual double baseline() const override {
				return std::max(_prim.baseline(), _bkup.baseline());
			}
			virtual vec2d get_kerning(char32_t left, char32_t right) const override {
				bool pl = _prim.has_valid_char_entry(left), pr = _prim.has_valid_char_entry(right);
				if (pl && pr) {
					return _prim.get_kerning(left, right);
				}
				if (pl || pr) {
					return vec2d();
				}
				return _bkup.get_kerning(left, right);
			}
		protected:
			Primary _prim;
			Backup _bkup;

			entry &_get_modify_char_entry(char32_t c) const override { // TODO align baselines
				if (_prim.has_valid_char_entry(c)) {
					return static_cast<const font*>(&_prim)->_get_modify_char_entry(c);
				}
				return static_cast<const font*>(&_bkup)->_get_modify_char_entry(c);
			}
		};
	}
}
