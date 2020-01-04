// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Contains the base class of the Cairo renderer backend.

#ifdef CP_USE_CAIRO

#	include <stack>

#	include <cairo.h>

#	include "../core/math.h"
#	include "renderer.h"
#	include "window.h"

namespace codepad::ui::cairo {
	class renderer_base;

	namespace _details {
		/// Converts a \ref matd3x3 to a \p cairo_matrix_t.
		inline static cairo_matrix_t cast_matrix(matd3x3 m) {
			cairo_matrix_t result;
			result.xx = m[0][0];
			result.xy = m[0][1];
			result.x0 = m[0][2];
			result.yx = m[1][0];
			result.yy = m[1][1];
			result.y0 = m[1][2];
			return result;
		}

		/// Reference to a Cairo object. This struct handles reference counting.
		template <typename T> struct cairo_object_ref final :
			public reference_counted_handle<cairo_object_ref<T>, T> {
			friend reference_counted_handle<cairo_object_ref<T>, T>;
		protected:
			/// Adds a reference to the handle if necessary.
			void _do_add_ref() {
				if constexpr (std::is_same_v<T, cairo_t>) {
					cairo_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_reference(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) << "Cairo add ref operation not implemented";
					abort();
				}
			}
			/// Removes a reference to the handle if necessary.
			void _do_release() {
				if constexpr (std::is_same_v<T, cairo_t>) {
					cairo_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_destroy(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_destroy(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) << "Cairo release operation for not implemented";
					abort();
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
		[[nodiscard]] vec2d get_size() const override {
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
		// we don't need to store the surface handle as we ca just call cairo_get_target()
		_details::cairo_object_ref<cairo_t> _context; ///< The cairo context.

		/// Returns the target surface.
		cairo_surface_t *_get_target() {
			return cairo_get_target(_context.get());
		}
	};

	///
	class text_format : public ui::text_format {
		friend renderer_base;
	protected:
		// TODO
	};

	///
	class formatted_text : public ui::formatted_text {
		friend renderer_base;
	public:
		/// Returns the layout of the text.
		rectd get_layout() const override {
			// TODO
			return rectd();
		}
		/// Returns the metrics of each line.
		std::vector<line_metrics> get_line_metrics() const override {
			return std::vector<line_metrics>(1);
			// TODO
		}

		///
		caret_hit_test_result hit_test(vec2d pos) const override {
			// TODO
			return caret_hit_test_result(0, rectd(), false);
		}
		///
		rectd get_character_placement(std::size_t pos) const override {
			// TODO
			return rectd();
		}
	protected:
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
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type) override {
			vec2d current_point;
			cairo_get_current_point(_context, &current_point.x, &current_point.y);
			// TODO
		}
	protected:
		cairo_t *_context = nullptr; ///< The Cairo context.
	};

	namespace _details {
		/// Casts a \ref ui::bitmap to a \ref bitmap.
		inline bitmap &cast_bitmap(ui::bitmap &b) {
			auto *bmp = dynamic_cast<bitmap*>(&b);
			assert_true_usage(bmp, "invalid bitmap type");
			return *bmp;
		}
		/// Casts a \ref ui::render_target to a \ref render_target.
		inline render_target &cast_render_target(ui::render_target &r) {
			auto *rt = dynamic_cast<render_target*>(&r);
			assert_true_usage(rt, "invalid render target type");
			return *rt;
		}
	}

	/// Platform-independent base class for Cairo renderers.
	///
	/// \todo There are (possibly intended) memory leaks when using this renderer.
	/// \todo Are we using hardware acceleration by implementing it like this? (probably not)
	class renderer_base : public ui::renderer_base {
	public:
		/// Creates a new image surface as a render target.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor) override {
			auto resrt = std::make_unique<render_target>();
			auto resbmp = std::make_unique<bitmap>();

			// create surface
			resbmp->_size = size;
			resbmp->_surface = _details::make_cairo_object_ref_give(cairo_image_surface_create(
				CAIRO_FORMAT_ARGB32,
				static_cast<int>(std::ceil(size.x * scaling_factor.x)),
				static_cast<int>(std::ceil(size.y * scaling_factor.y))
			));
			assert_true_sys(
				cairo_surface_status(resbmp->_surface.get()) == CAIRO_STATUS_SUCCESS,
				"failed to create cairo surface"
			);
			// set dpi scaling
			cairo_surface_set_device_scale(resbmp->_surface.get(), scaling_factor.x, scaling_factor.y);

			// create context
			resrt->_context = _details::make_cairo_object_ref_give(cairo_create(resbmp->_surface.get()));
			assert_true_sys(
				cairo_status(resrt->_context.get()) == CAIRO_STATUS_SUCCESS,
				"failed to create cairo context"
			);

			return render_target_data(std::move(resrt), std::move(resbmp));
		}
		/// Calls \p cairo_debug_reset_static_data() to clean up.
		~renderer_base() {
			cairo_debug_reset_static_data();
		}

		/// Loads a \ref bitmap from disk as an image surface.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			auto res = std::make_unique<bitmap>();

			// TODO

			return res;
		}

		/// Creates a new \ref text_format.
		std::unique_ptr<ui::text_format> create_text_format(
			str_view_t family, double size, ui::font_style style, ui::font_weight weight, ui::font_stretch stretch
		) override {
			// TODO
			return std::make_unique<text_format>();
		}

		/// Starts drawing to the given window.
		void begin_drawing(ui::window_base &w) override {
			auto &data = _window_data::get(w);
			_render_stack.emplace(data.context.get(), &w);
		}
		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target &generic_rt) override {
			auto &rt = _details::cast_render_target(generic_rt);
			_render_stack.emplace(rt._context.get());
		}
		/// Finishes drawing.
		void end_drawing() override {
			assert_true_usage(!_render_stack.empty(), "begin_drawing/end_drawing calls mismatch");
			assert_true_usage(_render_stack.top().matrices.size() == 1, "push_matrix/pop_matrix calls mismatch");
			_finish_drawing_to_target();
			_render_stack.pop();
		}

		/// Pushes a matrix onto the stack.
		void push_matrix(matd3x3 m) override {
			_render_stack.top().matrices.push(m);
			_render_stack.top().update_transform();
		}
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack.
		void push_matrix_mult(matd3x3 m) override {
			_render_stack.top().matrices.push(m * _render_stack.top().matrices.top());
			_render_stack.top().update_transform();
		}
		/// Pops a matrix from the stack.
		void pop_matrix() override {
			_render_stack.top().matrices.pop();
			_render_stack.top().update_transform();
		}

		/// Clears the current surface.
		void clear(colord color) override {
			cairo_t *context = _render_stack.top().context;
			cairo_save(context);
			{
				// reset state
				cairo_reset_clip(context);
				cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
				// clear
				cairo_set_source_rgba(context, color.r, color.g, color.b, color.a);
				cairo_paint(context);
			}
			cairo_restore(context);
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
		///
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			// TODO
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

		/// Pushes an ellipse onto the stack as a clip.
		void push_ellipse_clip(vec2d center, double radiusx, double radiusy) override {
			_make_ellipse_geometry(center, radiusx, radiusy);
			_push_clip(_render_stack.top().context);
		}
		/// Pushes a rectnagle onto the stack as a clip.
		void push_rectangle_clip(rectd rect) override {
			cairo_t *context = _render_stack.top().context;
			cairo_rectangle(context, rect.xmin, rect.ymin, rect.width(), rect.height());
			_push_clip(context);
		}
		///
		void push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) override {
			// TODO
		}
		/// Pushes the current path onto the stack as a clip.
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

		///
		std::unique_ptr<ui::formatted_text> format_text(
			str_view_t text, ui::text_format &fmt, vec2d maxsize, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
		) override {
			// TODO
			return std::make_unique<formatted_text>();
		}
		///
		std::unique_ptr<ui::formatted_text> format_text(
			std::basic_string_view<codepoint> text, ui::text_format &fmt, vec2d maxsize, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
		) override {
			// TODO
			return std::make_unique<formatted_text>();
		}
		///
		void draw_formatted_text(
			ui::formatted_text &text, vec2d topleft, const ui::generic_brush_parameters &brush
		) override {
			// TODO
		}
		///
		void draw_text(
			str_view_t text, rectd layout,
			ui::text_format &format, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign,
			const ui::generic_brush_parameters &brush
		) override {
			// TODO
		}
		///
		void draw_text(
			std::basic_string_view<codepoint> text, rectd layout,
			ui::text_format &format, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign,
			const ui::generic_brush_parameters &brush
		) override {
			// TODO
		}
	protected:
		/// Holds the \p cairo_t associated with a window.
		struct _window_data {
			_details::cairo_object_ref<cairo_t> context; ///< The \p cairo_t.

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
			void update_transform() {
				cairo_matrix_t mat = _details::cast_matrix(matrices.top());
				cairo_set_matrix(context, &mat);
			}

			std::stack<matd3x3> matrices; ///< The stack of matrices.
			/// The cairo context. Here we're using raw pointers for the same reason as in
			/// \ref os::direct2d::renderer.
			cairo_t *context = nullptr;
			window_base *window = nullptr; ///< The target window.
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of currently active render targets.
		path_geometry_builder _path_builder; ///< The \ref path_geometry_builder.


		/// Draws the current path using the given brush and pen.
		inline static void _draw_path(
			cairo_t *context,
			const ui::generic_brush_parameters &brush,
			const ui::generic_pen_parameters &pen
		) {
			if (_details::cairo_object_ref<cairo_pattern_t> brush_patt = _create_pattern(brush)) {
				cairo_set_source(context, brush_patt.get());
				cairo_fill_preserve(context);
			}
			if (_details::cairo_object_ref<cairo_pattern_t> pen_patt = _create_pattern(pen.brush)) {
				cairo_set_source(context, pen_patt.get());
				cairo_set_line_width(context, pen.thickness);
				cairo_stroke_preserve(context);
			}
			cairo_new_path(context); // clear current path
			// release the source pattern
			cairo_set_source_rgb(context, 1.0, 0.4, 0.7);
		}
		/// Saves the current cairo context status onto the stack by calling \p cairo_save(), then calls
		/// \p cairo_clip() to update the clip region.
		inline static void _push_clip(cairo_t *context) {
			// here the current path is also saved, so we'll need to claer the path after calling cairo_restore()
			cairo_save(context);
			cairo_clip(context);
		}

		/// Creates a new solid color pattern.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::solid_color &brush
		) {
			return _details::make_cairo_object_ref_give(cairo_pattern_create_rgba(
				brush.color.r, brush.color.g, brush.color.b, brush.color.a
			));
		}
		/// Adds gradient stops to a gradient pattern.
		inline static void _add_gradient_stops(cairo_pattern_t *patt, const gradient_stop_collection &gradients) {
			for (const gradient_stop &stop : gradients) {
				cairo_pattern_add_color_stop_rgba(
					patt, stop.position, stop.color.r, stop.color.g, stop.color.b, stop.color.a
				);
			}
		}
		/// Creates a new linear gradient pattern.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::linear_gradient &brush
		) {
			if (brush.gradients) {
				auto patt = _details::make_cairo_object_ref_give(cairo_pattern_create_linear(
					brush.from.x, brush.from.y, brush.to.x, brush.to.y
				));
				_add_gradient_stops(patt.get(), *brush.gradients);
				return patt;
			}
			return _details::cairo_object_ref<cairo_pattern_t>();
		}
		/// Creates a new radial gradient pattern.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::radial_gradient &brush
		) {
			if (brush.gradients) {
				auto patt = _details::make_cairo_object_ref_give(cairo_pattern_create_radial(
					brush.center.x, brush.center.y, 0.0, brush.center.x, brush.center.y, brush.radius
				));
				_add_gradient_stops(patt.get(), *brush.gradients);
				return patt;
			}
			return _details::cairo_object_ref<cairo_pattern_t>();
		}
		/// Creates a new bitmap gradient pattern.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::bitmap_pattern &brush
		) {
			if (brush.image) {
				return _details::make_cairo_object_ref_give(cairo_pattern_create_for_surface(
					_details::cast_bitmap(*brush.image)._surface.get()
				));
			}
			return _details::cairo_object_ref<cairo_pattern_t>();
		}
		/// Returns an empty \ref _details::cairo_object_ref.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::none&
		) {
			return _details::cairo_object_ref<cairo_pattern_t>();
		}
		/// Creates a new \p cairo_pattern_t given the parameters of the brush.
		inline static _details::cairo_object_ref<cairo_pattern_t> _create_pattern(
			const generic_brush_parameters &b
		) {
			auto pattern = std::visit([](auto &&brush) {
				return _create_pattern(brush);
				}, b.value);
			if (pattern) {
				cairo_matrix_t mat = _details::cast_matrix(b.transform);
				cairo_pattern_set_matrix(pattern.get(), &mat);
			}
			return pattern;
		}


		/// Changes the current path into a ellipse.
		void _make_ellipse_geometry(vec2d center, double rx, double ry) {
			_render_target_stackframe &stackframe = _render_stack.top();
			cairo_t *context = stackframe.context;

			cairo_matrix_t mat = _details::cast_matrix(
				stackframe.matrices.top() * matd3x3::scale(center, vec2d(rx, ry))
			); // apply this scale transform before all else (in local space)
			cairo_set_matrix(context, &mat);
			cairo_arc(context, center.x, center.y, 1.0, 0.0, 6.2831853); // 2 pi, but slightly less
			cairo_close_path(context);

			stackframe.update_transform(); // restore transform
		}


		/// Called to finalize drawing to the current rendering target.
		virtual void _finish_drawing_to_target() = 0;

		/// Creates a new Cairo surface for the given window.
		virtual _details::cairo_object_ref<cairo_surface_t> _create_surface_for_window(window_base&) = 0;
		/// Calls \ref _create_surface_for_window() to create a surface for the given window, sets the
		/// appropriate scaling factor, then creates and returns a Cairo context.
		_details::cairo_object_ref<cairo_t> _create_context_for_window(window_base &wnd, vec2d scaling) {
			auto surface = _create_surface_for_window(wnd);
			cairo_surface_set_device_scale(surface.get(), scaling.x, scaling.y);
			auto result = _details::make_cairo_object_ref_give(cairo_create(surface.get()));
			return result;
		}
		/// Creates a Cairo surface for the window, and listens to specific events to resize the surface as needed.
		void _new_window(window_base &wnd) override {
			std::any &data = _get_window_data(wnd);
			_window_data actual_data;

			// set data
			actual_data.context = _create_context_for_window(wnd, wnd.get_scaling_factor());
			data.emplace<_window_data>(actual_data);
			// resize buffer when the window size has changed
			wnd.size_changed += [this, pwnd = &wnd](ui::window_base::size_changed_info&) {
				auto &data = _window_data::get(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, pwnd->get_scaling_factor());
				pwnd->invalidate_visual();
			};
			// reallocate buffer when the window scaling has changed
			wnd.scaling_factor_changed += [this, pwnd = &wnd](window_base::scaling_factor_changed_info &p) {
				auto &data = _window_data::get(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, p.new_value);
				pwnd->invalidate_visual();
			};
		}
		/// Releases all resources.
		void _delete_window(window_base &w) override {
			_get_window_data(w).reset();
		}
	};
}

#endif
