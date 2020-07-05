// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains the base class of the Cairo renderer backend.

#ifdef CP_USE_CAIRO

#	include <stack>

#	include <cairo.h>

#	include <pango/pango.h>
#	include <pango/pangocairo.h>

#	include <harfbuzz/hb.h>

#	include <ft2build.h>
#	include FT_FREETYPE_H

#	include <fontconfig/fontconfig.h>

#	include "../core/math.h"
#	include "renderer.h"
#	include "window.h"

namespace codepad::ui::cairo {
	class font;
	class font_family;
	class renderer_base;
	class plain_text;

	namespace _details {
		/// Checks the given Freetype return value.
		inline void ft_check(FT_Error err) {
			if (err != FT_Err_Ok) {
				// TODO for some reason FT_Error_String is not defined on Ubuntu 18.04
				/*
				logger::get().log_error(CP_HERE) <<
					"Freetype error " << err << ": " << FT_Error_String(err);
				*/
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

	/// A Cairo surface used as a source.
	class bitmap : public ui::bitmap {
		friend renderer_base;
	public:
		/// Returns \ref _size.
		[[nodiscard]] vec2d get_size() const override {
			return _size;
		}
	protected:
		vec2d _size; ///< The logical size of this bitmap.
		_details::gtk_object_ref<cairo_surface_t> _surface; ///< The underlying Cairo surface.
	};

	/// A Cairo surface used as a render target.
	class render_target : public ui::render_target {
		friend renderer_base;
	protected:
		// we don't need to store the surface handle as we ca just call cairo_get_target()
		_details::gtk_object_ref<cairo_t> _context; ///< The cairo context.

		/// Returns the target surface.
		cairo_surface_t *_get_target() {
			return cairo_get_target(_context.get());
		}
	};

	/// Wraps around a \p PangoLayout.
	///
	/// \todo \n characters are not shown properly.
	class formatted_text : public ui::formatted_text {
		friend renderer_base;
	public:
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
		/// Invokes \p pango_layout_index_to_pos().
		rectd get_character_placement(std::size_t) const override;
		/// For each line in the text, calls \p pango_layout_line_get_x_ranges() to compute the range of the
		/// selection.
		std::vector<rectd> get_character_range_placement(std::size_t beg, std::size_t len) const override;

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
	protected:
		/// Positions of each character's starting byte. This includes one extra element at the end equal to the
		/// total byte length of the text. Moreover, this considers <tt>\r\n</tt> codepoints as a single character.
		std::vector<std::size_t> _bytepos;
		_details::glib_object_ref<PangoLayout> _layout; ///< The underlying \p PangoLayout object.

		/// Converts character indices to byte positions.
		std::pair<guint, guint> _char_to_byte(std::size_t beg, std::size_t len) const;
	};

	/// A freetype font.
	class font : public ui::font {
		friend renderer_base;
		friend font_family;
		friend plain_text;
	public:
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

		/// Initializes \ref _face directly.
		explicit font(_details::freetype_face_ref f) : _face(std::move(f)) {
		}

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
		friend renderer_base;
	public:
		/// Returns a font in this family matching the given description.
		[[nodiscard]] std::unique_ptr<ui::font> get_matching_font(
			font_style, font_weight, font_stretch
		) const override;
	protected:
		renderer_base &_renderer; ///< The \ref renderer_base that created this \ref font_family.
		_details::gtk_object_ref<FcPattern> _pattern; ///< The Fontconfig pattern.

		/// Initializes all fields of this struct.
		font_family(renderer_base &r, _details::gtk_object_ref<FcPattern> patt) :
			_renderer(r), _pattern(std::move(patt)) {
		}
	};

	/// Holds a \p hb_buffer_t.
	class plain_text : public ui::plain_text {
		friend renderer_base;
	public:
		/// Returns the total width of this text clip.
		[[nodiscard]] double get_width() const override {
			_maybe_calculate_block_map();
			return _cached_block_positions.back();
		}

		/// Retrieves information about the character that is below the given horizontal position.
		[[nodiscard]] caret_hit_test_result hit_test(double x) const override;
		/// Returns the space occupied by the character at the given position.
		[[nodiscard]] rectd get_character_placement(std::size_t i) const override;
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


		/// Directly initializes \ref _buffer.
		plain_text(
			_details::gtk_object_ref<hb_buffer_t> buf, const font &fnt,
			const FT_Size_Metrics &size_info, std::size_t nchars, double font_size
		) : _buffer(std::move(buf)), _font(fnt._face), _num_characters(nchars), _font_size(font_size) {
			_x_scale = size_info.x_scale / 64.0;
			_ascender = size_info.ascender / 64.0;
			_height = fnt._into_em(_font->height) * font_size;
		}

		/// Returns the width of a character at the specified block. This function assumes that
		/// \ref _cached_first_char_of_block and \ref _cached_block_positions has been calculated.
		[[nodiscard]] double _get_part_width(std::size_t block) const;
	};

	/// Allows for the user to build a path for a \p cairo_t.
	class path_geometry_builder : public ui::path_geometry_builder {
		friend renderer_base;
	public:
		/// Closes and ends the current sub-path.
		void close() override {
			cairo_close_path(_context);
		}
		/// Moves to the given position and starts a new sub-path.
		void move_to(vec2d pos) override {
			cairo_move_to(_context, pos.x, pos.y);
		}

		/// Adds a segment from the current position to the given position.
		void add_segment(vec2d to) override {
			cairo_line_to(_context, to.x, to.y);
		}
		/// Adds a cubic bezier segment.
		void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) override {
			cairo_curve_to(_context, control1.x, control1.y, control2.x, control2.y, to.x, to.y);
		}
		/// Adds an arc (part of a circle).
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction, ui::arc_type) override;
	protected:
		cairo_t *_context = nullptr; ///< The Cairo context.
	};

