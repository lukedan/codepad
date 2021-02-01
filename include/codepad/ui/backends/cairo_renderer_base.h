// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains the base class of the Cairo renderer backend.

#include <stack>

#include <cairo.h>

#include "../../core/math.h"
#include "../renderer.h"
#include "../window.h"
#include "pango_harfbuzz_text_context.h"

namespace codepad::ui::cairo {
	class renderer_base;

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
	public:
		/// Calls \p cairo_debug_reset_static_data() to clean up.
		~renderer_base() {
			// free pango stuff so cairo can be reset without problems
			_text_context.deinitialize();
			cairo_debug_reset_static_data();
		}

		/// Creates a new image surface as a render target and clears it.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor, colord clear) override;

		/// Invokes \ref pango_harfbuzz::text_context::find_font_family().
		std::shared_ptr<ui::font_family> find_font_family(const std::u8string &family) override {
			return _text_context.find_font_family(family);
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

		/// Invokes \ref pango_harfbuzz::text_context::create_formatted_text().
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return _text_context.create_formatted_text(text, font, c, size, wrap, halign, valign);
		}
		/// \overload
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint> utf32,
			const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return _text_context.create_formatted_text(utf32, font, c, size, wrap, halign, valign);
		}
		/// Draws the given \ref formatted_text at the given position.
		void draw_formatted_text(const ui::formatted_text&, vec2d pos) override;

		/// Invokes \ref pango_harfbuzz::text_context::create_plain_text().
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::u8string_view text, ui::font &fnt, double font_size
		) override {
			return _text_context.create_plain_text(text, fnt, font_size);
		}
		/// \overload
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint> text, ui::font &fnt, double font_size
		) override {
			return _text_context.create_plain_text(text, fnt, font_size);
		}
		/// Invokes \ref pango_harfbuzz::text_context::create_plain_text_fast().
		std::shared_ptr<ui::plain_text> create_plain_text_fast(
			std::basic_string_view<codepoint> text, ui::font &fnt, double size
		) override {
			return _text_context.create_plain_text_fast(text, fnt, size);
		}
		/// Renders the given fragment of text.
		void draw_plain_text(const ui::plain_text&, vec2d, colord) override;
	protected:
		/// Holds the \p cairo_t associated with a window.
		struct _window_data {
			_details::gtk_object_ref<cairo_t> context; ///< The \p cairo_t.
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
			explicit _render_target_stackframe(cairo_t *c, window *w = nullptr) : context(c), window(w) {
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
			window *window = nullptr; ///< The target window.
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of currently active render targets.
		path_geometry_builder _path_builder; ///< The \ref path_geometry_builder.
		pango_harfbuzz::text_context _text_context; ///< The context for text layout.
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
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::solid_color&
		);
		/// Adds gradient stops to a gradient pattern.
		static void _add_gradient_stops(cairo_pattern_t*, const gradient_stop_collection&);
		/// Creates a new linear gradient pattern.
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::linear_gradient&
		);
		/// Creates a new radial gradient pattern.
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::radial_gradient&
		);
		/// Creates a new bitmap gradient pattern.
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::bitmap_pattern&
		);
		/// Returns an empty \ref _details::gtk_object_ref.
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const brushes::none&
		);
		/// Creates a new \p cairo_pattern_t given the parameters of the brush.
		[[nodiscard]] static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const generic_brush&
		);


		/// Creates a surface similar to that of the given window. By default this function invokes
		/// \p cairo_surface_create_similar(), but derived classes can override this to change this behavior. This
		/// function does not need to handle errors or device scaling.
		[[nodiscard]] virtual _details::gtk_object_ref<cairo_surface_t> _create_similar_surface(
			window &wnd, int width, int height
		);
		/// Creates a new offscreen surface for use as render targets or bitmap surfaces.
		[[nodiscard]] _details::gtk_object_ref<cairo_surface_t> _create_offscreen_surface(
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
		[[nodiscard]] inline static _details::gtk_object_ref<cairo_surface_t> &_bitmap_surface(bitmap &bmp) {
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
