// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains the base class of the Cairo renderer backend.

#include <stack>

#include <cairo.h>
#include <pango/pangocairo.h>

#include "codepad/core/math.h"
#include "codepad/ui/renderer.h"
#include "codepad/ui/window.h"
#include "pango_harfbuzz_text_engine.h"

namespace codepad::ui::cairo {
	class renderer_base;

	namespace _details {
		/// Reference-counted handle of a Cairo object.
		template <typename T> struct cairo_object_ref final :
			public reference_counted_handle<cairo_object_ref<T>, T*> {
			friend reference_counted_handle<cairo_object_ref<T>, T*>;
		public:
			constexpr static T *empty_handle = nullptr; ///< The empty handle.
		protected:
			/// Adds a reference to the handle if necessary.
			void _do_add_ref() {
				if constexpr (std::is_same_v<T, cairo_t>) {
					cairo_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_font_face_t>) {
					cairo_font_face_reference(this->_handle);
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
				} else {
					logger::get().log_error(CP_HERE) <<
						"release operation for not implemented for " << demangle(typeid(T).name());
					std::abort();
				}
			}
		};
		/// Creates a new \ref cairo_object_ref, and calls \ref cairo_object_ref::set_share() to share the given
		/// pointer.
		template <typename T> inline cairo_object_ref<T> make_cairo_object_ref_share(T *ptr) {
			cairo_object_ref<T> res;
			res.set_share(ptr);
			return res;
		}
		/// Creates a new \ref cairo_object_ref, and calls \ref cairo_object_ref::set_give() to give the given
		/// pointer to it.
		template <typename T> inline cairo_object_ref<T> make_cairo_object_ref_give(T *ptr) {
			cairo_object_ref<T> res;
			res.set_give(ptr);
			return res;
		}
	}

	/// A Cairo surface used as a source.
	class bitmap : public ui::bitmap {
		friend renderer_base;
	public:
		/// Returns \ref _size.
		vec2d get_size() const override {
			return _size;
		}
	protected:
		vec2d _size; ///< The logical size of this bitmap.
		_details::cairo_object_ref<cairo_surface_t> _surface; ///< The underlying Cairo surface.
	};

	/// A Cairo surface used as a render target.
	class render_target : public ui::render_target {
		friend renderer_base;
	protected:
		// we don't need to store the surface handle as we can just call cairo_get_target()
		_details::cairo_object_ref<cairo_t> _context; ///< The cairo context.

		/// Returns the target surface.
		cairo_surface_t *_get_target() {
			return cairo_get_target(_context.get());
		}
	};

	/// Wraps around a \ref pango_harfbuzz::font_data.
	class font : public ui::font {
		friend renderer_base;
	public:
		/// Initializes \ref _data.
		explicit font(pango_harfbuzz::font_data d) : ui::font(), _data(std::move(d)) {
		}

		/// Returns \ref pango_harfbuzz::font_data::get_ascent_em().
		[[nodiscard]] double get_ascent_em() const override {
			return _data.get_ascent_em();
		}
		/// Returns \ref pango_harfbuzz::font_data::get_line_height_em().
		[[nodiscard]] double get_line_height_em() const override {
			return _data.get_line_height_em();
		}

		/// Returns \ref pango_harfbuzz::font_data::has_character().
		[[nodiscard]] bool has_character(codepoint cp) const override {
			return _data.has_character(cp);
		}

		/// Returns \ref pango_harfbuzz::font_data::get_character_width_em().
		[[nodiscard]] double get_character_width_em(codepoint cp) const override {
			return _data.get_character_width_em(cp);
		}
	protected:
		pango_harfbuzz::font_data _data; ///< Font data.
	};

	/// Wraps around \ref pango_harfbuzz::font_family_data.
	class font_family : public ui::font_family {
	public:
		/// Initializes \ref _data.
		explicit font_family(pango_harfbuzz::text_engine &eng, pango_harfbuzz::font_family_data d) :
			ui::font_family(), _data(std::move(d)), _text_engine(&eng) {
		}

		/// Searches in the cache for a matching font, or creates a new font and caches it.
		[[nodiscard]] std::shared_ptr<ui::font> get_matching_font(
			font_style style, font_weight weight, font_stretch stretch
		) const override {
			auto &entry = _data.get_cache_entry();
			auto [it, inserted] = entry.font_faces.try_emplace(pango_harfbuzz::font_params(style, weight, stretch));
			if (inserted) {
				auto found = entry.find_font(style, weight, stretch);
				it->second = std::make_shared<font>(_text_engine->create_font_for_file(
					reinterpret_cast<const char*>(found.get_font_file_path()), found.get_font_index()
				));
			}
			return it->second;
		}
	protected:
		pango_harfbuzz::font_family_data _data; ///< The font family object.
		pango_harfbuzz::text_engine *_text_engine = nullptr; ///< The associated text engine used for font loading.
	};

