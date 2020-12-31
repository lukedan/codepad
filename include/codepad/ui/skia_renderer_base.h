// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Base class of Skia renderers.

#include <type_traits>
// HACK skia uses result_of_t which is deprecated in C++20
namespace std {
	template <typename> class result_of;
	template <typename F, typename ...Args> class result_of<F(Args...)> {
	public:
		using type = std::invoke_result_t<F, Args...>;
	};
	template <typename T> using result_of_t = typename result_of<T>::type;
}

#include <stack>

#include <skia/gpu/GrContext.h>
#include <skia/core/SkSurface.h>
#include <skia/core/SkCanvas.h>

#include "renderer.h"
#include "pango_harfbuzz_text_context.h"
#include "window.h"

namespace codepad::ui::skia {
	class renderer_base;


	namespace _details {
		/// Converts a \ref colord to a \p SkColor using \p SkColorSetARGB().
		[[nodiscard]] inline SkColor cast_color(colord c) {
			auto u8c = c.convert<unsigned char>();
			return SkColorSetARGB(u8c.a, u8c.r, u8c.g, u8c.b);
		}

		/// Converts a \ref vec2d to a \p SkPoint.
		[[nodiscard]] inline SkPoint cast_point(vec2d p) {
			return SkPoint::Make(static_cast<SkScalar>(p.x), static_cast<SkScalar>(p.y));
		}

		/// Converts a \ref rectd to a \p SkRect.
		[[nodiscard]] inline SkRect cast_rect(rectd r) {
			return SkRect::MakeLTRB(
				static_cast<SkScalar>(r.xmin), static_cast<SkScalar>(r.ymin),
				static_cast<SkScalar>(r.xmax), static_cast<SkScalar>(r.ymax)
			);
		}

		/// Converts a \ref matd3x3 to a \p SkMatrix.
		[[nodiscard]] inline SkMatrix cast_matrix(matd3x3 m) {
			return SkMatrix::MakeAll(
				static_cast<SkScalar>(m[0][0]), static_cast<SkScalar>(m[0][1]), static_cast<SkScalar>(m[0][2]),
				static_cast<SkScalar>(m[1][0]), static_cast<SkScalar>(m[1][1]), static_cast<SkScalar>(m[1][2]),
				static_cast<SkScalar>(m[2][0]), static_cast<SkScalar>(m[2][1]), static_cast<SkScalar>(m[2][2])
			);
		}

		/// Converts a \p SkMatrix back to a \ref matd3x3.
		[[nodiscard]] inline matd3x3 cast_matrix_back(const SkMatrix m) {
			matd3x3 result;
			result[0][0] = m[0];
			result[0][1] = m[1];
			result[0][2] = m[2];
			result[1][0] = m[3];
			result[1][1] = m[4];
			result[1][2] = m[5];
			result[2][0] = m[6];
			result[2][1] = m[7];
			result[2][2] = m[8];
			return result;
		}
	}


	/// Encapsulates a \p SkImage.
	class bitmap : public ui::bitmap {
		friend renderer_base;
	public:
		/// Returns the pixel dimensions of this image divided by \ref _scaling.
		vec2d get_size() const override {
			SkISize size = std::visit([](const auto &ptr) {
				return ptr->imageInfo().dimensions();
				}, _image_or_surface);
			return vec2d(size.fWidth / _scaling.x, size.fHeight / _scaling.y);
		}
	protected:
		vec2d _scaling; ///< The scaling factor of this \ref bitmap.
		/// The underlying \p SkImage or \p SkSurface. This is done because \p SkSurface::makeImageSnapshot() needs
		/// to be called to retrieve an up-to-date \p SkImage.
		std::variant<sk_sp<SkImage>, sk_sp<SkSurface>> _image_or_surface;
	};

	/// Encapsulates a \p SkSurface.
	class render_target : public ui::render_target {
		friend renderer_base;
	protected:
		vec2d _scale; ///< The device scale of this render target.
		sk_sp<SkSurface> _surface; ///< The \p SkSurface to render to.
	};

	///
	class path_geometry_builder : public ui::path_geometry_builder {
	public:
		void close() override {
			// TODO
		}
		void move_to(vec2d pos) override {
			// TODO
		}

