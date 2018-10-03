#pragma once

/// \file
/// Generic font related enums and classes.

#include <map>
#include <unordered_map>
#include <variant>
#include <optional>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "../core/misc.h"
#include "renderer.h"

namespace codepad {
	/// The style of a font's characters.
	enum class font_style {
		normal = 0, ///< Normal.
		bold = 1, ///< Bold.
		italic = 2, ///< Italic.
		bold_italic = bold | italic ///< Bold and italic.
	};
	namespace os {
		template <typename Prim, typename Bkup> class backed_up_font;
		/// The base class for all font classes that declares all basic functionalities a font should provide.
		/// Aside from these functions, all fonts should provide a constructor that accepts a \ref str_t,
		/// a \p double, and a \ref font_style, representing the font's name, size, and style, respectively.
		class font {
			template <typename Prim, typename Bkup> friend class backed_up_font;
		public:
			/// Represents a character of the font.
			struct entry {
				/// The placement of the texture, with respect to the `pen', when the character is rendered.
				rectd placement;
				/// The distance that the `pen' should be moved forward to render the next character.
				double advance = 0.0;
				/// The texture of the character.
				os::char_texture texture;
			};

			/// Default constructor.
			font() = default;
			/// No move construction.
			font(font&&) = delete;
			/// No copy construction.
			font(const font&) = delete;
			/// No move assignment.
			font &operator=(font&&) = delete;
			/// No copy assignment.
			font &operator=(const font&) = delete;
			/// Default destructor.
			virtual ~font() = default;

			/// Returns whether this font has a valid character entry for the given codepoint.
			virtual bool has_valid_char_entry(codepoint) const = 0;
			/// Returns the font entry corresponding to the given codepoint.
			virtual const entry &get_char_entry(codepoint c) const {
				bool dummy;
				return _get_modify_char_entry(c, dummy);
			}

			/// Renders a character with the default renderer with the given color. This function should be preferred
			/// over \ref renderer_base::draw_character_custom since this can make use of additional information and
			/// assumptions to yield better-looking results. However, implementations of this function generally use
			/// \ref renderer_base::draw_character_custom as a backend.
			virtual entry &draw_character(codepoint, vec2d, colord) const = 0;

			/// Returns the width of the widest character of the given string.
			double get_max_width_charset(const std::u32string &s) const {
				double maxw = 0.0;
				for (codepoint c : s) {
					maxw = std::max(maxw, get_char_entry(c).advance);
				}
				return maxw;
			}

			/// Returns the height of a line for this font.
			virtual double height() const = 0;
			/// Returns the maximum width of a character of this font.
			virtual double max_width() const = 0;
			/// Returns the distance from the top of a line to the baseline.
			virtual double baseline() const = 0;
			/// Returns the kerning between the two given characters.
			virtual vec2d get_kerning(codepoint, codepoint) const = 0;
		protected:
			/// Returns a non-const reference to the \ref entry of the given character.
			///
			/// \param c The character.
			/// \param isnew Returns whether the entry has just been recorded.
			virtual entry &_get_modify_char_entry(codepoint c, bool &isnew) const = 0;
		};

		/// The base of freetype-based fonts for all platforms.
		///
		/// \remark Derived classes should call \ref _cache_kerning() at the end of their constructors,
		///         and <tt>_ft_verify(FT_Done_Face(_face));</tt> in their destructors.
		/// \todo Subpixel positioning using FT_Outline_Transform or such.
		class freetype_font_base : public font {
		public:
			/// The number of additional variants with subpixel positioning that will be considered when a character
			/// is being rendered.
			constexpr static size_t subpixel_character_variants = 3;

