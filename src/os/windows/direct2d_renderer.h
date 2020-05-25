// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The Direct2D renderer backend.

#include <stack>

#include <d2d1_1.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwrite.h>
#include <dwrite_3.h>

#include "../../ui/renderer.h"
#include "window.h"
#include "misc.h"

namespace codepad::os::direct2d {
	class render_target;
	class font_family;
	class renderer;

	/// A Direct2D bitmap.
	class bitmap : public ui::bitmap {
		friend render_target;
		friend renderer;
	public:
		/// Returns the size of the bitmap.
		vec2d get_size() const override {
			D2D1_SIZE_F sz = _bitmap->GetSize();
			return vec2d(sz.width, sz.height);
		}
	protected:
		_details::com_wrapper<ID2D1Bitmap1> _bitmap; ///< The bitmap.
	};

	/// A Direct2D bitmap render target.
	class render_target : public ui::render_target {
		friend renderer;
	protected:
		_details::com_wrapper<ID2D1Bitmap1> _bitmap; ///< The bitmap used with Direct2D.
		_details::com_wrapper<ID3D11Texture2D> _texture; ///< The texture.
	};

	/// Wrapper around an \p IDWriteTextLayout.
	class formatted_text : public ui::formatted_text {
		friend renderer;
	public:
		/// Default constructor.
		formatted_text() = default;

		/// Returns the layout of the text.
		rectd get_layout() const override;
		/// Returns the metrics of each line.
		std::vector<ui::line_metrics> get_line_metrics() const override;

		/// Invokes \p IDWriteTextLayout::HitTestPoint().
		ui::caret_hit_test_result hit_test(vec2d) const override;
		/// Invokes \p IDWriteTextLayout::HitTestTextPosition().
		rectd get_character_placement(std::size_t) const override;

		/// Calls \p IDWriteTextLayout::SetDrawingEffect().
		void set_text_color(colord, std::size_t, std::size_t) override;
		/// Calls \p IDWriteTextLayout::SetFontFamilyName().
		void set_font_family(const std::u8string&, std::size_t beg, std::size_t len) override;
		/// Calls \p IDWriteTextLayout::SetFontSize().
		void set_font_size(double, std::size_t beg, std::size_t len) override;
		/// Calls \p IDWriteTextLayout::SetFontStyle().
		void set_font_style(ui::font_style, std::size_t beg, std::size_t len) override;
		/// Calls \p IDWriteTextLayout::SetFontStyle().
		void set_font_weight(ui::font_weight, std::size_t beg, std::size_t len) override;
		/// Calls \p IDWriteTextLayout::SetFontStretch().
		void set_font_stretch(ui::font_stretch, std::size_t beg, std::size_t len) override;
	protected:
		/// Initializes \ref _rend.
		explicit formatted_text(renderer &r) : _rend(&r) {
		}

		_details::com_wrapper<IDWriteTextLayout> _text; ///< The \p IDWriteTextLayout handle.
		renderer *_rend = nullptr; ///< The renderer.
	};

	/// Encapsules a \p IDWriteFontFace.
	class font : public ui::font {
		friend renderer;
		friend font_family;
	public:
		/// Returns \p DWRITE_FONT_METRICS::ascent.
		double get_ascent_em() const override {
			return _metrics.ascent / static_cast<double>(_metrics.designUnitsPerEm);
		}
		/// Returns the sum of \p DWRITE_FONT_METRICS::ascent, \p DWRITE_FONT_METRICS::descent, and
		/// \p DWRITE_FONT_METRICS::lineGap.
		double get_line_height_em() const override {
			return
				(_metrics.ascent + _metrics.descent + _metrics.lineGap) /
				static_cast<double>(_metrics.designUnitsPerEm);
		}

		/// Calls \p IDWriteFont::HasCharacter to determine if the font contains a glyph for the given character.
		bool has_character(codepoint cp) const override {
			BOOL result;
			_details::com_check(_font->HasCharacter(cp, &result));
			return result;
		}

		/// Returns the width of the character.
		double get_character_width_em(codepoint cp) const override {
			UINT32 cp_u32 = cp;
			UINT16 glyph;
			DWRITE_GLYPH_METRICS gmetrics;
			_details::com_check(_font_face->GetGlyphIndices(&cp_u32, 1, &glyph));
			_details::com_check(_font_face->GetDesignGlyphMetrics(&glyph, 1, &gmetrics, false));
			return gmetrics.advanceWidth / static_cast<double>(_metrics.designUnitsPerEm);
		}
	protected:
		DWRITE_FONT_METRICS _metrics; ///< The metrics of this font.
		_details::com_wrapper<IDWriteFont> _font; ///< The \p IDWriteFont.
		_details::com_wrapper<IDWriteFontFace> _font_face; ///< The \p IDWriteFontFace.
	};