	/// Wraps around a \ref pango_harfbuzz::plain_text_data.
	class plain_text : public ui::plain_text {
		friend renderer_base;
	public:
		/// Initializes \ref _data.
		explicit plain_text(pango_harfbuzz::plain_text_data data) : ui::plain_text(), _data(std::move(data)) {
		}

		/// Returns \ref pango_harfbuzz::plain_text_data::get_width().
		[[nodiscard]] double get_width() const override {
			return _data.get_width();
		}

		/// Returns \ref pango_harfbuzz::plain_text_data::hit_test().
		[[nodiscard]] caret_hit_test_result hit_test(double x) const override {
			return _data.hit_test(x);
		}
		/// Returns \ref pango_harfbuzz::plain_text_data::get_character_placement().
		[[nodiscard]] rectd get_character_placement(std::size_t i) const override {
			return _data.get_character_placement(i);
		}
	protected:
		pango_harfbuzz::plain_text_data _data; ///< The plain text object.
	};

	/// Wraps around a \ref 
	class formatted_text : public ui::formatted_text {
		friend renderer_base;
	public:
		/// Initializes \ref _data.
		explicit formatted_text(pango_harfbuzz::formatted_text_data data) : ui::formatted_text(), _data(std::move(data)) {
		}

		/// Returns \ref pango_harfbuzz::formatted_text_data::get_layout().
		[[nodiscard]] rectd get_layout() const override {
			return _data.get_layout();
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_line_metrics().
		[[nodiscard]] std::vector<line_metrics> get_line_metrics() const override {
			return _data.get_line_metrics();
		}

		/// Returns \ref pango_harfbuzz::formatted_text_data::get_num_characters().
		[[nodiscard]] std::size_t get_num_characters() const override {
			return _data.get_num_characters();
		}

		/// Returns \ref pango_harfbuzz::formatted_text_data::hit_test().
		[[nodiscard]] caret_hit_test_result hit_test(vec2d x) const override {
			return _data.hit_test(x);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::hit_test_at_line().
		[[nodiscard]] caret_hit_test_result hit_test_at_line(std::size_t line, double x) const override {
			return _data.hit_test_at_line(line, x);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_character_placement().
		[[nodiscard]] rectd get_character_placement(std::size_t i) const override {
			return _data.get_character_placement(i);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_character_range_placement().
		[[nodiscard]] std::vector<rectd> get_character_range_placement(
			std::size_t beg, std::size_t len
		) const override {
			return _data.get_character_range_placement(beg, len);
		}

		/// Returns \ref pango_harfbuzz::formatted_text_data::get_layout_size().
		[[nodiscard]] vec2d get_layout_size() const override {
			return _data.get_layout_size();
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::set_layout_size().
		void set_layout_size(vec2d size) override {
			return _data.set_layout_size(size);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_horizontal_alignment().
		[[nodiscard]] horizontal_text_alignment get_horizontal_alignment() const override {
			return _data.get_horizontal_alignment();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_horizontal_alignment().
		void set_horizontal_alignment(horizontal_text_alignment align) override {
			_data.set_horizontal_alignment(align);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_vertical_alignment().
		[[nodiscard]] vertical_text_alignment get_vertical_alignment() const override {
			return _data.get_vertical_alignment();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_vertical_alignment().
		void set_vertical_alignment(vertical_text_alignment align) override {
			_data.set_vertical_alignment(align);
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_wrapping_mode().
		[[nodiscard]] wrapping_mode get_wrapping_mode() const override {
			return _data.get_wrapping_mode();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_wrapping_mode().
		void set_wrapping_mode(wrapping_mode wrap) override {
			_data.set_wrapping_mode(wrap);
		}

		/// Calls \ref pango_harfbuzz::formatted_text_data::set_text_color().
		void set_text_color(colord c, std::size_t beg, std::size_t len) override {
			_data.set_text_color(c, beg, len);
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_family().
		void set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) override {
			_data.set_font_family(family, beg, len);
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_size().
		void set_font_size(double size, std::size_t beg, std::size_t len) override {
			_data.set_font_size(size, beg, len);
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_style().
		void set_font_style(font_style style, std::size_t beg, std::size_t len) override {
			_data.set_font_style(style, beg, len);
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_weight().
		void set_font_weight(font_weight weight, std::size_t beg, std::size_t len) override {
			_data.set_font_weight(weight, beg, len);
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_stretch().
		void set_font_stretch(font_stretch stretch, std::size_t beg, std::size_t len) override {
			_data.set_font_stretch(stretch, beg, len);
		}
	protected:
		pango_harfbuzz::formatted_text_data _data; ///< The formatted text object.
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


	namespace _details {
		using namespace ui::_details;

		/// Casts a \ref ui::font to a \ref font.
		[[nodiscard]] font &cast_font(ui::font &f) {
			auto *res = dynamic_cast<font*>(&f);
			assert_true_logical(res, "invalid font type");
			return *res;
		}

		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		[[nodiscard]] const formatted_text &cast_formatted_text(const ui::formatted_text &f) {
			auto *res = dynamic_cast<const formatted_text*>(&f);
			assert_true_logical(res, "invalid formatted_text type");
			return *res;
		}

		/// Casts a \ref ui::plain_text to a \ref plain_text.
		[[nodiscard]] const plain_text &cast_plain_text(const ui::plain_text &f) {
			auto *res = dynamic_cast<const plain_text*>(&f);
			assert_true_logical(res, "invalid plain_text type");
			return *res;
		}
	}


	/// Platform-independent base class for Cairo renderers.
	///
	/// \todo There are (possibly intended) memory leaks when using this renderer.
	/// \todo Are we using hardware acceleration by implementing it like this? (probably not)
	class renderer_base : public ui::renderer_base {
	public:
		/// Initializes \ref _text_engine using a new font map created by \p pango_cairo_font_map_get_default().
		renderer_base() : ui::renderer_base(), _text_engine(pango_cairo_font_map_get_default()) {
		}
		/// Calls \p cairo_debug_reset_static_data() to clean up.
		~renderer_base() {
			// although this will replace the font map with a new instance, it will still hopefully free
			// resources the old one's holding on to. without this pango would still be using some fonts which
			// will cause the cairo check to fail
			pango_cairo_font_map_set_default(nullptr);
			_text_engine.deinitialize();
			// check if anything has not been freed yet
			cairo_debug_reset_static_data();
		}

		/// Creates a new image surface as a render target and clears it.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor, colord clear) override;

		/// Invokes \ref pango_harfbuzz::text_engine::find_font_family().
		std::shared_ptr<ui::font_family> find_font_family(const std::u8string &family) override {
			return std::make_shared<font_family>(_text_engine, _text_engine.find_font_family(family.c_str()));
		}

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
			const ui::generic_brush &brush, const ui::generic_pen &pen
		) override {
			_make_ellipse_geometry(center, radiusx, radiusy);
			_draw_path(_render_stack.top().context, brush, pen);
		}
		/// Draws a rectangle.
		void draw_rectangle(
			rectd rect, const ui::generic_brush &brush, const ui::generic_pen &pen
		) override {
			cairo_t *context = _render_stack.top().context;
			cairo_rectangle(context, rect.xmin, rect.ymin, rect.width(), rect.height());
			_draw_path(context, brush, pen);
		}
		/// Draws a rounded rectangle.
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush &brush, const ui::generic_pen &pen
		) override {
			_make_rounded_rectangle_geometry(region, radiusx, radiusy);
			_draw_path(_render_stack.top().context, brush, pen);
		}
		/// Draws the path in the current context.
		void end_and_draw_path(
			const ui::generic_brush &brush, const ui::generic_pen &pen
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

		/// Invokes \ref pango_harfbuzz::text_engine::create_formatted_text().
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return std::make_shared<formatted_text>(
				_text_engine.create_formatted_text(text, font, c, size, wrap, halign, valign)
			);
		}
		/// \overload
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint> utf32,
			const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return std::make_shared<formatted_text>(
				_text_engine.create_formatted_text(utf32, font, c, size, wrap, halign, valign)
			);
		}
		/// Draws the given \ref formatted_text at the given position.
		void draw_formatted_text(const ui::formatted_text&, vec2d pos) override;

		/// Invokes \ref pango_harfbuzz::text_engine::create_plain_text().
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::u8string_view text, ui::font &generic_fnt, double font_size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			return std::make_shared<plain_text>(_text_engine.create_plain_text(text, fnt._data, font_size));
		}
		/// \overload
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint> text, ui::font &generic_fnt, double font_size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			return std::make_shared<plain_text>(_text_engine.create_plain_text(text, fnt._data, font_size));
		}
		/// Invokes \ref pango_harfbuzz::text_engine::create_plain_text_fast().
		std::shared_ptr<ui::plain_text> create_plain_text_fast(
			std::basic_string_view<codepoint> text, ui::font &generic_fnt, double size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			return std::make_shared<plain_text>(_text_engine.create_plain_text_fast(text, fnt._data, size));
		}
		/// Renders the given fragment of text.
		void draw_plain_text(const ui::plain_text&, vec2d, colord) override;
	protected:
		/// Holds the \p cairo_t associated with a window.
		struct _window_data {
			_details::cairo_object_ref<cairo_t> context; ///< The \p cairo_t.
			window
				*prev = nullptr, ///< Previous window. Forms a loop between all windows.
				*next = nullptr; ///< Next window. Forms a loop between all windows.

			/// Returns the associated \p cairo_surface_t.
			[[nodiscard]] cairo_surface_t *get_surface() const {
				return cairo_get_target(context.get());
			}
		};
		/// Stores information about a currently active render target.
		struct _render_target_stackframe {
			/// Initializes \ref context and pushes an identity matrix onto \ref matrices.
			explicit _render_target_stackframe(cairo_t *c, window *w = nullptr) : context(c), target_wnd(w) {
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
			window *target_wnd = nullptr; ///< The target window.
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of currently active render targets.
		path_geometry_builder _path_builder; ///< The \ref path_geometry_builder.
		pango_harfbuzz::text_engine _text_engine; ///< The engine for text layout.
		/// Pointer to a random window. This is used with \ref _window_data::prev and \ref _window_data::next to keep
		/// track of all existing windows.
		window *_random_window = nullptr;


		/// Draws the current path using the given brush and pen.
		static void _draw_path(cairo_t*, const ui::generic_brush&, const ui::generic_pen&);
		/// Saves the current cairo context status onto the stack by calling \p cairo_save(), then calls
		/// \p cairo_clip() to update the clip region.
		inline static void _push_clip(cairo_t *context) {
			// here the current path is also saved, so we'll need to claer the path after calling cairo_restore()
			cairo_save(context);
			cairo_clip(context);
		}

		/// Creates a new solid color pattern.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::solid_color&
		);
		/// Adds gradient stops to a gradient pattern.
		static void _add_gradient_stops(cairo_pattern_t*, const gradient_stop_collection&);
		/// Creates a new linear gradient pattern.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::linear_gradient&
		);
		/// Creates a new radial gradient pattern.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::radial_gradient&
		);
		/// Creates a new bitmap gradient pattern.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::bitmap_pattern&
		);
		/// Returns an empty \ref _details::cairo_object_ref.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::none&
		);
		/// Creates a new \p cairo_pattern_t given the parameters of the brush.
		[[nodiscard]] static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const generic_brush&
		);


		/// Creates a surface similar to that of the given window. By default this function invokes
		/// \p cairo_surface_create_similar(), but derived classes can override this to change this behavior. This
		/// function does not need to handle errors or device scaling.
		[[nodiscard]] virtual _details::cairo_object_ref<cairo_surface_t> _create_similar_surface(
			window &wnd, int width, int height
		);
		/// Creates a new offscreen surface for use as render targets or bitmap surfaces.
		[[nodiscard]] _details::cairo_object_ref<cairo_surface_t> _create_offscreen_surface(
			int width, int height, vec2d scale
		);


		/// Changes the current path into a ellipse.
		void _make_ellipse_geometry(vec2d center, double rx, double ry);
		/// Changes the current path into a rounded rectangle.
		void _make_rounded_rectangle_geometry(rectd, double rx, double ry);


		/// Called to finalize drawing to the current rendering target.
		virtual void _finish_drawing_to_target() = 0;


		/// Allow children classes to access \ref bitmap::_size.
		[[nodiscard]] inline static vec2d &_bitmap_size(bitmap &bmp) {
			return bmp._size;
		}
		/// Allow children classes to access \ref bitmap::_surface.
		[[nodiscard]] inline static _details::cairo_object_ref<cairo_surface_t> &_bitmap_surface(bitmap &bmp) {
			return bmp._surface;
		}


		/// Adds the window to the linked list loop pointed to by \ref _random_window.
		void _new_window(window &w) override {
			auto &data = _get_window_data(w).emplace<_window_data>();
			if (_random_window) {
				auto &next_data = _get_window_data_as<_window_data>(*_random_window);
				auto &prev_data = _get_window_data_as<_window_data>(*next_data.prev);
				data.next = _random_window;
				data.prev = next_data.prev;
				next_data.prev = prev_data.next = &w;
			} else {
				_random_window = data.next = data.prev = &w;
			}
		}
		/// Releases all resources.
		void _delete_window(window &w) override {
			auto &data = _get_window_data_as<_window_data>(w);
			if (data.next == &w) {
				assert_true_logical(data.prev == &w && _random_window == &w, "invalid linked list loop");
				_random_window = nullptr;
			} else {
				_random_window = data.next;
				auto &next_data = _get_window_data_as<_window_data>(*data.next);
				auto &prev_data = _get_window_data_as<_window_data>(*data.prev);
				next_data.prev = data.prev;
				prev_data.next = data.next;
			}
			_get_window_data(w).reset();
		}
	};
}