			/// Calls \p FT_Get_Char_Index and checks if there is a glyph corresponding to the given codepoint.
			bool has_valid_char_entry(codepoint c) const override {
				return FT_Get_Char_Index(_face, static_cast<FT_ULong>(c)) != 0;
			}
			/// Picks the appropriate variant of \ref _full_entry::variants or \ref font::entry::texture for
			/// rendering the character.
			entry &draw_character(codepoint c, vec2d pos, colord color) const override {
				bool dummy;
				_full_entry &et = _get_modify_full_char_entry(c, dummy);
				pos.y = std::round(pos.y);
				pos.x += 0.5 / (subpixel_character_variants + 1);
				double rx = std::floor(pos.x);
				size_t variant = std::min(
					static_cast<size_t>((pos.x - rx) * (subpixel_character_variants + 1)),
					subpixel_character_variants
				);
				const os::char_texture &ctex = variant == 0 ? et.original.texture : et.variants[variant - 1];
				rectd place = variant == 0 ? et.original.placement : et.variant_placement[variant - 1];
				os::renderer_base::get().draw_character_custom(ctex, place.translated(vec2d(rx, pos.y)), color);
				return et.original;
			}

			/// Returns the height of a line.
			double height() const override {
				return static_cast<double>(_face->size->metrics.height) * _ft_fixed_scale;
			}
			/// Returns the maximum advance of a character.
			double max_width() const override {
				return static_cast<double>(_face->size->metrics.max_advance) * _ft_fixed_scale;
			}
			/// Returns position of the baseline.
			double baseline() const override {
				return static_cast<double>(_face->size->metrics.ascender) * _ft_fixed_scale;
			}
			/// Returns the kerning between the two given characters.
			vec2d get_kerning(codepoint left, codepoint right) const override {
				vec2d res;
				if (!_kerncache.find_freq({left, right}, res)) { // not in small cache
					if (!_kerncache.find_infreq({left, right}, res)) { // not in big cache too
						res = _get_kerning_impl(left, right); // get the value
						_kerncache.set_infreq({left, right}, res);
					}
				}
				return res;
			}
		protected:
			/// Used to convert freetype coordinates into pixels.
			constexpr static double _ft_fixed_scale = 1.0 / 64.0;

			/// Caches the kerning between pairs of characters. The cache is divided into two parts:
			/// the small cache, a 2D array, and the large cache, a hash table.
			struct _kerning_pair_cache {
				/// Specifies the size of the small cache. The kerning between any two
				/// characters whose codepoint are less than this value are cached.
				constexpr static size_t fast_size = 128;

				/// Used to calculate the hash of a pair of characters.
				struct char_pair_hash {
				public:
					/// Returns the hash value. Uses the function provided at
					/// https://stackoverflow.com/questions/682438/hash-function-providing-unique-uint-from-an-integer-coordinate-pair.
					size_t operator()(const std::pair<codepoint, codepoint> &x) const {
						return (static_cast<size_t>(x.first) * 0x1F1F1F1F) ^ static_cast<size_t>(x.second);
					}
				};

				/// Finds the kerning of the given pair of characters in the small cache.
				///
				/// \param p The pair of characters.
				/// \param res Holds the kerning between the given characters if it's in the small cache.
				/// \return \p true if the kerning of the given character pair is in the small cache.
				bool find_freq(const std::pair<codepoint, codepoint> &p, vec2d &res) {
					if (p.first < fast_size && p.second < fast_size) {
						res = small_cache[p.first][p.second];
						return true;
					}
					return false;
				}
				/// Finds the kerning of the given pair of characters in the large cache.
				///
				/// \param p The pair of characters.
				/// \param res Holds the kerning between the given characters if it's in the large cache.
				/// \return \p true if the kerning of the given character pair is in the large cache.
				bool find_infreq(const std::pair<codepoint, codepoint> &p, vec2d &res) {
					auto found = _kerncache.find(p);
					if (found != _kerncache.end()) {
						res = found->second;
						return true;
					}
					return false;
				}

				/// Adds an entry to the large cache.
				void set_infreq(const std::pair<codepoint, codepoint> &p, const vec2d &v) {
					_kerncache.emplace(p, v);
				}

				/// The small cache. Filled by the font before any usage occurs.
				vec2d small_cache[fast_size][fast_size];
				/// The large cache.
				std::unordered_map<std::pair<codepoint, codepoint>, vec2d, char_pair_hash> _kerncache;
			};