	/// Encapsules a \p IDWriteFontFamily.
	class font_family : public ui::font_family {
		friend renderer;
	public:
		/// Returns the first font matching the given descriptions.
		std::unique_ptr<ui::font> get_matching_font(
			ui::font_style, ui::font_weight, ui::font_stretch
		) const override;
	protected:
		_details::com_wrapper<IDWriteFontFamily> _family; ///< The \p IDWriteFontFamily.
	};

	/// Stores a piece of text analyzed using \p IDWriteTextAnalyzer.
	class plain_text : public ui::plain_text {
		friend renderer;
	public:
		/// Returns the width of this piece of text.
		double get_width() const override {
			_maybe_calculate_glyph_positions();
			return _cached_glyph_positions.back();
		}

		/// Performs hit testing.
		ui::caret_hit_test_result hit_test(double) const override;
		/// Returns the position of the given character.
		rectd get_character_placement(std::size_t) const override;
	protected:
		/// Fills \ref _cached_glyph_positions if necessary.
		void _maybe_calculate_glyph_positions() const;
		/// If necessary, calculates \ref _cached_glyph_to_char_mapping and
		/// \ref _cached_glyph_to_char_mapping_starting.
		void _maybe_calculate_glyph_backmapping() const;

		/// The positions of the left borders of all glyphs. This array has one additional element at the back - the
		/// total width of this clip.
		mutable std::vector<double> _cached_glyph_positions;
		/// A one-to-many mapping from glyphs to character indices. This should be used with
		/// \ref _cached_glyph_to_char_mapping_starting.
		mutable std::vector<std::size_t> _cached_glyph_to_char_mapping;
		/// The starting index in \ref _cached_glyph_to_char_mapping of characters corresponding to each glyph. This
		/// array has one additional element at the back that is the total number of characters.
		mutable std::vector<std::size_t> _cached_glyph_to_char_mapping_starting;

		std::unique_ptr<UINT16[]> _cluster_map; ///< Mapping from character ranges to glyph ranges.
		std::unique_ptr<UINT16[]> _glyphs; ///< The list of glyph indices.
		std::unique_ptr<FLOAT[]> _glyph_advances; ///< The horizontal advancement width of each glyph.
		std::unique_ptr<DWRITE_GLYPH_OFFSET[]> _glyph_offsets; ///< The offset of each glyph.
		_details::com_wrapper<IDWriteFontFace> _font_face; ///< Cached font face.
		double _font_size = 0.0; ///< Cached font size.
		std::size_t
			_char_count = 0, ///< The total number of characters.
			_glyph_count = 0; ///< The actual number of glyphs.

		/// Returns the placement of the specified character.
		rectd _get_character_placement_impl(std::size_t glyphid, std::size_t charoffset) const;
	};

	/// Encapsules a \p ID2D1PathGeometry and a \p ID2D1GeometrySink.
	class path_geometry_builder : public ui::path_geometry_builder {
		friend renderer;
	public:
		/// Closes and ends the current sub-path.
		void close() override;
		/// Moves to the given position and starts a new sub-path.
		void move_to(vec2d) override;

