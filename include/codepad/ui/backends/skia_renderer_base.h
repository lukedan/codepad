// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Base class of Skia renderers.

#include <type_traits>
#include <stack>

#include <skia/core/SkSurface.h>
#include <skia/core/SkCanvas.h>
#include <skia/core/SkTypeface.h>
#include <skia/core/SkFont.h>
#include <skia/core/SkTextBlob.h>
#include <skia/gpu/GrDirectContext.h>

#include "codepad/ui/renderer.h"
#include "codepad/ui/window.h"
#include "pango_harfbuzz_text_engine.h"

namespace codepad::ui::skia {
	class renderer_base;


	namespace _details {
		/// Converts a \ref colord to a \p SkColor using \p SkColorSetARGB().
		[[nodiscard]] inline SkColor cast_color(colord c) {
			auto u8c = c.convert<unsigned char>();
			return SkColorSetARGB(u8c.a, u8c.r, u8c.g, u8c.b);
		}

		/// Converts a \ref colord to a \p SkColor4f.
		[[nodiscard]] inline SkColor4f cast_colorf(colord c) {
			SkColor4f result;
			result.fR = static_cast<float>(c.r);
			result.fG = static_cast<float>(c.g);
			result.fB = static_cast<float>(c.b);
			result.fA = static_cast<float>(c.a);
			return result;
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

	/// Wraps around a \ref pango_harfbuzz::font_data and a \p SkTypeFace.
	class font : public ui::font {
		friend renderer_base;
	public:
		/// Loads the font from the given font file at the given index.
		font(pango_harfbuzz::text_engine &engine, const char *file, int index) {
			_skia_font = SkTypeface::MakeFromFile(file, index);
			_data = engine.create_font_for_file(file, index);
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
		sk_sp<SkTypeface> _skia_font; ///< The Skia font face.
		pango_harfbuzz::font_data _data; ///< Font data.
	};

	/// Wraps around a \ref pango_harfbuzz::font_family_data.
	class font_family : public ui::font_family {
		friend renderer_base;
	public:
		/// Initializes \ref _data and \ref _engine.
		font_family(pango_harfbuzz::text_engine &eng, pango_harfbuzz::font_family_data data) :
			ui::font_family(), _data(std::move(data)), _engine(&eng) {
		}

		/// Searches in the cache for a matching font, and if none is found, creates a new \ref font and returns it.
		[[nodiscard]] std::shared_ptr<ui::font> get_matching_font(
			font_style style, font_weight weight, font_stretch stretch
		) const override {
			return _get_matching_font_impl(*_engine, _data, style, weight, stretch);
		}
	protected:
		pango_harfbuzz::font_family_data _data; ///< The font family data.
		pango_harfbuzz::text_engine *_engine = nullptr; ///< The text engine that created this object.

		/// Finds and returns the matching font in the given font family, caching the result.
		[[nodiscard]] static std::shared_ptr<font> _get_matching_font_impl(
			pango_harfbuzz::text_engine&, pango_harfbuzz::font_family_data, font_style, font_weight, font_stretch
		);
	};

	/// Wraps around a \ref pango_harfbuzz::plain_text_data, its associated \p SkFont, and optionally a cached
	/// \p SkTextBlob.
	class plain_text : public ui::plain_text {
		friend renderer_base;
	public:
		/// Initializes \ref data and \ref _font.
		plain_text(pango_harfbuzz::plain_text_data data, SkFont fnt) :
			_data(std::move(data)), _font(std::move(fnt)) {

			fnt.setSubpixel(true);
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
		pango_harfbuzz::plain_text_data _data; ///< Plain text data.
		SkFont _font; ///< The Skia font.
		/// Cached \p SkTextBlob used only for rendering. This is initialized when this object is first rendered.
		mutable sk_sp<SkTextBlob> _cached_text;
	};

	/// Wraps around a \ref pango_harfbuzz::formatted_text_data and optionally a \p SkTextBlob.
	class formatted_text : public ui::formatted_text {
		friend renderer_base;
	public:
		/// Initializes \ref _data.
		explicit formatted_text(pango_harfbuzz::formatted_text_data data) : _data(std::move(data)) {
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
		[[nodiscard]] caret_hit_test_result hit_test(vec2d p) const override {
			return _data.hit_test(p);
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
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_layout_size() and resets \ref _cached_text.
		void set_layout_size(vec2d sz) override {
			_data.set_layout_size(sz);
			_cached_text.reset();
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_horizontal_alignment().
		[[nodiscard]] horizontal_text_alignment get_horizontal_alignment() const override {
			return _data.get_horizontal_alignment();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_horizontal_alignment() and resets \ref _cached_text.
		void set_horizontal_alignment(horizontal_text_alignment align) override {
			_data.set_horizontal_alignment(align);
			_cached_text.reset();
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_vertical_alignment().
		[[nodiscard]] vertical_text_alignment get_vertical_alignment() const override {
			return _data.get_vertical_alignment();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_vertical_alignment() and resets \ref _cached_text.
		void set_vertical_alignment(vertical_text_alignment align) override {
			_data.set_vertical_alignment(align);
			_cached_text.reset();
		}
		/// Returns \ref pango_harfbuzz::formatted_text_data::get_wrapping_mode().
		[[nodiscard]] wrapping_mode get_wrapping_mode() const override {
			return _data.get_wrapping_mode();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_wrapping_mode() and resets \ref _cached_text.
		void set_wrapping_mode(wrapping_mode wrap) override {
			_data.set_wrapping_mode(wrap);
			_cached_text.reset();
		}

		/// Calls \ref pango_harfbuzz::formatted_text_data::set_text_color() and resets \ref _cached_text.
		void set_text_color(colord c, std::size_t beg, std::size_t len) override {
			_data.set_text_color(c, beg, len);
			_cached_text.reset();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_family() and resets \ref _cached_text.
		void set_font_family(const std::u8string &family, std::size_t beg, std::size_t len) override {
			_data.set_font_family(family, beg, len);
			_cached_text.reset();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_size() and resets \ref _cached_text.
		void set_font_size(double size, std::size_t beg, std::size_t len) override {
			_data.set_font_size(size, beg, len);
			_cached_text.reset();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_style() and resets \ref _cached_text.
		void set_font_style(font_style style, std::size_t beg, std::size_t len) override {
			_data.set_font_style(style, beg, len);
			_cached_text.reset();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_weight() and resets \ref _cached_text.
		void set_font_weight(font_weight weight, std::size_t beg, std::size_t len) override {
			_data.set_font_weight(weight, beg, len);
			_cached_text.reset();
		}
		/// Calls \ref pango_harfbuzz::formatted_text_data::set_font_stretch() and resets \ref _cached_text.
		void set_font_stretch(font_stretch stretch, std::size_t beg, std::size_t len) override {
			_data.set_font_stretch(stretch, beg, len);
			_cached_text.reset();
		}
	protected:
		pango_harfbuzz::formatted_text_data _data; ///< Formatted text data from the text engine.
		/// Cached \p SkTextBlob used only for rendering. This is initialized when this object is first rendered.
		mutable sk_sp<SkTextBlob> _cached_text;
	};

	/// Contains a \p SkPath that is constructed by this builder.
	class path_geometry_builder : public ui::path_geometry_builder {
		friend renderer_base;
	public:
		/// Calls \p SkPath::close().
		void close() override {
			_path.close();
		}
		/// Calls \p SkPath::moveTo().
		void move_to(vec2d pos) override {
			_path.moveTo(_details::cast_point(pos));
		}

		/// Calls \p SkPath::lineTo().
		void add_segment(vec2d to) override {
			_path.lineTo(_details::cast_point(to));
		}
		/// Calls \p SkPath::cubicTo().
		void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) override {
			_path.cubicTo(_details::cast_point(control1), _details::cast_point(control2), _details::cast_point(to));
		}
		/// Calls \p SkPath::arcTo().
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type) override {
			_path.arcTo(
				_details::cast_point(radius), static_cast<SkScalar>(rotation * 180.0 / 3.14159265),
				type == ui::arc_type::major ? SkPath::kLarge_ArcSize : SkPath::kSmall_ArcSize,
				dir == ui::sweep_direction::clockwise ? SkPathDirection::kCW : SkPathDirection::kCCW,
				_details::cast_point(to)
			);
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

		/// Casts a \ref ui::font to a \ref font.
		[[nodiscard]] font &cast_font(ui::font &target) {
			auto *rt = dynamic_cast<font*>(&target);
			assert_true_usage(rt, "invalid font type");
			return *rt;
		}

		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		[[nodiscard]] const formatted_text &cast_formatted_text(const ui::formatted_text &target) {
			auto *rt = dynamic_cast<const formatted_text*>(&target);
			assert_true_usage(rt, "invalid formatted_text type");
			return *rt;
		}

		/// Casts a \ref ui::plain_text to a \ref plain_text.
		[[nodiscard]] const plain_text &cast_plain_text(const ui::plain_text &target) {
			auto *rt = dynamic_cast<const plain_text*>(&target);
			assert_true_usage(rt, "invalid plain_text type");
			return *rt;
		}
	}


	/// Base class of the Skia renderer. Contains platform-independent code.
	class renderer_base : public ui::renderer_base {
	public:
		/// Initializes \ref _color_space and \ref _text_engine.
		explicit renderer_base(PangoFontMap *font_map) : ui::renderer_base(), _text_engine(font_map) {
			// TODO this color space causes the image to look washed out
			_color_space = SkColorSpace::MakeSRGB();
		}

		/// Creates a new render target. \ref bitmap::_image_or_surface is set to the SkSurface.
		render_target_data create_render_target(vec2d size, vec2d scaling_factor, colord clear) override;

		std::shared_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			// TODO
			return nullptr;
		}

		/// Invokes \ref pango_harfbuzz::text_engine::find_font_family().
		std::shared_ptr<ui::font_family> find_font_family(const std::u8string &family) override {
			return std::make_shared<font_family>(_text_engine, _text_engine.find_font_family(family.c_str()));
		}

		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target &target) override {
			auto &rt = _details::cast_render_target(target);
			_render_stack.emplace(rt._surface.get(), rt._scale);
		}
		/// Starts drawing to the given \ref window and invokes \ref _start_drawing_to_window().
		void begin_drawing(window &wnd) override {
			_render_stack.emplace(_get_surface_for_window(wnd), wnd.get_scaling_factor(), &wnd);
			_start_drawing_to_window(wnd);
		}
		/// Finishes drawing. If the previous render target was a window, invokes \ref _start_drawing_to_window().
		void end_drawing() override {
			if (_render_stack.top().wnd) {
				_render_stack.top().surface->flushAndSubmit();
				_finish_drawing_to_window(*_render_stack.top().wnd);
			}
			_render_stack.pop();

			if (!_render_stack.empty()) {
				if (window *wnd = _render_stack.top().wnd) {
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

		/// Resets \ref _path_builder and returns it.
		ui::path_geometry_builder &start_path() override {
			_path_builder._path.reset();
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
			if (r.contains_nan()) {
				return; // skia is very perculiar with geometry that contains nan
			}
			SkCanvas *canvas = _render_stack.top().canvas;
			SkRect skrect = _details::cast_rect(r);
			if (auto fill = _create_paint(brush)) {
				canvas->drawRect(skrect, fill.value());
			}
			if (auto stroke = _create_paint(pen)) {
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
		/// Draws the path in \ref _path_builder using \p SkCanvas::drawPath().
		void end_and_draw_path(const ui::generic_brush &brush, const ui::generic_pen &pen) override {
			SkCanvas *canvas = _render_stack.top().canvas;
			if (auto fill = _create_paint(brush)) {
				canvas->drawPath(_path_builder._path, fill.value());
			}
			if (auto stroke = _create_paint(pen)) {
				canvas->drawPath(_path_builder._path, stroke.value());
			}
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
		/// Pushes a path clip using \p SkCanvas::clipPath() with the current path in \ref _path_builder.
		void end_and_push_path_clip() override {
			SkCanvas *canvas = _render_stack.top().canvas;
			canvas->save();
			canvas->clipPath(_path_builder._path, true);
		}
		/// Pops a clip and updates the transformation matrix.
		void pop_clip() override {
			_render_target_stackframe &stackframe = _render_stack.top();
			assert_true_usage(stackframe.canvas->getSaveCount() > 0, "push/pop clip mismatch");
			stackframe.canvas->restore();
			stackframe.update_matrix();
		}

		/// Invokes \ref pango_harfbuzz::text_engine::create_formatted_text().
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::u8string_view text, const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return std::make_shared<formatted_text>(_text_engine.create_formatted_text(
				text, font, c, size, wrap, halign, valign
			));
		}
		/// \overload
		std::shared_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint> utf32,
			const font_parameters &font, colord c, vec2d size, wrapping_mode wrap,
			horizontal_text_alignment halign, vertical_text_alignment valign
		) override {
			return std::make_shared<formatted_text>(_text_engine.create_formatted_text(
				utf32, font, c, size, wrap, halign, valign
			));
		}
		/// Draws the given \ref formatted_text at the given position.
		void draw_formatted_text(const ui::formatted_text&, vec2d) override;

		/// Invokes \ref pango_harfbuzz::text_engine::create_plain_text().
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::u8string_view text, ui::font &generic_fnt, double size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			// TODO SkFont expects the size in points, but we have the size in device-independent pixels
			//      it can't be this simple right?
			return std::make_shared<plain_text>(
				_text_engine.create_plain_text(text, fnt._data, size),
				SkFont(fnt._skia_font, static_cast<SkScalar>(size * 1.33))
			);
		}
		/// \overload
		std::shared_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint> text, ui::font &generic_fnt, double size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			// TODO SkFont expects the size in points, but we have the size in device-independent pixels
			return std::make_shared<plain_text>(
				_text_engine.create_plain_text(text, fnt._data, size),
				SkFont(fnt._skia_font, static_cast<SkScalar>(size * 1.33))
			);
		}
		/// Invokes \ref pango_harfbuzz::text_engine::create_plain_text_fast().
		std::shared_ptr<ui::plain_text> create_plain_text_fast(
			std::basic_string_view<codepoint> text, ui::font &generic_fnt, double size
		) override {
			auto &fnt = _details::cast_font(generic_fnt);
			// TODO SkFont expects the size in points, but we have the size in device-independent pixels
			return std::make_shared<plain_text>(
				_text_engine.create_plain_text_fast(text, fnt._data, size),
				SkFont(fnt._skia_font, static_cast<SkScalar>(size * 1.33))
			);
		}
		/// Renders the given fragment of text.
		void draw_plain_text(const ui::plain_text&, vec2d, colord) override;
	protected:
		/// Stores information about a render target that's being rendered to.
		struct _render_target_stackframe {
			/// Initializes all struct members and invokes \ref update_matrix().
			_render_target_stackframe(SkSurface *surf, vec2d scale, window *w = nullptr) :
				wnd(w), surface(surf), canvas(surf->getCanvas()),
				scale_matrix(SkMatrix::Scale(static_cast<SkScalar>(scale.x), static_cast<SkScalar>(scale.y))) {

				matrices.emplace();
				update_matrix();
			}

			window *wnd = nullptr; ///< A window.
			SkSurface *surface = nullptr; ///< The Skia surface.
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

		pango_harfbuzz::text_engine _text_engine; ///< Engine for text layout.
		std::stack<_render_target_stackframe> _render_stack; ///< The stack of render targets.
		sk_sp<GrDirectContext> _skia_context; ///< The Skia graphics context.
		sk_sp<SkColorSpace> _color_space; ///< The color space for all colors.
		path_geometry_builder _path_builder; ///< Used to build paths.

		/// Returns \p std::nullopt.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const brushes::none&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::solid_color.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const brushes::solid_color&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::linear_gradient.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const brushes::linear_gradient&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::radial_gradient.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const brushes::radial_gradient&, const matd3x3&);
		/// Creates a \p SkPaint from a \ref brushes::bitmap_pattern.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const brushes::bitmap_pattern&, const matd3x3&);

		/// Creates a \p SkPaint from a \ref generic_brush.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const generic_brush&);
		/// Creates a \p SkPaint from a \ref generic_pen.
		[[nodiscard]] std::optional<SkPaint> _create_paint(const generic_pen&);


		// TODO use proper pixel size for the size of the surface
		/// Creates a surface for the given window. This is mainly used by derived classes to handle window events.
		[[nodiscard]] sk_sp<SkSurface> _create_surface_for_window(ui::window &wnd, vec2d scaling);

		/// Returns the surface associated with the given window.
		[[nodiscard]] virtual SkSurface *_get_surface_for_window(ui::window&) const = 0;
		/// Called to start drawing to a window in a platform-specific way.
		virtual void _start_drawing_to_window(window&) = 0;
		/// Called to finalize drawing to a window in a platform-specific way.
		virtual void _finish_drawing_to_window(window&) = 0;
	};
}
