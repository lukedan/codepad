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
#include <pango/pangocairo.h>

#include "../core/logging.h"
#include "../core/assert.h"
#include "../core/misc.h"
#include "renderer.h"

namespace codepad::ui {
	namespace _details {
		/// Checks the given Freetype return value.
		inline void ft_check(FT_Error err) {
			if (err != FT_Err_Ok) {
				// TODO for some reason FT_Error_String is not defined on Ubuntu 18.04
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
				if constexpr (std::is_same_v<T, cairo_t>) { // cairo
					cairo_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_font_face_t>) {
					cairo_font_face_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, PangoAttrList>) { // pango
					pango_attr_list_ref(this->_handle);
				} else if constexpr (std::is_same_v<T, FcPattern>) { // fontconfig
					FcPatternReference(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_buffer_t>) { // harfbuzz
					hb_buffer_reference(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) <<
						"add ref operation not implemented for " << demangle(typeid(T).name());
					std::abort();
				}
			}
			/// Removes a reference to the handle if necessary.
			void _do_release() {
				if constexpr (std::is_same_v<T, cairo_t>) { // cairo
					cairo_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_font_face_t>) {
					cairo_font_face_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, PangoAttrList>) { // pango
					pango_attr_list_unref(this->_handle);
				} else if constexpr (std::is_same_v<T, FcPattern>) { // fontconfig
					FcPatternDestroy(this->_handle);
				} else if constexpr (std::is_same_v<T, hb_buffer_t>) { // harfbuzz
					hb_buffer_destroy(this->_handle);
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
		class font;
		class font_family;
		class plain_text;
		class text_context;


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

		/// Wraps around a \p PangoLayout.
		///
		/// \todo \n characters are not shown properly.
		class formatted_text : public ui::formatted_text {
			friend text_context;
		public:
			/// Initializes \ref _layout_size and \ref _valign.
			formatted_text(vec2d size, vertical_text_alignment valign) : _layout_size(size), _valign(valign) {
			}

			/// Returns the layout of the text.
			rectd get_layout() const override;
			/// Returns the metrics of each line.
			std::vector<line_metrics> get_line_metrics() const override;

			/// Returns the length of \ref _bytepos minus 1.
			std::size_t get_num_characters() const override {
				return _bytepos.size() - 1;
			}

			/// Invokes \p pango_layout_xy_to_index().
			caret_hit_test_result hit_test(vec2d) const override;
			/// Uses \p pango_layout_line_x_to_index() to perform hit testing at the given line.
			caret_hit_test_result hit_test_at_line(std::size_t, double) const override;
			/// Invokes \p pango_layout_index_to_pos().
			rectd get_character_placement(std::size_t) const override;
			/// For each line in the text, calls \p pango_layout_line_get_x_ranges() to compute the range of the
			/// selection.
			std::vector<rectd> get_character_range_placement(std::size_t beg, std::size_t len) const override;

			/// Returns \ref _layout_size.
			vec2d get_layout_size() const override {
				return _layout_size;
			}
			/// Sets \ref _layout_size.
			void set_layout_size(vec2d size) override {
				_layout_size = size;
			}

			/// Sets the color of the specified range of text.
			void set_text_color(colord, std::size_t beg, std::size_t len) override;
			/// Sets the font family of the specified range of text.
			void set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) override;
			/// Sets the font size of the specified range of text.
			void set_font_size(double, std::size_t beg, std::size_t len) override;
			/// Sets the font style of the specified range of text.
			void set_font_style(font_style, std::size_t beg, std::size_t len) override;
			/// Sets the font weight of the specified range of text.
			void set_font_weight(font_weight, std::size_t beg, std::size_t len) override;
			/// Sets the font stretch of the specified range of text.
			void set_font_stretch(font_stretch, std::size_t beg, std::size_t len) override;


			/// Returns \ref _layout.
			[[nodiscard]] PangoLayout *get_pango_layout() const {
				return _layout.get();
			}
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

			/// Returns the offset of the text inside the layout rectangle.
			[[nodiscard]] vec2d _get_offset() const;

			/// Converts character indices to byte positions.
			[[nodiscard]] std::pair<guint, guint> _char_to_byte(std::size_t beg, std::size_t len) const;
			[[nodiscard]] std::size_t _byte_to_char(std::size_t) const;
		};

		/// A freetype font.
		class font : public ui::font {
			friend text_context;
			friend font_family;
			friend plain_text;
		public:
			/// Initializes \ref _face directly.
			explicit font(_details::freetype_face_ref f) : _face(std::move(f)) {
			}

			/// Returns the ascender of the font.
			[[nodiscard]] double get_ascent_em() const override {
				// FIXME these fields are only relevant for scalable font formats
				return _into_em(static_cast<double>(_face->ascender));
			}
			/// Returns the distance between consecutive baselines.
			[[nodiscard]] double get_line_height_em() const override {
				// FIXME these fields are only relevant for scalable font formats
				return _into_em(static_cast<double>(_face->ascender));
			}

			/// If \p FT_Get_Char_Index() returns 0, then the character does not have a corresponding glyph in this font.
			[[nodiscard]] bool has_character(codepoint cp) const override {
				return FT_Get_Char_Index(_face.get(), static_cast<FT_ULong>(cp)) != 0;
			}

			/// Loads the corresponding glyph, and returns its horizontal advance.
			[[nodiscard]] double get_character_width_em(codepoint cp) const override {
				_details::ft_check(FT_Load_Char(
					_face.get(), static_cast<FT_ULong>(cp),
					FT_LOAD_NO_SCALE | FT_LOAD_IGNORE_TRANSFORM | FT_LOAD_LINEAR_DESIGN
				));
				return _into_em(static_cast<double>(_face->glyph->linearHoriAdvance));
			}
		protected:
			_details::freetype_face_ref _face; ///< The Freetype font face.

			/// Converts lengths from font design units into EM units. Since the default DPI on windows and ubuntu is 96,
			/// here we also scale the length accordingly.
			[[nodiscard]] double _into_em(double len) const {
				return len * 96.0 / (72.0 * static_cast<double>(_face->units_per_EM));
			}
		};

		// TODO binary editor performance is completely fucked because harfbuzz fonts are created on demand, which causes
		//      caches to be rebuilt every time i guess
		/// Holds a Fontconfig pattern.
		class font_family : public ui::font_family {
			friend text_context;
		public:
			/// Initializes all fields of this struct.
			font_family(text_context &ctx, _details::gtk_object_ref<FcPattern> patt) :
				_ctx(ctx), _pattern(std::move(patt)) {
			}

			/// Returns a font in this family matching the given description.
			[[nodiscard]] std::shared_ptr<ui::font> get_matching_font(
				font_style, font_weight, font_stretch
			) const override;
		protected:
			text_context &_ctx; ///< The \ref text_context that created this \ref font_family.
			_details::gtk_object_ref<FcPattern> _pattern; ///< The Fontconfig pattern.
		};

		/// Holds a \p hb_buffer_t.
		class plain_text : public ui::plain_text {
			friend text_context;
		public:
			/// Directly initializes \ref _buffer.
			plain_text(
				_details::gtk_object_ref<hb_buffer_t> buf, const font &fnt,
				const FT_Size_Metrics &size_info, std::size_t nchars, double font_size
			) : _buffer(std::move(buf)), _font(fnt._face), _num_characters(nchars), _font_size(font_size) {
				_x_scale = size_info.x_scale / 64.0;
				_ascender = size_info.ascender / 64.0;
				_height = fnt._into_em(_font->height) * font_size;
			}

			/// Returns the total width of this text clip.
			[[nodiscard]] double get_width() const override {
				_maybe_calculate_block_map();
				return _cached_block_positions.back();
			}

			/// Retrieves information about the character that is below the given horizontal position.
			[[nodiscard]] caret_hit_test_result hit_test(double x) const override;
			/// Returns the space occupied by the character at the given position.
			[[nodiscard]] rectd get_character_placement(std::size_t i) const override;


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
		class text_context {
			friend font_family;
		public:
			/// Initializes Fontconfig, Pango, and Freetype.
			text_context() {
				fontconfig_usage::maybe_initialize();

				_details::ft_check(FT_Init_FreeType(&_freetype));
				_pango_context.set_give(pango_font_map_create_context(pango_cairo_font_map_get_default()));
			}
			/// Calls \p cairo_debug_reset_static_data() to clean up.
			~text_context() {
				deinitialize();
			}


			/// De-initializes Pango and Freetype.
			void deinitialize() {
				if (_pango_context) {
					// although this will replace the font map with a new instance, it will still hopefully free
					// resources the old one's holding on to. without this pango would still be using some fonts which
					// will cause the cairo check to fail
					pango_cairo_font_map_set_default(nullptr);
					_pango_context.reset();

					_details::ft_check(FT_Done_FreeType(_freetype));
				}
			}


			/// Creates a new \ref text_format.
			[[nodiscard]] std::shared_ptr<ui::font_family> find_font_family(const std::u8string &family) {
				auto pattern = ui::_details::make_gtk_object_ref_give(FcPatternCreate());
				FcPatternAddString(pattern.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family.c_str()));
				return std::make_shared<pango_harfbuzz::font_family>(*this, std::move(pattern));
			}

			/// Creates a new \ref formatted_text object.
			[[nodiscard]] std::shared_ptr<formatted_text> create_formatted_text(
				std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
				horizontal_text_alignment halign, vertical_text_alignment valign
			) {
				return _create_formatted_text_impl(text, font, c, size, wrap, halign, valign);
			}
			/// Converts the text to UTF-8, then invokes \ref create_formatted_text().
			[[nodiscard]] std::shared_ptr<formatted_text> create_formatted_text(
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
			[[nodiscard]] std::shared_ptr<plain_text> create_plain_text(
				std::u8string_view text, ui::font &generic_fnt, double font_size
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
			[[nodiscard]] std::shared_ptr<plain_text> create_plain_text(
				std::basic_string_view<codepoint> text, ui::font &generic_fnt, double font_size
			) {
				auto buf = ui::_details::make_gtk_object_ref_give(hb_buffer_create());
				for (std::size_t i = 0; i < text.size(); ++i) {
					hb_buffer_add(buf.get(), text[i], static_cast<unsigned int>(i));
				}
				return _create_plain_text_impl(std::move(buf), generic_fnt, font_size);
			}
			/// Fast path for plain text creation. Currently no difference from \ref create_plain_text().
			[[nodiscard]] std::shared_ptr<plain_text> create_plain_text_fast(
				std::basic_string_view<codepoint> text, ui::font &fnt, double size
			) {
				// TODO implement fast path
				return create_plain_text(text, fnt, size);
			}


			/// Returns \ref _pango_context.
			[[nodiscard]] PangoContext *get_pango_context() const {
				return _pango_context.get();
			}
		protected:
			_details::glib_object_ref<PangoContext> _pango_context; ///< The Pango context.
			FT_Library _freetype = nullptr; ///< The Freetype library.

			/// Creates a new \ref formatted_text object.
			[[nodiscard]] std::shared_ptr<formatted_text> _create_formatted_text_impl(
				std::u8string_view text, const font_parameters&, colord, vec2d size, wrapping_mode,
				horizontal_text_alignment, vertical_text_alignment
			);
			/// Creates a new \ref plain_text from the given \p hb_buffer_t.
			[[nodiscard]] std::shared_ptr<plain_text> _create_plain_text_impl(
				_details::gtk_object_ref<hb_buffer_t>, ui::font&, double
			);
		};


		namespace _details {
			/// Casts a \ref ui::formatted_text to a \ref formatted_text.
			[[nodiscard]] inline const formatted_text &cast_formatted_text(const ui::formatted_text &t) {
				auto *rt = dynamic_cast<const formatted_text*>(&t);
				assert_true_usage(rt, "invalid formatted text type");
				return *rt;
			}

			/// Casts a \ref ui::plain_text to a \ref plain_text.
			[[nodiscard]] inline const plain_text &cast_plain_text(const ui::plain_text &t) {
				auto *rt = dynamic_cast<const plain_text*>(&t);
				assert_true_usage(rt, "invalid plain text type");
				return *rt;
			}
		}
	}
}
