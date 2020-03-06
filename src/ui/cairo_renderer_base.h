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

		/// Reference-counted handle of a GTK-related object.
		template <typename T> struct gtk_object_ref final :
			public reference_counted_handle<gtk_object_ref<T>, T> {
			friend reference_counted_handle<gtk_object_ref<T>, T>;
		protected:
			/// Adds a reference to the handle if necessary.
			void _do_add_ref() {
				if constexpr (std::is_same_v<T, cairo_t>) {
					cairo_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_surface_t>) {
					cairo_surface_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, cairo_pattern_t>) {
					cairo_pattern_reference(this->_handle);
				} else if constexpr (std::is_same_v<T, PangoAttrList>) {
					pango_attr_list_ref(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) <<
						"add ref operation not implemented for " << demangle(typeid(T).name());
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
				} else if constexpr (std::is_same_v<T, PangoAttrList>) {
					pango_attr_list_unref(this->_handle);
				} else {
					logger::get().log_error(CP_HERE) <<
						"release operation for not implemented for " << demangle(typeid(T).name());
					abort();
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
		template <typename T> struct glib_object_ref final : public reference_counted_handle<glib_object_ref<T>, T> {
			friend reference_counted_handle<glib_object_ref<T>, T>;
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
		template <typename T> inline glib_object_ref<T> make_glib_object_ref_share(T *ptr) {
			glib_object_ref<T> res;
			res.set_share(ptr);
			return res;
		}
		/// Creates a new \ref glib_object_ref, and calls \ref glib_object_ref::set_give() to give the given
		/// pointer to it.
		template <typename T> inline glib_object_ref<T> make_glib_object_ref_give(T *ptr) {
			glib_object_ref<T> res;
			res.set_give(ptr);
			return res;
		}

		/// Converts a component of a color to a \p guint16.
		inline guint16 cast_color_component(double c) {
			return static_cast<guint16>(std::round(c * std::numeric_limits<guint16>::max()));
		}

		/// Converts a \ref horizontal_text_alignment to a \p PangoAlignment.
		inline PangoAlignment cast_horizontal_alignment(horizontal_text_alignment align) {
			switch (align) {
			// FIXME front & rear does not have exactly the same semantics as left & right
			case horizontal_text_alignment::front:
				return PANGO_ALIGN_LEFT;
			case horizontal_text_alignment::rear:
				return PANGO_ALIGN_RIGHT;
			case horizontal_text_alignment::center:
				return PANGO_ALIGN_CENTER;
			}
			return PANGO_ALIGN_LEFT;
		}
		/// Converts a \ref font_style to a \p PangoStyle.
		inline PangoStyle cast_font_style(font_style style) {
			switch (style) {
			case font_style::normal:
				return PANGO_STYLE_NORMAL;
			case font_style::italic:
				return PANGO_STYLE_ITALIC;
			case font_style::oblique:
				return PANGO_STYLE_OBLIQUE;
			}
			return PANGO_STYLE_NORMAL;
		}
		/// Converts a \ref font_weight to a \p PangoWeight.
		inline PangoWeight cast_font_weight(font_weight weight) {
			// TODO
			return PANGO_WEIGHT_NORMAL;
		}
		/// Converts a \ref font_stretch to a \p PangoStretch.
		inline PangoStretch cast_font_stretch(font_stretch stretch) {
			// TODO
			return PANGO_STRETCH_NORMAL;
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

	///
	class formatted_text : public ui::formatted_text {
		friend renderer_base;
	public:
		/// Returns the layout of the text.
		rectd get_layout() const override {
			PangoRectangle layout;
			pango_layout_get_extents(_layout.get(), nullptr, &layout);
			return rectd::from_xywh(
				layout.x / static_cast<double>(PANGO_SCALE),
				layout.y / static_cast<double>(PANGO_SCALE),
				layout.width / static_cast<double>(PANGO_SCALE),
				layout.height / static_cast<double>(PANGO_SCALE)
			);
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

		/// Sets the color of the specified range of text.
		void set_text_color(colord c, std::size_t beg, std::size_t len) override {
			PangoAttribute
				*attr_rgb = pango_attr_foreground_new(
					_details::cast_color_component(c.r),
					_details::cast_color_component(c.g),
					_details::cast_color_component(c.b)
				),
				*attr_a = pango_attr_foreground_alpha_new(_details::cast_color_component(c.a));
			// TODO these indices are in bytes
			attr_rgb->start_index = attr_a->start_index = static_cast<guint>(beg);
			attr_rgb->end_index = attr_a->end_index = static_cast<guint>(beg + len);
			PangoAttrList *list = pango_layout_get_attributes(_layout.get());
			pango_attr_list_insert(list, attr_rgb);
			pango_attr_list_insert(list, attr_a);
		}
		/// Sets the font family of the specified range of text.
		void set_font_family(std::u8string_view, std::size_t, std::size_t) override {
			// TODO
		}
		/// Sets the font size of the specified range of text.
		void set_font_size(double, std::size_t, std::size_t) override {
			// TODO
		}
		/// Sets the font style of the specified range of text.
		void set_font_style(font_style, std::size_t, std::size_t) override {
			// TODO
		}
		/// Sets the font weight of the specified range of text.
		void set_font_weight(font_weight, std::size_t, std::size_t) override {
			// TODO
		}
		/// Sets the font stretch of the specified range of text.
		void set_font_stretch(font_stretch, std::size_t, std::size_t) override {
			// TODO
		}
	protected:
		_details::glib_object_ref<PangoLayout> _layout; ///< The underlying \p PangoLayout object.
	};

	///
	class font : public ui::font {
		friend renderer_base;
		friend font_family;
	public:
		///
		[[nodiscard]] double get_ascent_em() const override {
			// TODO
			return 0.0;
		}
		///
		[[nodiscard]] double get_line_height_em() const override {
			// TODO
			return 0.0;
		}

		///
		[[nodiscard]] double get_character_width_em(codepoint) const override {
			// TODO
			return 0.0;
		}
	};

	///
	class font_family : public ui::font_family {
		friend renderer_base;
	public:
		/// Returns a font in this family matching the given description.
		[[nodiscard]] std::unique_ptr<ui::font> get_matching_font(
			font_style, font_weight, font_stretch
		) const override {
			return std::make_unique<font>();
		}
	};

	///
	class plain_text : public ui::plain_text {
		friend renderer_base;
	public:
		/// Returns the total width of this text clip.
		[[nodiscard]] double get_width() const override {
			return 0.0;
		}

		/// Retrieves information about the character that is below the given horizontal position.
		[[nodiscard]] caret_hit_test_result hit_test(double) const override {
			return caret_hit_test_result();
		}
		/// Returns the space occupied by the character at the given position.
		[[nodiscard]] rectd get_character_placement(std::size_t) const override {
			return rectd();
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
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type) override {
			cairo_matrix_t old_matrix;
			cairo_get_matrix(_context, &old_matrix);

			matd3x3
				scale = matd3x3::scale(vec2d(), radius),
				rotate = matd3x3::rotate_clockwise(vec2d(), rotation);
			matd3x3 inverse_transform =
				matd3x3::scale(vec2d(), vec2d(1.0 / radius.x, 1.0 / radius.y)) * rotate.transpose();
			cairo_matrix_t trans = _details::cast_matrix(rotate * scale), full_trans;
			cairo_matrix_multiply(&full_trans, &old_matrix, &trans);
			cairo_transform(_context, &full_trans);

			vec2d current_point;
			cairo_get_current_point(_context, &current_point.x, &current_point.y);
			vec2d trans_to = inverse_transform.transform_position(to);

			// find the center
			vec2d offset = trans_to - current_point;
			vec2d center = current_point + 0.5 * offset;
			if ((dir == ui::sweep_direction::clockwise) == (type == ui::arc_type::major)) {
				offset = vec2d(-offset.y, offset.x);
			} else {
				offset = vec2d(offset.y, -offset.x);
			}
			double sqrlen = offset.length_sqr();
			offset *= std::sqrt((1.0 - 0.25 * sqrlen) / sqrlen);
			center += offset;

			vec2d off1 = current_point - center, off2 = trans_to - center;
			double angle1 = std::atan2(off1.y, off1.x), angle2 = std::atan2(off2.y, off2.x);
			if (dir == ui::sweep_direction::clockwise) {
				cairo_arc(_context, center.x, center.y, 1.0, angle1, angle2);
			} else {
				cairo_arc_negative(_context, center.x, center.y, 1.0, angle1, angle2);
			}

			cairo_set_matrix(_context, &old_matrix); // restore transformation
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
		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		inline formatted_text &cast_formatted_text(ui::formatted_text &t) {
			auto *rt = dynamic_cast<formatted_text*>(&t);
			assert_true_usage(rt, "invalid formatted text type");
			return *rt;
		}
	}

	/// Platform-independent base class for Cairo renderers.
	///
	/// \todo There are (possibly intended) memory leaks when using this renderer.
	/// \todo Are we using hardware acceleration by implementing it like this? (probably not)
	class renderer_base : public ui::renderer_base {
	public:
		/// Initializes the Pango context.
		renderer_base() {
			_pango_context.set_give(pango_font_map_create_context(pango_cairo_font_map_get_default()));
		}
		/// Calls \p cairo_debug_reset_static_data() to clean up.
		~renderer_base() {
			cairo_debug_reset_static_data();
		}

		/// Creates a new image surface as a render target.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor) override {
			auto resrt = std::make_unique<render_target>();
			auto resbmp = std::make_unique<bitmap>();

			// create surface
			resbmp->_size = size;
			resbmp->_surface = _details::make_gtk_object_ref_give(
				cairo_image_surface_create(
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
			resrt->_context = _details::make_gtk_object_ref_give(cairo_create(resbmp->_surface.get()));
			assert_true_sys(
				cairo_status(resrt->_context.get()) == CAIRO_STATUS_SUCCESS,
				"failed to create cairo context"
			);

			return render_target_data(std::move(resrt), std::move(resbmp));
		}

		/// Loads a \ref bitmap from disk as an image surface.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			auto res = std::make_unique<bitmap>();

			// TODO

			return res;
		}

		/// Creates a new \ref text_format.
		std::unique_ptr<ui::font_family> find_font_family(std::u8string_view family) override {
			// TODO
			return std::make_unique<font_family>();
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
				std::u8string_view(reinterpret_cast<const char*>(text.c_str()), text.size()),
				font, c, size, wrap, halign, valign
			);
		}
		/// Draws the given \ref formatted_text at the given position using the given brush. The position indicates
		/// the top left corner of the layout box.
		void draw_formatted_text(ui::formatted_text &ft, vec2d pos) override {
			auto text = _details::cast_formatted_text(ft);
			cairo_t *context = _render_stack.top().context;

			pango_cairo_update_context(context, _pango_context.get());
			pango_layout_context_changed(text._layout.get());
			cairo_move_to(context, pos.x, pos.y);
			pango_cairo_show_layout(context, text._layout.get());
			cairo_new_path(context);
		}

		///
		std::unique_ptr<ui::plain_text> create_plain_text(std::u8string_view, ui::font&, double) override {
			// TODO
			return std::make_unique<plain_text>();
		}
		///
		std::unique_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint>, ui::font&, double
		) override {
			// TODO
			return std::make_unique<plain_text>();
		}
		///
		void draw_plain_text(ui::plain_text&, vec2d, colord) override {
			// TODO
		}
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
		_details::glib_object_ref<PangoContext> _pango_context; ///< The Pango context.


		/// Draws the current path using the given brush and pen.
		inline static void _draw_path(
			cairo_t *context,
			const ui::generic_brush_parameters &brush,
			const ui::generic_pen_parameters &pen
		) {
			if (_details::gtk_object_ref<cairo_pattern_t> brush_patt = _create_pattern(brush)) {
				cairo_set_source(context, brush_patt.get());
				cairo_fill_preserve(context);
			}
			if (_details::gtk_object_ref<cairo_pattern_t> pen_patt = _create_pattern(pen.brush)) {
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
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::solid_color &brush
		) {
			return _details::make_gtk_object_ref_give(
				cairo_pattern_create_rgba(
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
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::linear_gradient &brush
		) {
			if (brush.gradients) {
				auto patt = _details::make_gtk_object_ref_give(
					cairo_pattern_create_linear(
						brush.from.x, brush.from.y, brush.to.x, brush.to.y
					));
				_add_gradient_stops(patt.get(), *brush.gradients);
				return patt;
			}
			return _details::gtk_object_ref<cairo_pattern_t>();
		}
		/// Creates a new radial gradient pattern.
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::radial_gradient &brush
		) {
			if (brush.gradients) {
				auto patt = _details::make_gtk_object_ref_give(
					cairo_pattern_create_radial(
						brush.center.x, brush.center.y, 0.0, brush.center.x, brush.center.y, brush.radius
					));
				_add_gradient_stops(patt.get(), *brush.gradients);
				return patt;
			}
			return _details::gtk_object_ref<cairo_pattern_t>();
		}
		/// Creates a new bitmap gradient pattern.
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::bitmap_pattern &brush
		) {
			if (brush.image) {
				return _details::make_gtk_object_ref_give(
					cairo_pattern_create_for_surface(
						_details::cast_bitmap(*brush.image)._surface.get()
					));
			}
			return _details::gtk_object_ref<cairo_pattern_t>();
		}
		/// Returns an empty \ref _details::gtk_object_ref.
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
			const ui::brush_parameters::none&
		) {
			return _details::gtk_object_ref<cairo_pattern_t>();
		}
		/// Creates a new \p cairo_pattern_t given the parameters of the brush.
		inline static _details::gtk_object_ref<cairo_pattern_t> _create_pattern(
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
		/// Changes the current path into a rounded rectangle.
		void _make_rounded_rectangle_geometry(rectd rgn, double rx, double ry) {
			_render_target_stackframe &stackframe = _render_stack.top();
			cairo_t *context = stackframe.context;

			if (rx < 1e-4 || ry < 1e-4) { // radius too small or negative, append rectangle directly
				cairo_rectangle(context, rgn.xmin, rgn.ymin, rgn.width(), rgn.height());
				return;
			}

			cairo_matrix_t mat = _details::cast_matrix(
				stackframe.matrices.top() * matd3x3::scale(vec2d(), vec2d(rx, ry))
			); // apply scale transform locally
			// adjust region
			rgn.xmin = rgn.xmin / rx + 1.0;
			rgn.xmax = rgn.xmax / rx - 1.0;
			rgn.ymin = rgn.ymin / ry + 1.0;
			rgn.ymax = rgn.ymax / ry - 1.0;
			rgn.make_valid_average();

			cairo_set_matrix(context, &mat);
			cairo_arc(context, rgn.xmin, rgn.ymin, 1.0, -1.57079632, 0.0);
			cairo_arc(context, rgn.xmax, rgn.ymin, 1.0, 0.0, 1.57079632);
			cairo_arc(context, rgn.xmax, rgn.ymax, 1.0, 1.57079632, 3.14159265);
			cairo_arc(context, rgn.xmin, rgn.ymax, 1.0, 3.14159265, 4.71238898);
			cairo_close_path(context);

			stackframe.update_transform();
		}


		/// Creates a new \ref formatted_text object.
		std::unique_ptr<formatted_text> _create_formatted_text_impl(
			std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment
		) {
			auto result = std::make_unique<formatted_text>();
			result->_layout.set_give(pango_layout_new(_pango_context.get()));

			pango_layout_set_text(result->_layout.get(), text.data(), static_cast<int>(text.size()));

			{ // set font
				PangoFontDescription *desc = pango_font_description_new();
				pango_font_description_set_family(desc, font.family.c_str());
				pango_font_description_set_style(desc, _details::cast_font_style(font.style));
				pango_font_description_set_weight(desc, _details::cast_font_weight(font.weight));
				pango_font_description_set_stretch(desc, _details::cast_font_stretch(font.stretch));
				pango_font_description_set_size(desc, static_cast<gint>(std::round(font.size * PANGO_SCALE)));
				pango_layout_set_font_description(result->_layout.get(), desc);
				pango_font_description_free(desc);
			}

			pango_layout_set_ellipsize(result->_layout.get(), PANGO_ELLIPSIZE_NONE);

			// horizontal wrapping
			if (wrap == wrapping_mode::none) {
				// FIXME alignment won't work for this case
				pango_layout_set_width(result->_layout.get(), -1); // disable wrapping
			} else {
				pango_layout_set_width(result->_layout.get(), PANGO_PIXELS(size.x));
				pango_layout_set_wrap(result->_layout.get(), PANGO_WRAP_WORD_CHAR);
			}
			pango_layout_set_alignment(result->_layout.get(), _details::cast_horizontal_alignment(halign));

			pango_layout_set_height(result->_layout.get(), PANGO_PIXELS(size.y));
			// TODO vertical alignment

			{ // set color
				auto attr_list = _details::make_gtk_object_ref_give(pango_attr_list_new());
				pango_attr_list_insert(attr_list.get(), pango_attr_foreground_new(
					_details::cast_color_component(c.r),
					_details::cast_color_component(c.g),
					_details::cast_color_component(c.b)
				));
				pango_attr_list_insert(
					attr_list.get(), pango_attr_foreground_alpha_new(_details::cast_color_component(c.a))
				);
				pango_layout_set_attributes(result->_layout.get(), attr_list.get());
			}

			return result;
		}


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