			/// Extends \ref font::entry to include additional variants with subpixel positioning.
			struct _full_entry {
				font::entry original; ///< The original entry.
				/// Textures of all variants with subpixel positioning.
				os::char_texture variants[subpixel_character_variants];
				/// Placement of all variants of textures.
				rectd variant_placement[subpixel_character_variants];
			};

			/// Stores all font entries. Entries whose codepoints are less than \ref fast_size are
			/// stored in an array, while others are stored in a hash table.
			struct _entry_table {
				/// The number of entries in the array.
				constexpr static size_t fast_size = 128;

				/// Obtains the \ref font::entry corresponding to the given codepoint.
				/// If no entry is previously stored in the slot, a new one is created.
				///
				/// \param v The codepoint.
				/// \param valid Returns whether an entry has been in the slot.
				_full_entry &get(codepoint v, bool &valid) {
					if (v < fast_size) {
						std::optional<_full_entry> &er = array[v];
						valid = er.has_value();
						if (!valid) {
							er.emplace();
						}
						return *er;
					}
					auto found = map.find(v);
					valid = found != map.end();
					if (!valid) {
						return map[v];
					}
					return found->second;
				}

				/// Entries with codepoints smaller than \ref fast_size.
				std::optional<_full_entry> array[fast_size];
				/// Entries with codepoints larger than \ref fast_size.
				std::unordered_map<codepoint, _full_entry> map;
			};

			/// Creates a \ref os::char_texture from the given \p FT_Bitmap.
			///
			/// \todo Better integrate subpixel antialiasing.
			os::char_texture _create_texture(const FT_Bitmap &src) const {
				std::vector<unsigned char> v(4 * src.width * src.rows);
				size_t
					stride = static_cast<size_t>(std::abs(src.pitch)),
					width = static_cast<size_t>(src.width), height = static_cast<size_t>(src.rows);
				switch (src.pixel_mode) {
				case FT_PIXEL_MODE_MONO:
					_copy_image<FT_PIXEL_MODE_MONO>(src.buffer, v.data(), width, height, stride);
					break;
				case FT_PIXEL_MODE_GRAY:
					_copy_image<FT_PIXEL_MODE_GRAY>(src.buffer, v.data(), width, height, stride);
					break;
				case FT_PIXEL_MODE_LCD:
					width /= 3;
					_copy_image<FT_PIXEL_MODE_LCD>(src.buffer, v.data(), width, height, stride);
					break;
				case FT_PIXEL_MODE_LCD_V: // TODO
					_copy_image<FT_PIXEL_MODE_LCD_V>(src.buffer, v.data(), width, height, stride);
					break;
				case FT_PIXEL_MODE_BGRA:
					_copy_image<FT_PIXEL_MODE_BGRA>(src.buffer, v.data(), width, height, stride);
					break;
				}
				if (src.pitch < 0) { // the image is upside down, flip it
					for (size_t y = 0, invy = height - 1; y < invy; ++y, --invy) {
						unsigned char *s1 = &v[y * width * 4], *s2 = &v[invy * width * 4];
						for (size_t x = 0; x < width; ++x, s1 += 4, s2 += 4) {
							std::swap(s1[0], s2[0]);
							std::swap(s1[1], s2[1]);
							std::swap(s1[2], s2[2]);
							std::swap(s1[3], s2[3]);
						}
					}
				}
				return os::renderer_base::get().new_character_texture(width, height, v.data());
			}
			/// Copies the image of the given pixel format stored in the buffer into the other buffer.
			///
			/// \tparam Mode The pixel format of the source buffer.
			/// \param src The source buffer.
			/// \param dst The destination buffer.
			/// \param w The width of the buffer.
			/// \param h The height of the buffer.
			/// \param stride The number of bytes in a row.
			template <FT_Pixel_Mode Mode> inline static void _copy_image(
				const unsigned char *src, unsigned char *dst, size_t w, size_t h, size_t stride
			) {
				// the compiler should be able to optimize this
				// even without all the hassle - but just in case
				for (size_t y = 0; y < h; ++y, src += stride) {
					for (size_t x = 0; x < w; ++x, dst += 4) {
						if constexpr (Mode == FT_PIXEL_MODE_MONO) {
							dst[0] = dst[1] = dst[2] = 255;
							dst[3] = ((src[x / 8] & (128 >> (x % 8))) != 0) ? 255 : 0;
						} else if constexpr (Mode == FT_PIXEL_MODE_GRAY) {
							dst[0] = dst[1] = dst[2] = 255;
							dst[3] = src[x];
						} else if constexpr (Mode == FT_PIXEL_MODE_LCD) {
							const unsigned char *xptr = src + x * 3;
							dst[0] = xptr[0];
							dst[1] = xptr[1];
							dst[2] = xptr[2];
							dst[3] = 255;
						} else if constexpr (Mode == FT_PIXEL_MODE_BGRA) {
							// also rid of pre-multiplied alpha
							const unsigned char *xptr = src + x * 4;
							dst[0] = static_cast<unsigned char>(std::min(255, (255 * xptr[2]) / xptr[3]));
							dst[1] = static_cast<unsigned char>(std::min(255, (255 * xptr[1]) / xptr[3]));
							dst[2] = static_cast<unsigned char>(std::min(255, (255 * xptr[0]) / xptr[3]));
							dst[3] = xptr[3];
						}
					}
				}
			}