		void add_segment(vec2d to) override {
			// TODO
		}
		void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) override {
			// TODO
		}
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction, ui::arc_type) override {
			// TODO
		}
	protected:
		SkPath _path; ///< The path being constructed.
	};


	namespace _details {
		/// Casts a \ref ui::bitmap to a \ref bitmap.
		[[nodiscard]] bitmap &cast_bitmap(ui::bitmap &bmp) {
			auto *b = dynamic_cast<bitmap*>(&bmp);
			assert_true_usage(b, "invalid bitmap type");
			return *b;
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		[[nodiscard]] render_target &cast_render_target(ui::render_target &target) {
			auto *rt = dynamic_cast<render_target*>(&target);
			assert_true_usage(rt, "invalid render target type");
			return *rt;
		}
	}


	/// Base class of the Skia renderer. Contains platform-independent code.
	class renderer_base : public ui::renderer_base {
	public:
		/// Creates a new render target. \ref bitmap::_image_or_surface is set to the SkSurface.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor, colord clear) override;

		std::shared_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			// TODO
			return nullptr;
		}

		/// Invokes \ref pango_harfbuzz::text_context::find_font_family().
		std::shared_ptr<ui::font_family> find_font_family(const std::u8string &family) override {
			return _text_context.find_font_family(family);
		}

		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target &target) override {
			auto &rt = _details::cast_render_target(target);
			_render_target_stackframe &stackframe = _render_stack.emplace(rt._surface->getCanvas(), rt._scale);
		}
		/// Starts drawing to the given \ref window_base and invokes \ref _start_drawing_to_window().
		void begin_drawing(window_base &wnd) override {
			_render_stack.emplace(
				_get_window_data_as<_window_data>(wnd).surface->getCanvas(), wnd.get_scaling_factor(), &wnd
			);
			_start_drawing_to_window(wnd);
		}
		/// Finishes drawing. If the previous render target was a window, invokes \ref _start_drawing_to_window().
		void end_drawing() override {
			if (_render_stack.top().window) {
				_render_stack.top().canvas->flush();
				_finish_drawing_to_window(*_render_stack.top().window);
			}
			_render_stack.pop();

			if (!_render_stack.empty()) {
				if (window_base *wnd = _render_stack.top().window) {
					_start_drawing_to_window(*wnd);
				}
			}
		}

		/// Invokes \p SkCanvas::clear().
		void clear(colord c) override {
			_render_stack.top().canvas->clear(_details::cast_color(c));
		}

		/// Pushes a matrix onto \ref _render_target_stackframe::matrices and invokes
		/// \ref _render_target_stackframe::update_matrix().
		void push_matrix(matd3x3 m) override {
			auto &stackframe = _render_stack.top();
			stackframe.matrices.emplace(_details::cast_matrix(m));
			stackframe.update_matrix();
		}
		/// Similar to \ref push_matrix(), only that the matrix is multiplied by the top element of
		/// \ref _render_stack.
		void push_matrix_mult(matd3x3 m) override {
			auto &stackframe = _render_stack.top();
			stackframe.matrices.emplace(SkMatrix::Concat(stackframe.matrices.top(), _details::cast_matrix(m)));
			stackframe.update_matrix();
		}
		/// Pops a matrix from the stack and invokes \ref _render_target_stackframe::update_matrix().
		void pop_matrix() override {
			auto &stackframe = _render_stack.top();
			stackframe.matrices.pop();
			stackframe.update_matrix();
		}
		/// Returns the current matrix.
		[[nodiscard]] matd3x3 get_matrix() const override {
			return _details::cast_matrix_back(_render_stack.top().matrices.top());
		}


		ui::path_geometry_builder &start_path() override {
			// TODO
			return _path_builder;
		}

		/// Draws an ellipse using \p SkCanvas::drawOval().
		void draw_ellipse(
			vec2d center, double radiusx, double radiusy, const generic_brush &brush, const generic_pen &pen
		) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			SkRect rect = SkRect::MakeLTRB(
				static_cast<SkScalar>(center.x - radiusx),
				static_cast<SkScalar>(center.y - radiusy),
				static_cast<SkScalar>(center.x + radiusx),
				static_cast<SkScalar>(center.y + radiusy)
			);
			if (auto fill = _create_paint(brush)) {
				canvas->drawOval(rect, fill.value());
			}
			if (auto stroke = _create_paint(pen)) {
				canvas->drawOval(rect, stroke.value());
			}
		}
		/// Draws a rectangle using \p SkCanvas::drawRect().
		void draw_rectangle(rectd r, const generic_brush &brush, const generic_pen &pen) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			SkRect skrect = _details::cast_rect(r);
			if (auto fill = _create_paint(brush)) {
				canvas->drawRect(skrect, fill.value());
			}
			if (auto stroke = _create_paint(brush)) {
				canvas->drawRect(skrect, stroke.value());
			}
		}
		/// Draws a rounded rectangle usign \p SkCanvas::drawRoundRect().
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy, const generic_brush &brush, const generic_pen &pen
		) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			SkRect skrect = _details::cast_rect(region);
			SkScalar rx = static_cast<SkScalar>(radiusx), ry = static_cast<SkScalar>(radiusy);
			if (auto fill = _create_paint(brush)) {
				canvas->drawRoundRect(skrect, rx, ry, fill.value());
			}
			if (auto stroke = _create_paint(pen)) {
				canvas->drawRoundRect(skrect, rx, ry, stroke.value());
			}
		}
		///
		void end_and_draw_path(const ui::generic_brush &brush, const ui::generic_pen &pen) override {
			// TODO
		}

		/// Pushes an ellipse-shaped clip using \p SkCanvas::clipRRect().
		void push_ellipse_clip(vec2d center, double radiusx, double radiusy) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			canvas->save();
			canvas->clipRRect(
				SkRRect::MakeOval(SkRect::MakeLTRB(
					static_cast<SkScalar>(center.x - radiusx),
					static_cast<SkScalar>(center.y - radiusy),
					static_cast<SkScalar>(center.x + radiusx),
					static_cast<SkScalar>(center.y + radiusy)
				)),
				true
			);
		}
		/// Pushes a rectangular clip using \p SkCanvas::clipRect().
		void push_rectangle_clip(rectd rgn) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			canvas->save();
			canvas->clipRect(_details::cast_rect(rgn), true);
		}
		/// Pushes a rounded rectangle clip using \p SkCanvas::clipRRect().
		void push_rounded_rectangle_clip(rectd rgn, double radiusx, double radiusy) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			canvas->save();
			canvas->clipRRect(SkRRect::MakeRectXY(
				_details::cast_rect(rgn), static_cast<SkScalar>(radiusx), static_cast<SkScalar>(radiusy)
			));
		}
		///
		void end_and_push_path_clip() override {
			// TODO
		}
		/// Pops a clip and updates the transformation matrix.
		void pop_clip() override {
			_render_target_stackframe &stackframe = _render_stack.top();
			assert_true_usage(stackframe.canvas->getSaveCount() > 0, "push/pop clip mismatch");
			stackframe.canvas->restore();
			stackframe.update_matrix();
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
		void draw_formatted_text(const ui::formatted_text &text, vec2d pos) override {
			// TODO
		}

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
		void draw_plain_text(const ui::plain_text&, vec2d, colord) override {
			// TODO
		}
	protected:
		/// Skia data associated with a \ref window_base.
		struct _window_data {
			sk_sp<SkSurface> surface; ///< The Skia surface.
		};
		/// Stores information about a render target that's being rendered to.
		struct _render_target_stackframe {
			/// Initializes all struct members and invokes \ref update_matrix().
			_render_target_stackframe(SkCanvas *c, vec2d scale, window_base *wnd = nullptr) :
				window(wnd), canvas(c),
				scale_matrix(SkMatrix::MakeScale(
					static_cast<SkScalar>(scale.x), static_cast<SkScalar>(scale.y)
				)) {

				matrices.emplace();
				update_matrix();
			}

			window_base *window = nullptr; ///< A window.
			SkCanvas *canvas = nullptr; ///< The canvas to draw to.
			/// The stack of matrices. Although \p SkCanvas has a \p save() function which saves its state,
			/// unfortunately the two attributes (matrix and clip) are combined when saving, which makes it
			/// impossible to manipulate them independently. Since matrices are much more lightweight than clips, the
			/// internal stack of a \p SkCanvas is used to save clips, and this stack is used to save matrices.
			std::stack<SkMatrix> matrices;
			SkMatrix scale_matrix; ///< The matrix used to enforce device scale.

			/// Updates the matrix of \ref canvas using the product of \ref scale_matrix and the top item in
			/// \ref matrices.
			void update_matrix() {
				canvas->setMatrix(SkMatrix::Concat(scale_matrix, matrices.top()));
			}
		};

		pango_harfbuzz::text_context _text_context; ///< Context for rendering text.
		std::stack<_render_target_stackframe> _render_stack; ///< The stack of render targets.
		sk_sp<GrContext> _skia_context; ///< The Skia graphics context.
		path_geometry_builder _path_builder; ///< Used to build paths.

		/// Returns \p std::nullopt.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const brushes::none&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::solid_color.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const brushes::solid_color&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::linear_gradient.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const brushes::linear_gradient&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::radial_gradient.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const brushes::radial_gradient&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::bitmap_pattern.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const brushes::bitmap_pattern&, const matd3x3&);

		/// Creates a \p SkPaint from a \ref generic_brush.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const generic_brush&);
		/// Creates a \p SkPaint from a \ref generic_pen.
		[[nodiscard]] static std::optional<SkPaint> _create_paint(const generic_pen&);


		/// Called to start drawing to a window in a platform-specific way.
		virtual void _start_drawing_to_window(window_base&) = 0;
		/// Called to finalize drawing to a window in a platform-specific way.
		virtual void _finish_drawing_to_window(window_base&) = 0;
	};
}