	/// Platform-independent base class for Cairo renderers.
	///
	/// \todo There are (possibly intended) memory leaks when using this renderer.
	/// \todo Are we using hardware acceleration by implementing it like this? (probably not)
	class renderer_base : public ui::renderer_base {
		friend font_family;
	public:
		/// Initializes the Pango context.
		renderer_base() {
			_pango_context.set_give(pango_font_map_create_context(pango_cairo_font_map_get_default()));
			// FIXME on windows, only fonts installed system-wide can be discovered
			//       fonts that are installed for one user cannot be found
			//       https://gitlab.freedesktop.org/fontconfig/fontconfig/-/issues/144
			assert_true_sys(FcInit(), "failed to initialize Fontconfig");
			_details::ft_check(FT_Init_FreeType(&_freetype));
		}
		/// Calls \p cairo_debug_reset_static_data() to clean up.
		~renderer_base() {
			_details::ft_check(FT_Done_FreeType(_freetype));
			_pango_context.reset();

			// without this pango would still be using some fonts which will cause the cairo check to fail
			pango_cairo_font_map_set_default(nullptr);
			cairo_debug_reset_static_data();

			// finally, unload fontconfig
			FcFini();
		}

		/// Creates a new image surface as a render target and clears it.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor, colord clear) override;

		/// Loads a \ref bitmap from disk as an image surface.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			auto res = std::make_unique<bitmap>();

			// TODO

			return res;
		}

		/// Creates a new \ref text_format.
		std::unique_ptr<ui::font_family> find_font_family(const std::u8string&) override;

		/// Starts drawing to the given window.
		void begin_drawing(ui::window_base&) override;
		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target&) override;
		/// Finishes drawing.
		void end_drawing() override;

		/// Clears the current surface.
		void clear(colord) override;

		/// Pushes a matrix onto the stack.
		void push_matrix(matd3x3 m) override {
			_render_stack.top().matrices.push(m);
			_render_stack.top().update_transform();
		}
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack.
		void push_matrix_mult(matd3x3 m) override {
			_render_stack.top().matrices.push(_render_stack.top().matrices.top() * m);
			_render_stack.top().update_transform();
		}
		/// Pops a matrix from the stack.
		void pop_matrix() override {
			_render_stack.top().matrices.pop();
			_render_stack.top().update_transform();
		}
		/// Returns the current transformation matrix.
		[[nodiscard]] matd3x3 get_matrix() const override {
			return _render_stack.top().matrices.top();
		}