			/// Obtains the kerning between the two characters directly from freetype, bypassing the cache. This is
			/// also used when initializing the cache.
			vec2d _get_kerning_impl(codepoint l, codepoint r) const {
				FT_Vector v;
				_ft_verify(FT_Get_Kerning(
					_face, FT_Get_Char_Index(_face, l), FT_Get_Char_Index(_face, r), FT_KERNING_UNFITTED, &v)
				);
				return vec2d(static_cast<double>(v.x), static_cast<double>(v.y)) * _ft_fixed_scale;
			}
			/// Fills the small table of the kerning cache. Should be called by
			/// all derived classes after they've finished their initialization.
			void _cache_kerning() {
				for (codepoint i = 0; i < _kerning_pair_cache::fast_size; ++i) {
					for (codepoint j = 0; j < _kerning_pair_cache::fast_size; ++j) {
						_kerncache.small_cache[i][j] = _get_kerning_impl(i, j);
					}
				}
			}
			/// Returns the \ref _full_entry, generating a new one if none exists.
			_full_entry &_get_modify_full_char_entry(codepoint c, bool &isnew) const {
				_full_entry &et = _ents.get(c, isnew);
				isnew = !isnew;
				if (isnew) {
					FT_Set_Transform(_face, nullptr, nullptr);
					FT_Int32 loadflags = FT_LOAD_DEFAULT | FT_LOAD_RENDER/* | FT_LOAD_TARGET_LCD*/;
					_ft_verify(FT_Load_Char(_face, c, loadflags));
					et.original.advance = static_cast<double>(_face->glyph->metrics.horiAdvance) * _ft_fixed_scale;
					double baseline = std::round(
						static_cast<double>(_face->size->metrics.ascender) * _ft_fixed_scale
					);
					et.original.texture = _create_texture(_face->glyph->bitmap);
					et.original.placement = rectd::from_xywh(
						static_cast<double>(_face->glyph->bitmap_left),
						baseline - static_cast<double>(_face->glyph->bitmap_top),
						static_cast<double>(et.original.texture.get_width()),
						static_cast<double>(et.original.texture.get_height())
					);
					for (size_t i = 0; i < subpixel_character_variants; ++i) {
						FT_Vector v;
						v.x = static_cast<FT_Pos>((i + 1) / (_ft_fixed_scale * (subpixel_character_variants + 1)));
						v.y = 0;
						FT_Set_Transform(_face, nullptr, &v);
						_ft_verify(FT_Load_Char(_face, c, loadflags));
						et.variants[i] = _create_texture(_face->glyph->bitmap);
						et.variant_placement[i] = rectd::from_xywh(
							static_cast<double>(_face->glyph->bitmap_left),
							baseline - static_cast<double>(_face->glyph->bitmap_top),
							static_cast<double>(et.variants[i].get_width()),
							static_cast<double>(et.variants[i].get_height())
						);
					}
				}
				return et;
			}
			/// Returns \ref _full_entry::original of the entry returned by \ref _get_modify_full_char_entry.
			entry &_get_modify_char_entry(codepoint c, bool &isnew) const override {
				return _get_modify_full_char_entry(c, isnew).original;
			}

