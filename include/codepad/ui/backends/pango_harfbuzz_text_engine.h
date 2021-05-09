// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Utility classes for text layout based on Pango and Harfbuzz.

#include <fontconfig/fontconfig.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <harfbuzz/hb.h>

#include <pango/pango.h>

#include "codepad/core/logging.h"
#include "codepad/core/assert.h"
#include "codepad/core/misc.h"
#include "codepad/ui/renderer.h"

namespace codepad::ui {
	namespace _details {
		/// Checks the given Freetype return value.
		inline void ft_check(FT_Error err) {
			if (err != FT_Err_Ok) {
				logger::get().log_error(CP_HERE) <<
					"Freetype error " << err << ": " << FT_Error_String(err);
				assert_true_sys(false, "Freetype error");
			}
		}

		/// Reference-counted handle of a GTK-related object.
		template <typename T> struct gtk_object_ref final :
			public reference_counted_handle<gtk_object_ref<T>, T*> {
			friend reference_counted_handle<gtk_object_ref<T>, T*>;
		public:
			constexpr static T *empty_handle = nullptr; ///< The empty handle.
		protected:
			/// Adds a reference to the handle if necessary.
			void _do_add_ref() {
				if constexpr (std::is_same_v<T, PangoAttrList>) { // pango
					pango_attr_list_ref(this->_handle);
				} else if constexpr (std::is_same_v<T, FcPattern>) { // fontconfig
					FcPatternReference(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_buffer_t>) { // harfbuzz
					hb_buffer_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_font_t>) {
					hb_font_reference(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) <<
						"add ref operation not implemented for " << demangle(typeid(T).name());
					std::abort();
				}
			}
			/// Removes a reference to the handle if necessary.
			void _do_release() {
				if constexpr (std::is_same_v<T, PangoAttrList>) { // pango
					pango_attr_list_unref(this->_handle);
				} else if constexpr (std::is_same_v<T, FcPattern>) { // fontconfig
					FcPatternDestroy(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_buffer_t>) { // harfbuzz
					hb_buffer_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_font_t>) {
					hb_font_destroy(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) <<
						"release operation for not implemented for " << demangle(typeid(T).name());
					std::abort();
				}
			}
		};
		/// Creates a new \ref gtk_object_ref, and calls \ref gtk_object_ref::set_share() to share the given
		/// pointer.
		template <typename T> inline gtk_object_ref<T> make_gtk_object_ref_share(T *ptr) {
			gtk_object_ref<T> res;
			res.set_share(ptr);
			return res;
		}
		/// Creates a new \ref gtk_object_ref, and calls \ref gtk_object_ref::set_give() to give the given
		/// pointer to it.
		template <typename T> inline gtk_object_ref<T> make_gtk_object_ref_give(T *ptr) {
			gtk_object_ref<T> res;
			res.set_give(ptr);
			return res;
		}

		/// Reference-counted handle of a GLib object.
		template <typename T> struct glib_object_ref final :
			public reference_counted_handle<glib_object_ref<T>, T*> {
			friend reference_counted_handle<glib_object_ref<T>, T*>;
		public:
			constexpr static T *empty_handle = nullptr; ///< The empty handle.
		protected:
			/// Calls \p g_object_ref().
			void _do_add_ref() {
				g_object_ref(this->_handle);
			}
			/// Calls \p g_object_unref().
			void _do_release() {
				g_object_unref(this->_handle);
			}
		};
		/// Creates a new \ref glib_object_ref, and calls \ref glib_object_ref::set_share() to share the given
		/// pointer.
		template <typename T> inline glib_object_ref<T> make_glib_object_ref_share(T ptr) {
			glib_object_ref<T> res;
			res.set_share(ptr);
			return res;
		}
		/// Creates a new \ref glib_object_ref, and calls \ref glib_object_ref::set_give() to give the given
		/// pointer to it.
		template <typename T> inline glib_object_ref<T> make_glib_object_ref_give(T ptr) {
			glib_object_ref<T> res;
			res.set_give(ptr);
			return res;
		}

		/// Holds a \p FT_Face.
		struct freetype_face_ref final : public reference_counted_handle<freetype_face_ref, FT_Face> {
			friend reference_counted_handle<freetype_face_ref, FT_Face>;
		public:
			constexpr static FT_Face empty_handle = nullptr; ///< The empty handle.
		protected:
			/// Calls \p FT_Reference_Face().
			void _do_add_ref() {
				ft_check(FT_Reference_Face(this->_handle));
			}
			/// Calls \p FT_Done_Face().
			void _do_release() {
				ft_check(FT_Done_Face(this->_handle));
			}
		};
		/// Creates a new \ref freetype_face_ref, and calls \ref freetype_face_ref::set_share() to share the given
		/// pointer.
		inline freetype_face_ref make_freetype_face_ref_share(FT_Face face) {
			freetype_face_ref res;
			res.set_share(face);
			return res;
		}
		/// Creates a new \ref freetype_face_ref, and calls \ref freetype_face_ref::set_give() to give the pointer
		/// to it.
		inline freetype_face_ref make_freetype_face_ref_give(FT_Face face) {
			freetype_face_ref res;
			res.set_give(face);
			return res;
		}
	}

	namespace pango_harfbuzz {
		struct font_data;
		struct font_family_data;
		struct plain_text_data;
		class text_engine;


		namespace _details {
			using namespace ui::_details;

			/// Converts a \ref PangoStyle to a \ref font_style.
			[[nodiscard]] font_style cast_font_style_back(PangoStyle);
			/// Converts a \ref PangoWeight to a \ref font_weight.
			[[nodiscard]] font_weight cast_font_weight_back(PangoWeight);
			/// Converts a \ref PangoStretch to a \ref font_stretch.
			[[nodiscard]] font_stretch cast_font_stretch_back(PangoStretch);
		}


		/// Initializes and finalizes Fontconfig.
		class fontconfig_usage {
		public:
			/// Initializes Fontconfig if it hasn't been initialized. Deinitialization is handled automatically by a
			/// static variable.
			static void maybe_initialize();
		protected:
			/// Calls \p FcInit().
			fontconfig_usage() {
				// FIXME on windows, only fonts installed system-wide can be discovered
				//       fonts that are installed for one user cannot be found
				//       https://gitlab.freedesktop.org/fontconfig/fontconfig/-/issues/144
				assert_true_sys(FcInit(), "failed to initialize Fontconfig");
			}
			/// Calls \p FcFini().
			~fontconfig_usage() {
				FcFini();
			}
		};

		/// Result of a font lookup operation using Fontconfig.
		struct find_font_result {
		public:
			/// Initializes \ref _pattern.
			explicit find_font_result(_details::gtk_object_ref<FcPattern> patt) : _pattern(std::move(patt)) {
			}

			/// Returns the font file name. The returned string lives as long as the underlying \p FcPattern.
			[[nodiscard]] FcChar8 *get_font_file_path() const {
				FcChar8 *file_name = nullptr;
				assert_true_sys(
					FcPatternGetString(_pattern.get(), FC_FILE, 0, &file_name) == FcResultMatch,
					"failed to obtain font file name from Fontconfig"
				);
				return file_name;
			}
			/// Returns the index of the font in the font file.
			[[nodiscard]] int get_font_index() const {
				int font_index;
				FcPatternGetInteger(_pattern.get(), FC_INDEX, 0, &font_index);
				return font_index;
			}
		protected:
			_details::gtk_object_ref<FcPattern> _pattern; ///< The Fontconfig pattern.
		};

		/// Used as keys of caching entries.
		struct font_params {
			/// Default constructor.
			font_params() = default;
			/// Initializes all fields of the struct.
			font_params(font_style sty, font_weight w, font_stretch str) : style(sty), weight(w), stretch(str) {
			}

			font_style style = font_style::normal; ///< Font style.
			font_weight weight = font_weight::normal; ///< Font weight.
			font_stretch stretch = font_stretch::normal; ///< Font stretch.

			/// Support comparison.
			friend std::strong_ordering operator<=>(font_params, font_params) = default;
		};
		/// Hash function for \ref font_params.
		struct font_params_hash {
			/// The implementation.
			std::size_t operator()(const font_params &params) const {
				std::size_t res = std::hash<font_style>()(params.style);
				res = combine_hashes(res, std::hash<font_weight>()(params.weight));
				res = combine_hashes(res, std::hash<font_stretch>()(params.stretch));
				return res;
			}
		};

		/// Wraps around a \p PangoLayout.
		///
		/// \todo \n characters are not shown properly.
		struct formatted_text_data {
			friend text_engine;
		public:
			/// Initializes \ref _layout_size and \ref _valign.
			formatted_text_data(vec2d size, vertical_text_alignment valign) : _layout_size(size), _valign(valign) {
			}
			/// Default move construction.
			formatted_text_data(formatted_text_data&&) = default;
			/// No copy construction.
			formatted_text_data(const formatted_text_data&) = delete;
			/// Default move assignment.
			formatted_text_data &operator=(formatted_text_data&&) = default;
			/// No copy assignment.
			formatted_text_data &operator=(const formatted_text_data&) = delete;

			/// Returns the layout of the text.
			rectd get_layout() const;
			/// Returns the metrics of each line.
			std::vector<line_metrics> get_line_metrics() const;

			/// Returns the length of \ref _bytepos minus 1.
			std::size_t get_num_characters() const {
				return _bytepos.size() - 1;
			}

			/// Invokes \p pango_layout_xy_to_index().
			caret_hit_test_result hit_test(vec2d) const;
			/// Uses \p pango_layout_line_x_to_index() to perform hit testing at the given line.
			caret_hit_test_result hit_test_at_line(std::size_t, double) const;
			/// Invokes \p pango_layout_index_to_pos().
			rectd get_character_placement(std::size_t) const;
			/// For each line in the text, calls \p pango_layout_line_get_x_ranges() to compute the range of the
			/// selection.
			std::vector<rectd> get_character_range_placement(std::size_t beg, std::size_t len) const;

			/// Returns \ref _layout_size.
			vec2d get_layout_size() const {
				return _layout_size;
			}
			/// Sets \ref _layout_size, then updates the width of the layout if wrapping is enabled.
			void set_layout_size(vec2d);
			/// Returns the result of \p pango_layout_get_alignment().
			horizontal_text_alignment get_horizontal_alignment() const;
			/// Sets the horizontal text alignment using \p pango_layout_set_alignment().
			void set_horizontal_alignment(horizontal_text_alignment);
			/// Returns \ref _valign.
			vertical_text_alignment get_vertical_alignment() const {
				return _valign;
			}
			/// Sets \ref _valign.
			void set_vertical_alignment(vertical_text_alignment align) {
				_valign = align;
			}
			/// Checks if a width is currently set using \p pango_layout_get_width(), and returns the corresponding
			/// mode.
			wrapping_mode get_wrapping_mode() const;
			/// Sets the wrapping mode of this text using \p pango_layout_set_width() and \p pango_layout_set_wrap().
			void set_wrapping_mode(wrapping_mode);

			/// Sets the color of the specified range of text.
			void set_text_color(colord, std::size_t beg, std::size_t len);
			/// Sets the font family of the specified range of text.
			void set_font_family(const std::u8string &family, std::size_t beg, std::size_t len);
			/// Sets the font size of the specified range of text.
			void set_font_size(double, std::size_t beg, std::size_t len);
			/// Sets the font style of the specified range of text.
			void set_font_style(font_style, std::size_t beg, std::size_t len);
			/// Sets the font weight of the specified range of text.
			void set_font_weight(font_weight, std::size_t beg, std::size_t len);
			/// Sets the font stretch of the specified range of text.
			void set_font_stretch(font_stretch, std::size_t beg, std::size_t len);


			/// Returns \ref _layout.
			[[nodiscard]] PangoLayout *get_pango_layout() const {
				return _layout.get();
			}
			/// Returns the offset of the text inside the layout rectangle.
			[[nodiscard]] vec2d get_alignment_offset() const;
		protected:
			/// Contains length information about a single line.
			struct _line_position {
				/// Default constructor.
				_line_position() = default;
				/// Initializes all fields.
				_line_position(std::size_t before, std::size_t after) :
					end_pos_before_break(before), end_pos_after_break(after) {
				}

				std::size_t
					/// The position after the last character on this line before the line break.
					end_pos_before_break = 0,
					/// The position after the last character on this line after the line break.
					end_pos_after_break = 0;
			};

			/// Positions of each character's starting byte. This includes one extra element at the end equal to the
			/// total byte length of the text.
			std::vector<std::size_t> _bytepos;
			std::vector<_line_position> _line_positions;
			vec2d _layout_size; ///< The size of the virtual layout box.
			_details::glib_object_ref<PangoLayout> _layout; ///< The underlying \p PangoLayout object.
			vertical_text_alignment _valign = vertical_text_alignment::center; ///< Vertical text alignment.

			/// Converts character indices to byte positions.
			[[nodiscard]] std::pair<guint, guint> _char_to_byte(std::size_t beg, std::size_t len) const;
			[[nodiscard]] std::size_t _byte_to_char(std::size_t) const;

			/// Similar to \ref get_alignment_offset(), but only for the horizontal part.
			[[nodiscard]] double _get_horizontal_alignment_offset() const;
		};

		/// Stores a Freetype font and a Harfbuzz font.
		struct font_data {
			friend text_engine;
			friend font_family_data;
			friend plain_text_data;
		public:
			/// Default constructor.
			font_data() = default;
			/// Initializes \ref _face directly.
			explicit font_data(_details::freetype_face_ref f) : _face(std::move(f)) {
			}
			/// Default move construction.
			font_data(font_data&&) = default;
			/// No copy construction.
			font_data(const font_data&) = delete;
			/// Default move assignment.
			font_data &operator=(font_data&&) = default;
			/// No copy assignment.
			font_data &operator=(const font_data&) = delete;

			/// Returns the ascender of the font.
			[[nodiscard]] double get_ascent_em() const {
				// FIXME these fields are only relevant for scalable font formats
				return _into_em(static_cast<double>(_face->ascender));
			}
			/// Returns the distance between consecutive baselines.
			[[nodiscard]] double get_line_height_em() const {
				// FIXME these fields are only relevant for scalable font formats
				return _into_em(static_cast<double>(_face->ascender));
			}

			/// If \p FT_Get_Char_Index() returns 0, then the character does not have a corresponding glyph in this font.
			[[nodiscard]] bool has_character(codepoint cp) const {
				return FT_Get_Char_Index(_face.get(), static_cast<FT_ULong>(cp)) != 0;
			}

			/// Loads the corresponding glyph, and returns its horizontal advance.
			[[nodiscard]] double get_character_width_em(codepoint cp) const {
				_details::ft_check(FT_Load_Char(
					_face.get(), static_cast<FT_ULong>(cp),
					FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_TRANSFORM | FT_LOAD_LINEAR_DESIGN
				));
				return _into_em(static_cast<double>(_face->glyph->linearHoriAdvance));
			}
		protected:
			_details::freetype_face_ref _face; ///< The Freetype font face.
			_details::gtk_object_ref<hb_font_t> _harfbuzz_font; ///< The chached Harfbuzz font. This may be empty.

			/// Converts lengths from font design units into EM units. Since the default DPI on windows and ubuntu is 96,
			/// here we also scale the length accordingly.
			[[nodiscard]] double _into_em(double len) const {
				return len * 96.0 / (72.0 * static_cast<double>(_face->units_per_EM));
			}
		};

		/// Holds a Fontconfig pattern.
		struct font_family_data {
			friend text_engine;
		public:
			/// An entry in the font cache.
			struct cache_entry {
				/// Default constructor.
				cache_entry() = default;
				/// No copy & move construction.
				cache_entry(const cache_entry&) = delete;
				/// No copy & move assignment.
				cache_entry &operator=(const cache_entry&) = delete;

				/// Finds the font in this font family corresponding to the given \ref font_style, \ref font_weight,
				/// and \ref font_stretch.
				[[nodiscard]] find_font_result find_font(font_style, font_weight, font_stretch) const;

				/// The cached list of fonts.
				std::unordered_map<font_params, std::shared_ptr<ui::font>, font_params_hash> font_faces;
				/// A partially-filled \p FcPattern that can be used for searching for font faces.
				_details::gtk_object_ref<FcPattern> pattern;
			};


			/// Initializes all fields of this struct.
			explicit font_family_data(cache_entry &entry) : _cache_entry(&entry) {
			}

			/// Returns the associated \ref cache_entry.
			[[nodiscard]] cache_entry &get_cache_entry() const {
				return *_cache_entry;
			}
		protected:
			cache_entry *_cache_entry = nullptr; ///< The entry in \ref text_engine::_font_cache.
		};

		/// Holds a \p hb_buffer_t.
		struct plain_text_data {
			friend text_engine;
		public:
			/// Directly initializes \ref _buffer.
			plain_text_data(
				_details::gtk_object_ref<hb_buffer_t> buf, const font_data &fnt,
				const FT_Size_Metrics &size_info, std::size_t nchars, double font_size
			) : _buffer(std::move(buf)), _font(fnt._face), _num_characters(nchars), _font_size(font_size) {
				_x_scale = static_cast<double>(size_info.x_scale) / 64.0;
				_ascender = static_cast<double>(size_info.ascender) / 64.0;
				_height = fnt._into_em(_font->height) * font_size;
			}

			/// Returns the total width of this text clip.
			[[nodiscard]] double get_width() const {
				_maybe_calculate_block_map();
				return _cached_block_positions.back();
			}

			/// Retrieves information about the character that is below the given horizontal position.
			[[nodiscard]] caret_hit_test_result hit_test(double x) const;
			/// Returns the space occupied by the character at the given position.
			[[nodiscard]] rectd get_character_placement(std::size_t i) const;


			/// Returns \ref _buffer.
			[[nodiscard]] hb_buffer_t *get_buffer() const {
				return _buffer.get();
			}
			/// Returns \ref _font.
			[[nodiscard]] FT_Face get_font() const {
				return _font.get();
			}
			/// Returns \ref _font_size.
			[[nodiscard]] double get_font_size() const {
				return _font_size;
			}
			/// Returns \ref _ascender.
			[[nodiscard]] double get_ascender() const {
				return _ascender;
			}
		protected:
			/// Fills \ref _cached_first_char_of_block and \ref _cached_block_positions if necessary.
			void _maybe_calculate_block_map() const;

			/// Mapping from blocks to the index of the first character in every block. This array has one additional
			/// element at the end that is the total number of characters.
			mutable std::vector<std::size_t> _cached_first_char_of_block;
			/// The positions of the left borders of all blocks. A block is a consecutive set of glyphs that share the
			/// same cluster value. This array has one additional element at the end that is the total width of the
			/// text clip.
			mutable std::vector<double> _cached_block_positions;

			_details::gtk_object_ref<hb_buffer_t> _buffer; ///< The harfbuzz buffer.
			_details::freetype_face_ref _font; ///< The font.
			std::size_t _num_characters = 0; ///< The number of characters in this clip of text.
			double
				_font_size = 0.0, ///< Originally required font size.
				_x_scale = 0.0, ///< Used to convert horizontal width from font units into device-independent pixels.
				_ascender = 0.0, ///< Ascender in device-independent pixels.
				_height = 0.0; ///< Font height in device-independent pixels.

			/// Returns the width of a character at the specified block. This function assumes that
			/// \ref _cached_first_char_of_block and \ref _cached_block_positions has been calculated.
			[[nodiscard]] double _get_part_width(std::size_t block) const;
		};


		/// Context for text layout.
		class text_engine {
			friend font_family_data;
		public:
			/// Initializes Fontconfig, Pango, and Freetype, using the provided \p PangoFontMap.
			explicit text_engine(PangoFontMap *font_map) {
				fontconfig_usage::maybe_initialize();

				_details::ft_check(FT_Init_FreeType(&_freetype));
				_pango_context.set_give(pango_font_map_create_context(font_map));
			}
			/// Calls \ref deinitialize() to clean up if necessary.
			~text_engine() {
				deinitialize();
			}


			/// De-initializes Pango and Freetype.
			void deinitialize() {
				if (_pango_context) {
					_font_cache.clear();
					_pango_context.reset();
					_details::ft_check(FT_Done_FreeType(_freetype));
				}
			}


			/// Creates a new \ref font_family_data.
			[[nodiscard]] font_family_data find_font_family(const char8_t *family) {
				auto [it, inserted] = _font_cache.try_emplace(family);
				if (inserted) {
					it->second.pattern = _details::make_gtk_object_ref_give(FcPatternCreate());
					FcPatternAddString(
						it->second.pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family)
					);
				}
				return font_family_data(it->second);
			}
			/// Creates a \ref font_data object corresponding to the font at the given index in the given file.
			[[nodiscard]] font_data create_font_for_file(const char *file, int index) {
				FT_Face face;
				_details::ft_check(FT_New_Face(_freetype, file, index, &face));
				return font_data(_details::make_freetype_face_ref_give(face));
			}

			/// Creates a new \ref formatted_text object.
			[[nodiscard]] formatted_text_data create_formatted_text(
				std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
				horizontal_text_alignment halign, vertical_text_alignment valign
			) {
				return _create_formatted_text_impl(text, font, c, size, wrap, halign, valign);
			}
			/// Converts the text to UTF-8, then invokes \ref create_formatted_text().
			[[nodiscard]] formatted_text_data create_formatted_text(
				std::basic_string_view<codepoint> utf32,
				const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
				horizontal_text_alignment halign, vertical_text_alignment valign
			) {
				std::basic_string<std::byte> text;
				text.reserve(utf32.size());
				for (codepoint cp : utf32) {
					text += encodings::utf8::encode_codepoint(cp);
				}
				return create_formatted_text(
					std::u8string_view(reinterpret_cast<const char8_t*>(text.c_str()), text.size()),
					font, c, size, wrap, halign, valign
				);
			}

			/// Creates a new \ref plain_text object for the given text and font.
			[[nodiscard]] plain_text_data create_plain_text(
				std::u8string_view text, font_data &generic_fnt, double font_size
			) {
				auto buf = _details::make_gtk_object_ref_give(hb_buffer_create());
				codepoint replacement = hb_buffer_get_replacement_codepoint(buf.get());
				std::size_t index = 0;
				for (auto it = text.begin(); it != text.end(); ) {
					codepoint cp;
					if (!encodings::utf8::next_codepoint(it, text.end(), cp)) {
						cp = replacement;
					}
					hb_buffer_add(buf.get(), cp, static_cast<unsigned int>(index));
				}
				return _create_plain_text_impl(std::move(buf), generic_fnt, font_size);
			}
			/// \overload
			[[nodiscard]] plain_text_data create_plain_text(
				std::basic_string_view<codepoint> text, font_data &generic_fnt, double font_size
			) {
				auto buf = _details::make_gtk_object_ref_give(hb_buffer_create());
				for (std::size_t i = 0; i < text.size(); ++i) {
					hb_buffer_add(buf.get(), text[i], static_cast<unsigned int>(i));
				}
				return _create_plain_text_impl(std::move(buf), generic_fnt, font_size);
			}
			/// Fast path for plain text creation. Currently no difference from \ref create_plain_text().
			[[nodiscard]] plain_text_data create_plain_text_fast(
				std::basic_string_view<codepoint> text, font_data &fnt, double size
			) {
				// TODO implement fast path
				return create_plain_text(text, fnt, size);
			}


			/// Returns \ref _pango_context.
			[[nodiscard]] PangoContext *get_pango_context() const {
				return _pango_context.get();
			}
		protected:
			/// Cached font information.
			std::unordered_map<std::u8string, font_family_data::cache_entry> _font_cache;
			_details::glib_object_ref<PangoContext> _pango_context; ///< The Pango context.
			FT_Library _freetype = nullptr; ///< The Freetype library.

			/// Creates a new \ref formatted_text object.
			[[nodiscard]] formatted_text_data _create_formatted_text_impl(
				std::u8string_view text, const font_parameters&, colord, vec2d size, wrapping_mode,
				horizontal_text_alignment, vertical_text_alignment
			);
			/// Creates a new \ref plain_text from the given \p hb_buffer_t.
			[[nodiscard]] plain_text_data _create_plain_text_impl(
				_details::gtk_object_ref<hb_buffer_t>, font_data&, double
			);
		};
	}
}