		/// Adds a segment from the current position to the given position.
		void add_segment(vec2d) override;
		/// Adds a cubic bezier segment.
		void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) override;
		/// Adds an arc (part of a circle).
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction, ui::arc_type) override;
	protected:
		_details::com_wrapper<ID2D1PathGeometry> _geom; ///< The geometry.
		_details::com_wrapper<ID2D1GeometrySink> _sink; ///< The actual builder.
		D2D1_POINT_2F _last_point; ///< The last destination point.
		bool _stroking = false; ///< Indicates whether strokes are being drawn.

		/// Initializes \ref _geom.
		void _start(ID2D1Factory *factory);
		/// Ends the path, releases \ref _sink, and returns the geometry.
		_details::com_wrapper<ID2D1PathGeometry> _end();

		/// Called before any actual stroke is added to ensure that \ref _stroking is \p true.
		void _on_stroke();
	};


	/// The Direct2D renderer backend.
	class renderer : public ui::renderer_base {
		friend formatted_text;
	public:
		constexpr static DXGI_FORMAT pixel_format = DXGI_FORMAT_B8G8R8A8_UNORM; ///< The default pixel format.

		/// Initializes \ref _d2d_factory.
		renderer();

		/// Creates a render target of the given size.
		ui::render_target_data create_render_target(vec2d size, vec2d scaling_factor) override;

		/// Loads a \ref bitmap from disk.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path&, vec2d scaling_factor) override;

		/// Creates a \p IDWriteTextFormat.
		std::unique_ptr<ui::font_family> find_font_family(const std::u8string&) override;

		/// Starts drawing to the given window.
		void begin_drawing(ui::window_base&) override;
		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target&) override;
		/// Finishes drawing.
		void end_drawing() override;

		/// Pushes a matrix onto the stack.
		void push_matrix(matd3x3 m) override {
			_render_stack.top().matrices.push(m);
			_update_transform();
		}
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack.
		void push_matrix_mult(matd3x3 m) override {
			_render_target_stackframe &frame = _render_stack.top();
			frame.matrices.push(m * frame.matrices.top());
			_update_transform();
		}
		/// Pops a matrix from the stack.
		void pop_matrix() override {
			_render_stack.top().matrices.pop();
			_update_transform();
		}
		/// Returns the current transformation matrix.
		matd3x3 get_matrix() const override {
			return _render_stack.top().matrices.top();
		}

		/// Clears the current surface using the given color.
		void clear(colord) override;

		/// Calls \ref path_geometry_builder::_start() and returns \ref _path_builder.
		ui::path_geometry_builder &start_path() override {
			_path_builder._start(_d2d_factory.get());
			return _path_builder;
		}

		/// Draws a \p ID2D1EllipseGeometry.
		void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override;
		/// Draws a \p ID2D1RectangleGeometry.
		void draw_rectangle(
			rectd, const ui::generic_brush_parameters&, const ui::generic_pen_parameters&
		) override;
		/// Draws a \p ID2D1RoundedRectangleGeometry.
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush_parameters&, const ui::generic_pen_parameters&
		) override;
		/// Calls \ref path_geometry_builder::_end() and draws the returned geometry.
		void end_and_draw_path(
			const ui::generic_brush_parameters&, const ui::generic_pen_parameters&
		) override;

		/// Creates a \p ID2D1EllipseGeometry and pushes it as a clip.
		void push_ellipse_clip(vec2d center, double radiusx, double radiusy) override;
		/// Creates a \p ID2D1RectangleGeometry and pushes it as a clip.
		void push_rectangle_clip(rectd) override;
		/// Creates a \p ID2D1RoundedRectangleGeometry and pushes it as a clip.
		void push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) override;
		/// Calls \ref path_geometry_builder::_end() and pushes the returned geometry as a clip.
		void end_and_push_path_clip() override;
		/// Calls \p ID2D1DeviceContext::PopLayer() to pop the most recently pushed layer.
		void pop_clip() override;

		/// Calls \ref _create_formatted_text_impl().
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			std::u8string_view, const ui::font_parameters&, colord, vec2d maxsize, ui::wrapping_mode,
			ui::horizontal_text_alignment, ui::vertical_text_alignment
		) override;
		/// Calls \ref _create_formatted_text_impl().
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint>, const ui::font_parameters&, colord,
			vec2d maxsize, ui::wrapping_mode,
			ui::horizontal_text_alignment, ui::vertical_text_alignment
		) override;
		/// Calls \p ID2D1DeviceContext::DrawTextLayout to render the given \ref formatted_text.
		void draw_formatted_text(const ui::formatted_text&, vec2d topleft) override;

		/// Calls \ref _create_plain_text_impl to shape the given text.
		std::unique_ptr<ui::plain_text> create_plain_text(
			std::u8string_view, ui::font&, double size
		) override;
		/// Calls \ref _create_plain_text_impl to shape the given text.
		std::unique_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint>, ui::font&, double size
		) override;
		/// Calls \p ID2D1DeviceContext::DrawGlyphRun to render the text clip. Any pushed layer will disable subpixel
		/// antialiasing for this text.
		void draw_plain_text(const ui::plain_text&, vec2d pos, colord color) override;
	protected:
		/// Stores window-specific data.
		struct _window_data {
			_details::com_wrapper<IDXGISwapChain1> swap_chain; ///< The DXGI swap chain.
			_details::com_wrapper<ID2D1Bitmap1> target; ///< The target bitmap.

			/// Returns the \ref _window_data associated with the given window.
			static _window_data &get(ui::window_base&);
		};
		/// Stores information about a currently active render target.
		struct _render_target_stackframe {
			/// Initializes \ref target, and pushes an identity matrix onto the stack.
			explicit _render_target_stackframe(ID2D1Bitmap1 *t) : target(t) {
				matrices.emplace(matd3x3::identity());
			}

			std::stack<matd3x3> matrices; ///< The stack of matrices.
			/// The render target. Here we're using raw pointers to skip a few \p AddRef() and \p Release() calls
			/// since the target must be alive during the entire duration of rendering (if it doesn't, then it's the
			/// user's fault and we'll just let it crash).
			ID2D1Bitmap1 *target = nullptr;
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of render targets.
		std::set<IDXGISwapChain*> _present_chains; ///< The DXGI swap chains to call \p Present() on.
		path_geometry_builder _path_builder; ///< The path builder.

		_details::com_wrapper<ID2D1Factory5> _d2d_factory; ///< The Direct2D factory.
		_details::com_wrapper<ID2D1Device4> _d2d_device; ///< The Direct2D device.
		_details::com_wrapper<ID2D1DeviceContext4> _d2d_device_context; ///< The Direct2D device context.
		_details::com_wrapper<ID3D11Device> _d3d_device; ///< The Direct3D device.
		_details::com_wrapper<IDXGIDevice> _dxgi_device; ///< The DXGI device.
		_details::com_wrapper<IDXGIAdapter> _dxgi_adapter; ///< The DXGI adapter.
		_details::com_wrapper<IDWriteFactory4> _dwrite_factory; ///< The DirectWrite factory.

		_details::com_wrapper<ID2D1SolidColorBrush> _text_brush; ///< The brush used for rendering text.
		/// The text analyzer used for fast text rendering.
		_details::com_wrapper<IDWriteTextAnalyzer> _dwrite_text_analyzer;


		/// Starts drawing to a \p ID2D1RenderTarget.
		void _begin_draw_impl(ID2D1Bitmap1*, vec2d dpi);
		/// Updates the transform of \ref _d2d_device_context.
		void _update_transform();
		/// Draws the given geometry.
		void _draw_geometry(
			_details::com_wrapper<ID2D1Geometry>,
			const ui::generic_brush_parameters&,
			const ui::generic_pen_parameters&
		);
		/// Calls \p ID2D1DeviceContext::PushLayer() to push a layer with the specified geometry as its clip.
		void _push_layer(_details::com_wrapper<ID2D1Geometry> clip);


		/// Creates a brush based on \ref ui::brush_parameters::solid_color.
		_details::com_wrapper<ID2D1SolidColorBrush> _create_brush(const ui::brush_parameters::solid_color&);
		/// Creates a \p ID2D1GradientStopCollection.
		_details::com_wrapper<ID2D1GradientStopCollection> _create_gradient_stop_collection(
			const std::vector<ui::gradient_stop>&
		);
		/// Creates a brush based on \ref ui::brush_parameters::linear_gradient.
		_details::com_wrapper<ID2D1LinearGradientBrush> _create_brush(
			const ui::brush_parameters::linear_gradient&
		);
		/// Creates a brush based on \ref ui::brush_parameters::radial_gradient.
		_details::com_wrapper<ID2D1RadialGradientBrush> _create_brush(
			const ui::brush_parameters::radial_gradient&
		);
		/// Creates a brush based on \ref ui::brush_parameters::bitmap_pattern.
		_details::com_wrapper<ID2D1BitmapBrush> _create_brush(
			const ui::brush_parameters::bitmap_pattern&
		);
		/// Returns an empty \ref _details::com_wrapper.
		_details::com_wrapper<ID2D1Brush> _create_brush(const ui::brush_parameters::none&);
		/// Creates a \p ID2D1Brush from the given \ref ui::generic_brush_parameters specification.
		_details::com_wrapper<ID2D1Brush> _create_brush(const ui::generic_brush_parameters&);


		/// Creates an \p IDWriteTextLayout.
		std::unique_ptr<formatted_text> _create_formatted_text_impl(
			std::basic_string_view<WCHAR>, const ui::font_parameters&, colord,
			vec2d maxsize, ui::wrapping_mode,
			ui::horizontal_text_alignment, ui::vertical_text_alignment
		);
		/// Creates a \ref plain_text object.
		std::unique_ptr<plain_text> _create_plain_text_impl(
			std::basic_string_view<WCHAR>, ui::font&, double size
		);


		/// Returns the \p IDXGIFactory associated with \ref _dxgi_device.
		_details::com_wrapper<IDXGIFactory2> _get_dxgi_factory();
		/// Creates a \p ID2DBitmap1 from a \p IDXGISwapChain1.
		_details::com_wrapper<ID2D1Bitmap1> _create_bitmap_from_swap_chain(
			IDXGISwapChain1*, vec2d scaling_factor
		);

		/// Creates a corresponding \p ID2D1HwndRenderTarget.
		void _new_window(ui::window_base&) override;
		/// Releases all resources.
		void _delete_window(ui::window_base&) override;
	};
}