			mutable _kerning_pair_cache _kerncache; ///< The kerning cache.
			mutable _entry_table _ents; ///< The registry of character entries.
			FT_Face _face = nullptr; ///< The freetype font face.

			/// Checks the return code of freetype functions.
			inline static void _ft_verify(FT_Error code) {
#ifdef CP_CHECK_SYSTEM_ERRORS
				if (code != 0) {
					logger::get().log_error(CP_HERE, "FreeType error code ", code);
					assert_true_sys(false, "FreeType error");
				}
#endif
			}

			/// Manages the initialization and disposal of the freetype library.
			struct _library {
				/// Initializes the freetype library object.
				_library() {
					_ft_verify(FT_Init_FreeType(&lib));
				}
				/// Disposes the freetype library object.
				~_library() {
					_ft_verify(FT_Done_FreeType(lib));
				}

				FT_Library lib = nullptr; ///< The freetype library object.

				/// Returns the global \ref _library object.
				static _library &get();
			};
		};

		/// Font used to provide a larger character set. For every requested character,
		/// the primary font is searched, and if no corresponding entry is found, the
		/// entry of the back-up font is used.
		///
		/// \tparam Primary The type of the primary font.
		/// \tparam Backup The type of the back-up font.
		/// \todo Implement more generic multi-charset fonts.
		template <typename Primary, typename Backup> class backed_up_font : public font {
		public:
			/// Initializes the two fonts using the given font name, size, and style.
			backed_up_font(const str_t &str, double sz, font_style style) :
				_prim(str, sz, style), _bkup(CP_STRLIT(""), sz, style) {
				static_assert(std::is_base_of_v<font, Primary>, "Primary must be a font type");
				static_assert(std::is_base_of_v<font, Backup>, "Backup must be a font type");
			}

			/// Checks if there is an entry corresponding to the given codepoint in either font.
			bool has_valid_char_entry(codepoint c) const override {
				return _prim.has_valid_char_entry(c) || _bkup.has_valid_char_entry(c);
			}

			/// The height of the font, which is the maximum height of the two fonts.
			double height() const override {
				return std::max(_prim.height(), _bkup.height());
			}
			/// The maximum width of all characters of all two fonts.
			double max_width() const override {
				return std::max(_prim.max_width(), _bkup.max_width());
			}
			/// The baseline, whcih is the lower baseline (larger in value) of the two fonts.
			double baseline() const override {
				return std::max(_prim.baseline(), _bkup.baseline());
			}
			/// Returns the kerning between the two chars. If the corresponding entries of the two
			/// chars are of the same font, the corresponding kerning of that font is retrieved.
			/// Otherwise, the kerning is zero.
			vec2d get_kerning(codepoint left, codepoint right) const override {
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
			Primary _prim; ///< The primary font.
			Backup _bkup; ///< The back-up font.

			/// Returns the corresponding font entry, adjusting its placement if necessary.
			/// If both fonts have the entry, that of the primary font is used.
			entry &_get_modify_char_entry(codepoint c, bool &isnew) const override {
				if (_prim.has_valid_char_entry(c)) {
					auto &entry = static_cast<const font*>(&_prim)->_get_modify_char_entry(c, isnew);
					if (isnew && _bkup.baseline() > _prim.baseline()) {
						entry.placement = entry.placement.translated(
							vec2d(0.0, _bkup.baseline() - _prim.baseline())
						);
					}
					return entry;
				}
				auto &entry = static_cast<const font*>(&_bkup)->_get_modify_char_entry(c, isnew);
				if (isnew && _prim.baseline() > _bkup.baseline()) {
					entry.placement = entry.placement.translated(vec2d(0.0, _prim.baseline() - _bkup.baseline()));
				}
				return entry;
			}
		};
	}
}