		/// Returns \ref _path_builder with the current context.
		ui::path_geometry_builder &start_path() override {
			_path_builder._context = _render_stack.top().context;
			return _path_builder;
		}

		/// Draws an ellipse.
		void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_make_ellipse_geometry(center, radiusx, radiusy);
			_draw_path(_render_stack.top().context, brush, pen);
		}
		/// Draws a rectangle.
		void draw_rectangle(
			rectd rect, const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			cairo_t *context = _render_stack.top().context;
			cairo_rectangle(context, rect.xmin, rect.ymin, rect.width(), rect.height());
			_draw_path(context, brush, pen);
		}
		/// Draws a rounded rectangle.
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_make_rounded_rectangle_geometry(region, radiusx, radiusy);
			_draw_path(_render_stack.top().context, brush, pen);
		}
		/// Draws the path in the current context.
		void end_and_draw_path(
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			assert_true_usage(
				!_render_stack.empty() && _path_builder._context == _render_stack.top().context,
				"do not switch contexts when a path builder is in use"
			);
			_path_builder._context = nullptr;
			_draw_path(_render_stack.top().context, brush, pen);
		}

		/// Pushes a clip onto the stack with the shape of an ellipse.
		void push_ellipse_clip(vec2d center, double radiusx, double radiusy) override {
			_make_ellipse_geometry(center, radiusx, radiusy);
			_push_clip(_render_stack.top().context);
		}
		/// Pushes a clip onto the stack with the shape of a rectangle.
		void push_rectangle_clip(rectd rect) override {
			cairo_t *context = _render_stack.top().context;
			cairo_rectangle(context, rect.xmin, rect.ymin, rect.width(), rect.height());
			_push_clip(context);
		}
		/// Pushes a clip onto the stack with the shape of a rounded rectangle.
		void push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) override {
			_make_rounded_rectangle_geometry(rect, radiusx, radiusy);
			_push_clip(_render_stack.top().context);
		}
		/// Pushes a clip onto the stack with the shape of the current path.
		void end_and_push_path_clip() override {
			assert_true_usage(
				!_render_stack.empty() && _path_builder._context == _render_stack.top().context,
				"do not switch contexts when a path builder is in use"
			);
			_path_builder._context = nullptr;
			_push_clip(_render_stack.top().context);
		}
		/// Calls \p cairo_restore() to restore the previously saved clip area, resets the current path, and resets
		/// the transformation.
		void pop_clip() override {
			assert_true_usage(
				_path_builder._context == nullptr,
				"a path is being built whcih is going to be cleared by this call to pop_clip()"
			);

			cairo_t *context = _render_stack.top().context;
			cairo_restore(context);
			// since the path was saved along with the previous clip, we need to clear the current path
			cairo_new_path(context);
			// restore transformation
			_render_stack.top().update_transform();
		}

		/// Creates a new \ref formatted_text object.
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return _create_formatted_text_impl(text, font, c, size, wrap, halign, valign);
		}
		/// Creates a new \ref formatted_text object.
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint> utf32,
			const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			std::basic_string<std::byte> text;
			for (codepoint cp : utf32) {
				text += encodings::utf8::encode_codepoint(cp);
			}
			return _create_formatted_text_impl(
				std::u8string_view(reinterpret_cast<const char8_t*>(text.c_str()), text.size()),
				font, c, size, wrap, halign, valign
			);
		}
		/// Draws the given \ref formatted_text at the given position.
		void draw_formatted_text(const ui::formatted_text&, vec2d pos) override;

		/// \overload
		std::unique_ptr<ui::plain_text> create_plain_text(std::u8string_view, ui::font&, double) override;
		/// Creates a new \ref plain_text object for the given text and font.
		std::unique_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint>, ui::font&, double
		) override;
		/// Renders the given fragment of text.
		void draw_plain_text(const ui::plain_text&, vec2d, colord) override;
	protected:
		/// Holds the \p cairo_t associated with a window.
		struct _window_data {
			_details::gtk_object_ref<cairo_t> context; ///< The \p cairo_t.

			/// Returns the associated \p cairo_surface_t.
			[[nodiscard]] cairo_surface_t *get_surface() const {
				return cairo_get_target(context.get());
			}

			/// Returns the \ref _window_data associated with the given window.
			inline static _window_data &get(window_base &wnd) {
				auto *d = std::any_cast<_window_data>(&_get_window_data(wnd));
				assert_true_usage(d, "window has no associated data");
				return *d;
			}
		};
		/// Stores information about a currently active render target.
		struct _render_target_stackframe {
			/// Initializes \ref context and pushes an identity matrix onto \ref matrices.
			explicit _render_target_stackframe(cairo_t *c, window_base *w = nullptr) : context(c), window(w) {
				matd3x3 id;
				id.set_identity();
				matrices.emplace(id);
			}

			/// Updates the transform matrix of the context.
			void update_transform();

			std::stack<matd3x3> matrices; ///< The stack of matrices.
			/// The cairo context. Here we're using raw pointers for the same reason as in
			/// \ref os::direct2d::renderer.
			cairo_t *context = nullptr;
			window_base *window = nullptr; ///< The target window.
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of currently active render targets.
		path_geometry_builder _path_builder; ///< The \ref path_geometry_builder.
		_details::glib_object_ref<PangoContext> _pango_context; ///< The Pango context.
		FT_Library _freetype = nullptr; ///< The Freetype library.


		/// Draws the current path using the given brush and pen.
		static void _draw_path(cairo_t*, const ui::generic_brush_parameters&, const ui::generic_pen_parameters&);
		/// Saves the current cairo context status onto the stack by calling \p cairo_save(), then calls
		/// \p cairo_clip() to update the clip region.
		inline static void _push_clip(cairo_t *context) {
			// here the current path is also saved, so we'll need to claer the path after calling cairo_restore()
			cairo_save(context);
			cairo_clip(context);
		}

		/// Creates a new solid color pattern.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::solid_color&
		);
		/// Adds gradient stops to a gradient pattern.
		static void _add_gradient_stops(cairo_pattern_t*, const gradient_stop_collection&);
		/// Creates a new linear gradient pattern.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::linear_gradient&
		);
		/// Creates a new radial gradient pattern.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::radial_gradient&
		);
		/// Creates a new bitmap gradient pattern.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::bitmap_pattern&
		);
		/// Returns an empty \ref _details::gtk_object_ref.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::none&
		);
		/// Creates a new \p cairo_pattern_t given the parameters of the brush.
		static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const generic_brush_parameters&
		);


		/// Changes the current path into a ellipse.
		void _make_ellipse_geometry(vec2d center, double rx, double ry);
		/// Changes the current path into a rounded rectangle.
		void _make_rounded_rectangle_geometry(rectd, double rx, double ry);


		/// Creates a new \ref formatted_text object.
		[[nodiscard]] std::unique_ptr<formatted_text> _create_formatted_text_impl(
			std::u8string_view text, const font_parameters&, colord, vec2d size, wrapping_mode,
			horizontal_text_alignment, vertical_text_alignment
		);
		/// Creates a new \ref plain_text from the given \p hb_buffer_t.
		[[nodiscard]] std::unique_ptr<ui::plain_text> _create_plain_text_impl(
			_details::gtk_object_ref<hb_buffer_t>, ui::font&, double
		);


		/// Called to finalize drawing to the current rendering target.
		virtual void _finish_drawing_to_target() = 0;

		/// Creates a new Cairo surface for the given window.
		virtual _details::gtk_object_ref<cairo_surface_t> _create_surface_for_window(window_base&) = 0;
		/// Calls \ref _create_surface_for_window() to create a surface for the given window, sets the
		/// appropriate scaling factor, then creates and returns a Cairo context.
		_details::gtk_object_ref<cairo_t> _create_context_for_window(window_base &wnd, vec2d scaling) {
			auto surface = _create_surface_for_window(wnd);
			cairo_surface_set_device_scale(surface.get(), scaling.x, scaling.y);
			auto result = _details::make_gtk_object_ref_give(cairo_create(surface.get()));
			return result;
		}
		/// Creates a Cairo surface for the window, and listens to specific events to resize the surface as needed.
		void _new_window(window_base&) override;
		/// Releases all resources.
		void _delete_window(window_base &w) override {
			_get_window_data(w).reset();
		}
	};
}

#endif
