// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The Direct2D renderer backend.

#include <stack>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dwrite.h>

#include "../../ui/renderer.h"
#include "window.h"
#include "misc.h"

namespace codepad::os::direct2d {
	class render_target;
	class renderer;

	/// Miscellaneous helper functions.
	namespace _details {
		using namespace os::_details; // import stuff from common win32

		/// Converts a \ref matd3x3 to a \p D2D1_MATRIX_3X2_F.
		inline D2D1_MATRIX_3X2_F cast_matrix(matd3x3 m) {
			// the resulting matrix is transposed
			D2D1_MATRIX_3X2_F mat;
			mat.m11 = static_cast<FLOAT>(m[0][0]);
			mat.m12 = static_cast<FLOAT>(m[1][0]);
			mat.m21 = static_cast<FLOAT>(m[0][1]);
			mat.m22 = static_cast<FLOAT>(m[1][1]);
			mat.dx = static_cast<FLOAT>(m[0][2]);
			mat.dy = static_cast<FLOAT>(m[1][2]);
			return mat;
		}
		/// Converts a \ref colord to a \p D2D1_COLOR_F.
		inline D2D1_COLOR_F cast_color(colord c) {
			return D2D1::ColorF(
				static_cast<FLOAT>(c.r),
				static_cast<FLOAT>(c.g),
				static_cast<FLOAT>(c.b),
				static_cast<FLOAT>(c.a)
			);
		}
		/// Converts a \ref rectd to a \p D2D1_RECT_F.
		inline D2D1_RECT_F cast_rect(rectd r) {
			return D2D1::RectF(
				static_cast<FLOAT>(r.xmin),
				static_cast<FLOAT>(r.ymin),
				static_cast<FLOAT>(r.xmax),
				static_cast<FLOAT>(r.ymax)
			);
		}
		/// Converts a \ref vec2d to a \p D2D1_POINT_2F.
		inline D2D1_POINT_2F cast_point(vec2d pt) {
			return D2D1::Point2F(static_cast<FLOAT>(pt.x), static_cast<FLOAT>(pt.y));
		}

		/// Constructs a \p DWRITE_TEXT_RANGE.
		inline DWRITE_TEXT_RANGE make_text_range(std::size_t beg, std::size_t len) {
			DWRITE_TEXT_RANGE result;
			result.startPosition = static_cast<UINT32>(beg);
			// this works for size_t::max() as well since both types are unsigned
			result.length = static_cast<UINT32>(len);
			return result;
		}

		/// Casts a \ref ui::font_style to a \p DWRITE_FONT_STYLE.
		inline DWRITE_FONT_STYLE cast_font_style(ui::font_style style) {
			switch (style) {
			case ui::font_style::normal:
				return DWRITE_FONT_STYLE_NORMAL;
			case ui::font_style::italic:
				return DWRITE_FONT_STYLE_ITALIC;
			case ui::font_style::oblique:
				return DWRITE_FONT_STYLE_OBLIQUE;
			}
			return DWRITE_FONT_STYLE_NORMAL; // should not be here
		}
		/// Casts a \ref ui::font_weight to a \p DWRITE_FONT_WEIGHT.
		inline DWRITE_FONT_WEIGHT cast_font_weight(ui::font_weight weight) {
			// TODO
			return DWRITE_FONT_WEIGHT_REGULAR;
		}
		/// Casts a \ref ui::font_stretch to a \p DWRITE_FONT_STRETCH.
		inline DWRITE_FONT_STRETCH cast_font_stretch(ui::font_stretch stretch) {
			// TODO
			return DWRITE_FONT_STRETCH_NORMAL;
		}
		/// Casts a \ref ui::horizontal_text_alignment to a \p DWRITE_TEXT_ALIGNMENT.
		inline DWRITE_TEXT_ALIGNMENT cast_horizontal_text_alignment(ui::horizontal_text_alignment align) {
			switch (align) {
			case ui::horizontal_text_alignment::center:
				return DWRITE_TEXT_ALIGNMENT_CENTER;
			case ui::horizontal_text_alignment::front:
				return DWRITE_TEXT_ALIGNMENT_LEADING;
			case ui::horizontal_text_alignment::rear:
				return DWRITE_TEXT_ALIGNMENT_TRAILING;
			}
			return DWRITE_TEXT_ALIGNMENT_LEADING; // should not be here
		}
		/// Casts a \ref ui::vertical_text_alignment to a \p DWRITE_PARAGRAPH_ALIGNMENT.
		inline DWRITE_PARAGRAPH_ALIGNMENT cast_vertical_text_alignment(ui::vertical_text_alignment align) {
			switch (align) {
			case ui::vertical_text_alignment::top:
				return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
			case ui::vertical_text_alignment::center:
				return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
			case ui::vertical_text_alignment::bottom:
				return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
			}
			return DWRITE_PARAGRAPH_ALIGNMENT_NEAR; // should not be here
		}
		/// Casts a \ref ui::wrapping_mode to a \p DWRITE_WORD_WRAPPING.
		inline DWRITE_WORD_WRAPPING cast_wrapping_mode(ui::wrapping_mode wrap) {
			switch (wrap) {
			case ui::wrapping_mode::none:
				return DWRITE_WORD_WRAPPING_NO_WRAP;
			case ui::wrapping_mode::wrap:
				return DWRITE_WORD_WRAPPING_WRAP;
			}
			return DWRITE_WORD_WRAPPING_NO_WRAP; // should not be here
		}
	}

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
		rectd get_layout() const override {
			DWRITE_TEXT_METRICS metrics;
			com_check(_text->GetMetrics(&metrics));
			return rectd::from_xywh(
				metrics.left, metrics.top, metrics.widthIncludingTrailingWhitespace, metrics.height
			);
		}
		/// Returns the metrics of each line.
		std::vector<ui::line_metrics> get_line_metrics() const override {
			constexpr std::size_t small_buffer_size = 5;

			DWRITE_LINE_METRICS small_buffer[small_buffer_size], *bufptr = small_buffer;
			std::vector<DWRITE_LINE_METRICS> large_buffer;
			UINT32 linecount;
			HRESULT res = _text->GetLineMetrics(small_buffer, small_buffer_size, &linecount); // TODO bottleneck
			auto ressize = static_cast<std::size_t>(linecount);
			if (res == E_NOT_SUFFICIENT_BUFFER) {
				large_buffer.resize(ressize);
				bufptr = large_buffer.data();
				com_check(_text->GetLineMetrics(bufptr, linecount, &linecount));
			} else {
				com_check(res);
			}
			std::vector<ui::line_metrics> resvec;
			resvec.reserve(ressize);
			for (std::size_t i = 0; i < ressize; ++i) {
				resvec.emplace_back(bufptr[i].height, bufptr[i].baseline);
			}
			return resvec;
		}

		/// Invokes \p IDWriteTextLayout::HitTestPoint().
		ui::caret_hit_test_result hit_test(vec2d pos) const override {
			BOOL trailing, inside;
			DWRITE_HIT_TEST_METRICS metrics;
			com_check(_text->HitTestPoint(
				static_cast<FLOAT>(pos.x), static_cast<FLOAT>(pos.y), &trailing, &inside, &metrics
			));
			return ui::caret_hit_test_result(
				static_cast<std::size_t>(metrics.textPosition),
				rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height),
				trailing != 0
			);
		}
		/// Invokes \p IDWriteTextLayout::HitTestTextPosition().
		rectd get_character_placement(std::size_t pos) const override {
			FLOAT px, py;
			DWRITE_HIT_TEST_METRICS metrics;
			com_check(_text->HitTestTextPosition(static_cast<UINT32>(pos), false, &px, &py, &metrics));
			return rectd::from_xywh(metrics.left, metrics.top, metrics.width, metrics.height);
		}

		/// Calls \p IDWriteTextLayout::SetDrawingEffect().
		void set_text_color(colord, std::size_t, std::size_t) override;
		/// Calls \p IDWriteTextLayout::SetFontFamilyName().
		void set_font_family(str_view_t family, std::size_t beg, std::size_t len) override {
			com_check(_text->SetFontFamilyName(
				_details::utf8_to_wstring(family).c_str(), _details::make_text_range(beg, len)
			));
		}
		/// Calls \p IDWriteTextLayout::SetFontSize().
		void set_font_size(double size, std::size_t beg, std::size_t len) override {
			com_check(_text->SetFontSize(static_cast<FLOAT>(size), _details::make_text_range(beg, len)));
		}
		/// Calls \p IDWriteTextLayout::SetFontStyle().
		void set_font_style(ui::font_style style, std::size_t beg, std::size_t len) override {
			com_check(_text->SetFontStyle(
				_details::cast_font_style(style), _details::make_text_range(beg, len)
			));
		}
		/// Calls \p IDWriteTextLayout::SetFontStyle().
		void set_font_weight(ui::font_weight weight, std::size_t beg, std::size_t len) override {
			com_check(_text->SetFontWeight(
				_details::cast_font_weight(weight), _details::make_text_range(beg, len)
			));
		}
		/// Calls \p IDWriteTextLayout::SetFontStretch().
		void set_font_stretch(ui::font_stretch stretch, std::size_t beg, std::size_t len) override {
			com_check(_text->SetFontStretch(
				_details::cast_font_stretch(stretch), _details::make_text_range(beg, len)
			));
		}
	protected:
		/// Initializes \ref _rend.
		explicit formatted_text(renderer &r) : _rend(&r) {
		}

		_details::com_wrapper<IDWriteTextLayout> _text; ///< The \p IDWriteTextLayout handle.
		renderer *_rend = nullptr; ///< The renderer.
	};

	/// Encapsules a \p IDWriteFontFamily.
	class font_family : public ui::font_family {
		friend renderer;
	protected:
		_details::com_wrapper<IDWriteFontFamily> _family;
	};

	/// Stores a piece of analyzed text.
	class plain_text : public ui::plain_text {
	public:
		/// Returns the width of this piece of text.
		double get_width() const override {
			// TODO
			return 0.0;
		}

		/// Performs hit testing.
		ui::caret_hit_test_result hit_test(double) const override {
			// TODO
			return ui::caret_hit_test_result();
		}
		/// Returns the position of the given character.
		rectd get_character_placement(std::size_t) const override {
			// TODO
			return rectd();
		}
	};

	/// Encapsules a \p ID2D1PathGeometry and a \p ID2D1GeometrySink.
	class path_geometry_builder : public ui::path_geometry_builder {
		friend renderer;
	public:
		/// Closes and ends the current sub-path.
		void close() override {
			if (_stroking) {
				_sink->EndFigure(D2D1_FIGURE_END_CLOSED);
				_stroking = false;
			}
		}
		/// Moves to the given position and starts a new sub-path.
		void move_to(vec2d pos) override {
			if (_stroking) { // end current subpath first
				_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			}
			_last_point = _details::cast_point(pos);
			_sink->BeginFigure(_last_point, D2D1_FIGURE_BEGIN_FILLED); // TODO no additional information
			_stroking = true;
		}

		/// Adds a segment from the current position to the given position.
		void add_segment(vec2d to) override {
			_on_stroke();
			_last_point = _details::cast_point(to);
			_sink->AddLine(_last_point);
		}
		/// Adds a cubic bezier segment.
		void add_cubic_bezier(vec2d to, vec2d control1, vec2d control2) override {
			_on_stroke();
			_last_point = _details::cast_point(to);
			_sink->AddBezier(D2D1::BezierSegment(
				_details::cast_point(control1),
				_details::cast_point(control2),
				_last_point
			));
		}
		/// Adds an arc (part of a circle).
		void add_arc(vec2d to, vec2d radius, double rotation, ui::sweep_direction dir, ui::arc_type type) override {
			_on_stroke();
			_last_point = _details::cast_point(to);
			_sink->AddArc(D2D1::ArcSegment(
				_last_point,
				D2D1::SizeF(static_cast<FLOAT>(radius.x), static_cast<FLOAT>(radius.y)),
				static_cast<FLOAT>(rotation) * (180.0f / 3.14159f), // in degrees
				dir == ui::sweep_direction::clockwise ?
				D2D1_SWEEP_DIRECTION_CLOCKWISE :
				D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
				type == ui::arc_type::minor ?
				D2D1_ARC_SIZE_SMALL :
				D2D1_ARC_SIZE_LARGE
			));
		}
	protected:
		_details::com_wrapper<ID2D1PathGeometry> _geom; ///< The geometry.
		_details::com_wrapper<ID2D1GeometrySink> _sink; ///< The actual builder.
		D2D1_POINT_2F _last_point; ///< The last destination point.
		bool _stroking = false; ///< Indicates whether strokes are being drawn.

		/// Initializes \ref _geom.
		void _start(ID2D1Factory *factory) {
			com_check(factory->CreatePathGeometry(_geom.get_ref()));
			com_check(_geom->Open(_sink.get_ref()));
			_stroking = false;
		}
		/// Ends the path, releases \ref _sink, and returns the geometry.
		_details::com_wrapper<ID2D1PathGeometry> _end() {
			if (_stroking) {
				_sink->EndFigure(D2D1_FIGURE_END_OPEN);
			}
			com_check(_sink->Close());
			_sink.reset();
			_details::com_wrapper<ID2D1PathGeometry> result;
			std::swap(result, _geom);
			return result;
		}

		/// Called before any actual stroke is added to ensure that \ref _stroking is \p true.
		void _on_stroke() {
			if (!_stroking) {
				_sink->BeginFigure(_last_point, D2D1_FIGURE_BEGIN_FILLED); // TODO no additional information
				_stroking = true;
			}
		}
	};


	namespace _details { // cast objects to types specific to the direct2d renderer
		/// Underlying implementation of various <tt>cast_*</tt> functions.
		template <typename To, typename From> inline To &cast_object(From &f) {
#ifdef CP_CHECK_LOGICAL_ERRORS
			To *res = dynamic_cast<To*>(&f);
			assert_true_logical(res, "invalid object type");
			return *res;
#else
			return *static_cast<To*>(&f);
#endif
		}

		/// Casts a \ref ui::render_target to a \ref render_target.
		inline render_target &cast_render_target(ui::render_target &t) {
			return cast_object<render_target>(t);
		}
		/// Casts a \ref ui::bitmap to a \ref bitmap.
		inline bitmap &cast_bitmap(ui::bitmap &b) {
			return cast_object<bitmap>(b);
		}
		/// Casts a \ref ui::formatted_text to a \ref formatted_text.
		inline formatted_text &cast_formatted_text(ui::formatted_text &t) {
			return cast_object<formatted_text>(t);
		}
	}


	/// The Direct2D renderer backend.
	class renderer : public ui::renderer_base {
		friend formatted_text;
	public:
		constexpr static DXGI_FORMAT pixel_format = DXGI_FORMAT_B8G8R8A8_UNORM; ///< The default pixel format.

		/// Initializes \ref _d2d_factory.
		renderer() {
			D3D_FEATURE_LEVEL supported_feature_levels[]{
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1,
				D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_9_3,
				D3D_FEATURE_LEVEL_9_2,
				D3D_FEATURE_LEVEL_9_1
			};
			D3D_FEATURE_LEVEL created_feature_level;
			com_check(D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
				supported_feature_levels,
				ARRAYSIZE(supported_feature_levels),
				D3D11_SDK_VERSION,
				_d3d_device.get_ref(),
				&created_feature_level,
				nullptr
			));
			logger::get().log_debug(CP_HERE) << "D3D feature level: " << created_feature_level;
			com_check(_d3d_device->QueryInterface(_dxgi_device.get_ref()));

			com_check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _d2d_factory.get_ref()));
			com_check(_d2d_factory->CreateDevice(_dxgi_device.get(), _d2d_device.get_ref()));
			com_check(_d2d_device->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _d2d_device_context.get_ref()
			));
			_d2d_device_context->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
			// create brush for text
			com_check(_d2d_device_context->CreateSolidColorBrush(
				_details::cast_color(colord()), _text_brush.get_ref()
			));

			com_check(DWriteCreateFactory(
				DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(_dwrite_factory.get_ref())
			));
		}

		/// Creates a render target of the given size.
		ui::render_target_data create_render_target(vec2d size, vec2d scaling_factor) override {
			auto resrt = std::make_unique<render_target>();
			auto resbmp = std::make_unique<bitmap>();
			_details::com_wrapper<IDXGISurface> surface;

			D3D11_TEXTURE2D_DESC texture_desc;
			texture_desc.Width = static_cast<UINT>(std::ceil(size.x * scaling_factor.x));
			texture_desc.Height = static_cast<UINT>(std::ceil(size.y * scaling_factor.y));
			texture_desc.MipLevels = 1;
			texture_desc.ArraySize = 1;
			texture_desc.Format = pixel_format;
			texture_desc.SampleDesc.Count = 1;
			texture_desc.SampleDesc.Quality = 0;
			texture_desc.Usage = D3D11_USAGE_DEFAULT;
			texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			texture_desc.CPUAccessFlags = 0;
			texture_desc.MiscFlags = 0;
			com_check(_d3d_device->CreateTexture2D(&texture_desc, nullptr, resrt->_texture.get_ref()));
			com_check(resrt->_texture->QueryInterface(surface.get_ref()));
			com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
				surface.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_TARGET,
					D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
					static_cast<FLOAT>(USER_DEFAULT_SCREEN_DPI * scaling_factor.x),
					static_cast<FLOAT>(USER_DEFAULT_SCREEN_DPI * scaling_factor.y)
				),
				resrt->_bitmap.get_ref()
			));
			resbmp->_bitmap = resrt->_bitmap;
			return ui::render_target_data(std::move(resrt), std::move(resbmp));
		}

		/// Loads a \ref bitmap from disk.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp, vec2d scaling_factor) override {
			auto res = std::make_unique<bitmap>();
			_details::com_wrapper<IWICBitmapSource> converted;
			_details::com_wrapper<IWICBitmapSource> img = _details::wic_image_loader::get().load_image(bmp);

			com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, img.get(), converted.get_ref()));
			com_check(_d2d_device_context->CreateBitmapFromWicBitmap(
				converted.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_NONE,
					D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN),
					static_cast<FLOAT>(scaling_factor.x * USER_DEFAULT_SCREEN_DPI),
					static_cast<FLOAT>(scaling_factor.y * USER_DEFAULT_SCREEN_DPI),
					nullptr
				),
				res->_bitmap.get_ref())
			);
			return res;
		}

		/// Creates a \p IDWriteTextFormat.
		std::unique_ptr<ui::font_family> find_font_family(str_view_t family) override {
			_details::com_wrapper<IDWriteFontCollection> fonts;
			com_check(_dwrite_factory->GetSystemFontCollection(fonts.get_ref(), false)); // no need to hurry

			UINT32 index = 0;
			BOOL exist = false;
			com_check(fonts->FindFamilyName(os::_details::utf8_to_wstring(family).c_str(), &index, &exist));
			if (!exist) {
				return nullptr;
			}

			auto res = std::make_unique<font_family>();
			com_check(fonts->GetFontFamily(index, res->_family.get_ref()));
			return res;
		}

		/// Starts drawing to the given window.
		void begin_drawing(ui::window_base &w) override {
			auto &data = _window_data::get(w);
			_begin_draw_impl(data.target.get(), w.get_scaling_factor() * USER_DEFAULT_SCREEN_DPI);
			_present_chains.emplace(data.swap_chain.get());
		}
		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target &r) override {
			_details::com_wrapper<ID2D1Bitmap1> &bitmap = _details::cast_render_target(r)._bitmap;
			FLOAT dpix, dpiy;
			bitmap->GetDpi(&dpix, &dpiy);
			_begin_draw_impl(bitmap.get(), vec2d(dpix, dpiy));
		}
		/// Finishes drawing.
		void end_drawing() override {
			assert_true_usage(!_render_stack.empty(), "begin_drawing/end_drawing calls mismatch");
			assert_true_usage(_render_stack.top().matrices.size() == 1, "push_matrix/pop_matrix calls mismatch");
			_render_stack.pop();
			if (_render_stack.empty()) {
				com_check(_d2d_device_context->EndDraw());
				_d2d_device_context->SetTarget(nullptr); // so that windows can be resized normally
				for (IDXGISwapChain *chain : _present_chains) {
					com_check(chain->Present(0, 0));
				}
				_present_chains.clear();
			} else {
				_d2d_device_context->SetTarget(_render_stack.top().target);
				_update_transform();
			}
		}

		/// Pushes a matrix onto the stack.
		void push_matrix(matd3x3 m) override {
			_render_stack.top().matrices.push(_details::cast_matrix(m));
			_update_transform();
		}
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack.
		void push_matrix_mult(matd3x3 m) override {
			D2D1_MATRIX_3X2_F mat = _details::cast_matrix(m);
			_render_target_stackframe &frame = _render_stack.top();
			frame.matrices.push(mat * frame.matrices.top());
			_update_transform();
		}
		/// Pops a matrix from the stack.
		void pop_matrix() override {
			_render_stack.top().matrices.pop();
			_update_transform();
		}

		/// Clears the current surface using the given color.
		void clear(colord color) override {
			_d2d_device_context->Clear(_details::cast_color(color));
		}

		/// Calls \ref path_geometry_builder::_start() and returns \ref _path_builder.
		ui::path_geometry_builder &start_path() override {
			_path_builder._start(_d2d_factory.get());
			return _path_builder;
		}

		/// Draws a \p ID2D1EllipseGeometry.
		void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_details::com_wrapper<ID2D1EllipseGeometry> geom;
			com_check(_d2d_factory->CreateEllipseGeometry(
				D2D1::Ellipse(
					_details::cast_point(center), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
				),
				geom.get_ref()
			));
			_draw_geometry(std::move(geom), brush, pen);
		}
		/// Draws a \p ID2D1RectangleGeometry.
		void draw_rectangle(
			rectd rect, const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_details::com_wrapper<ID2D1RectangleGeometry> geom;
			com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), geom.get_ref()));
			_draw_geometry(std::move(geom), brush, pen);
		}
		/// Draws a \p ID2D1RoundedRectangleGeometry.
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_details::com_wrapper<ID2D1RoundedRectangleGeometry> geom;
			com_check(_d2d_factory->CreateRoundedRectangleGeometry(
				D2D1::RoundedRect(
					_details::cast_rect(region), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
				),
				geom.get_ref()
			));
			_draw_geometry(std::move(geom), brush, pen);
		}
		/// Calls \ref path_geometry_builder::_end() and draws the returned geometry.
		void end_and_draw_path(
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_draw_geometry(_path_builder._end(), brush, pen);
		}

		/// Creates a \p ID2D1EllipseGeometry and pushes it as a clip.
		void push_ellipse_clip(vec2d center, double radiusx, double radiusy) override {
			_details::com_wrapper<ID2D1EllipseGeometry> geom;
			com_check(_d2d_factory->CreateEllipseGeometry(D2D1::Ellipse(
				_details::cast_point(center), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
			), geom.get_ref()));
			_push_layer(std::move(geom));
		}
		/// Creates a \p ID2D1RectangleGeometry and pushes it as a clip.
		void push_rectangle_clip(rectd rect) override {
			_details::com_wrapper<ID2D1RectangleGeometry> geom;
			com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), geom.get_ref()));
			_push_layer(std::move(geom));
		}
		/// Creates a \p ID2D1RoundedRectangleGeometry and pushes it as a clip.
		void push_rounded_rectangle_clip(rectd rect, double radiusx, double radiusy) override {
			_details::com_wrapper<ID2D1RoundedRectangleGeometry> geom;
			com_check(_d2d_factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(
				_details::cast_rect(rect), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
			), geom.get_ref()));
			_push_layer(std::move(geom));
		}
		/// Calls \ref path_geometry_builder::_end() and pushes the returned geometry as a clip.
		void end_and_push_path_clip() override {
			_push_layer(_path_builder._end());
		}
		/// Calls \p ID2D1DeviceContext::PopLayer() to pop the most recently pushed layer.
		void pop_clip() override {
			_d2d_device_context->PopLayer();
		}

		/// Calls \ref _create_formatted_text_impl().
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			str_view_t text, const ui::font_parameters &params, colord c, vec2d maxsize, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
		) override {
			auto converted = _details::utf8_to_wstring(text);
			return _create_formatted_text_impl(converted, params, c, maxsize, wrap, halign, valign);
		}
		/// Calls \ref _create_formatted_text_impl().
		std::unique_ptr<ui::formatted_text> create_formatted_text(
			std::basic_string_view<codepoint> text, const ui::font_parameters &params, colord c,
			vec2d maxsize, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
		) override {
			std::basic_string<std::byte> bytestr;
			for (codepoint cp : text) {
				bytestr += encodings::utf16<>::encode_codepoint(cp);
			}
			return _create_formatted_text_impl(
				std::basic_string_view<WCHAR>(reinterpret_cast<const WCHAR*>(bytestr.c_str()), bytestr.size() / 2),
				params, c, maxsize, wrap, halign, valign
			);
		}
		/// Calls \p ID2D1DeviceContext::DrawTextLayout to render the given \ref formatted_text.
		void draw_formatted_text(ui::formatted_text &text, vec2d topleft) override {
			auto ctext = _details::cast_formatted_text(text);
			/*_text_brush->SetColor(_details::cast_color(colord(1.0, 1.0, 1.0, 1.0)));*/
			_text_brush->SetColor(_details::cast_color(colord(0.0, 0.0, 0.0, 1.0)));
			_d2d_device_context->DrawTextLayout(
				_details::cast_point(topleft),
				ctext._text.get(),
				_text_brush.get(),
				D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT
			);
		}

		///
		std::unique_ptr<ui::plain_text> create_plain_text(
			str_view_t, ui::font_family&, double, ui::font_style, ui::font_weight, ui::font_stretch
		) override {
			// TODO
			return std::make_unique<plain_text>();
		}
		///
		std::unique_ptr<ui::plain_text> create_plain_text(
			std::basic_string_view<codepoint>, ui::font_family&, double, ui::font_style, ui::font_weight, ui::font_stretch
		) override {
			// TODO
			return std::make_unique<plain_text>();
		}
		///
		void draw_plain_text(ui::plain_text&, vec2d, colord) override {
			// TODO
		}
	protected:
		/// Stores window-specific data.
		struct _window_data {
			_details::com_wrapper<IDXGISwapChain1> swap_chain; ///< The DXGI swap chain.
			_details::com_wrapper<ID2D1Bitmap1> target; ///< The target bitmap.

			/// Returns the \ref _window_data associated with the given window.
			inline static _window_data &get(ui::window_base &wnd) {
				_window_data *d = std::any_cast<_window_data>(&_get_window_data(wnd));
				assert_true_usage(d, "window has no associated data");
				return *d;
			}
		};
		/// Stores information about a currently active render target.
		struct _render_target_stackframe {
			/// Initializes \ref target, and pushes an identity matrix onto the stack.
			explicit _render_target_stackframe(ID2D1Image *t) : target(t) {
				matrices.emplace(D2D1::IdentityMatrix());
			}

			std::stack<D2D1_MATRIX_3X2_F> matrices; ///< The stack of matrices.
			/// The render target. Here we're using raw pointers to skip a few \p AddRef() and \p Release() calls
			/// since the target must be alive during the entire duration of rendering (if it doesn't, then it's the
			/// user's fault and we'll just let it crash).
			ID2D1Image *target = nullptr;
		};

		std::stack<_render_target_stackframe> _render_stack; ///< The stack of render targets.
		std::set<IDXGISwapChain*> _present_chains; ///< The DXGI swap chains to call \p Present() on.
		path_geometry_builder _path_builder; ///< The path builder.
		_details::com_wrapper<ID2D1Factory1> _d2d_factory; ///< The Direct2D factory.
		_details::com_wrapper<ID2D1Device> _d2d_device; ///< The Direct2D device.
		_details::com_wrapper<ID2D1DeviceContext> _d2d_device_context; ///< The Direct2D device context.
		_details::com_wrapper<ID3D11Device> _d3d_device; ///< The Direct3D device.
		_details::com_wrapper<IDXGIDevice> _dxgi_device; ///< The DXGI device.
		_details::com_wrapper<IDXGIAdapter> _dxgi_adapter; ///< The DXGI adapter.
		_details::com_wrapper<IDWriteFactory> _dwrite_factory; ///< The DirectWrite factory.

		_details::com_wrapper<ID2D1SolidColorBrush> _text_brush; ///< The brush used for rendering text.

		/// Starts drawing to a \p ID2D1RenderTarget.
		void _begin_draw_impl(ID2D1Image *target, vec2d dpi) {
			_d2d_device_context->SetTarget(target);
			_d2d_device_context->SetDpi(static_cast<FLOAT>(dpi.x), static_cast<FLOAT>(dpi.y));
			if (_render_stack.empty()) {
				_d2d_device_context->BeginDraw();
			}
			_render_stack.emplace(target);
			_update_transform();
		}
		/// Updates the transform of \ref _d2d_device_context.
		void _update_transform() {
			_d2d_device_context->SetTransform(_render_stack.top().matrices.top());
		}
		/// Draws the given geometry.
		void _draw_geometry(
			_details::com_wrapper<ID2D1Geometry> geom,
			const ui::generic_brush_parameters &brush_def,
			const ui::generic_pen_parameters &pen_def
		) {
			if (_details::com_wrapper<ID2D1Brush> brush = _create_brush(brush_def)) {
				_d2d_device_context->FillGeometry(geom.get(), brush.get());
			}
			if (_details::com_wrapper<ID2D1Brush> pen = _create_brush(pen_def.brush)) {
				_d2d_device_context->DrawGeometry(geom.get(), pen.get(), static_cast<FLOAT>(pen_def.thickness));
			}
		}
		/// Calls \p ID2D1DeviceContext::PushLayer() to push a layer with the specified geometry as its clip.
		void _push_layer(_details::com_wrapper<ID2D1Geometry> clip) {
			_d2d_device_context->PushLayer(D2D1::LayerParameters(
				D2D1::InfiniteRect(), clip.get(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
				D2D1::IdentityMatrix(), 1.0f, nullptr, D2D1_LAYER_OPTIONS_INITIALIZE_FOR_CLEARTYPE
			), nullptr);
		}

		/// Creates a brush based on \ref ui::brush_parameters::solid_color.
		_details::com_wrapper<ID2D1SolidColorBrush> _create_brush(
			const ui::brush_parameters::solid_color &brush_def
		) {
			_details::com_wrapper<ID2D1SolidColorBrush> brush;
			com_check(_d2d_device_context->CreateSolidColorBrush(
				_details::cast_color(brush_def.color), brush.get_ref()
			));
			return brush;
		}
		/// Creates a \p ID2D1GradientStopCollection.
		_details::com_wrapper<ID2D1GradientStopCollection> _create_gradient_stop_collection(
			const std::vector<ui::gradient_stop> &stops_def
		) {
			_details::com_wrapper<ID2D1GradientStopCollection> gradients;
			std::vector<D2D1_GRADIENT_STOP> stops;
			for (const auto &s : stops_def) {
				stops.emplace_back(D2D1::GradientStop(static_cast<FLOAT>(s.position), _details::cast_color(s.color)));
			}
			com_check(_d2d_device_context->CreateGradientStopCollection(
				stops.data(), static_cast<UINT32>(stops.size()), D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP,
				gradients.get_ref()
			)); // TODO clamp mode
			return gradients;
		}
		/// Creates a brush based on \ref ui::brush_parameters::linear_gradient.
		_details::com_wrapper<ID2D1LinearGradientBrush> _create_brush(
			const ui::brush_parameters::linear_gradient &brush_def
		) {
			_details::com_wrapper<ID2D1LinearGradientBrush> brush;
			if (brush_def.gradients) {
				com_check(_d2d_device_context->CreateLinearGradientBrush(
					D2D1::LinearGradientBrushProperties(
						_details::cast_point(brush_def.from), _details::cast_point(brush_def.to)
					),
					_create_gradient_stop_collection(*brush_def.gradients).get(),
					brush.get_ref()
				));
			}
			return brush;
		}
		/// Creates a brush based on \ref ui::brush_parameters::radial_gradient.
		_details::com_wrapper<ID2D1RadialGradientBrush> _create_brush(
			const ui::brush_parameters::radial_gradient &brush_def
		) {
			_details::com_wrapper<ID2D1RadialGradientBrush> brush;
			if (brush_def.gradients) {
				com_check(_d2d_device_context->CreateRadialGradientBrush(
					D2D1::RadialGradientBrushProperties(
						_details::cast_point(brush_def.center), D2D1::Point2F(),
						static_cast<FLOAT>(brush_def.radius), static_cast<FLOAT>(brush_def.radius)
					),
					_create_gradient_stop_collection(*brush_def.gradients).get(),
					brush.get_ref()
				));
			}
			return brush;
		}
		/// Creates a brush based on \ref ui::brush_parameters::bitmap_pattern.
		_details::com_wrapper<ID2D1BitmapBrush> _create_brush(
			const ui::brush_parameters::bitmap_pattern &brush_def
		) {
			_details::com_wrapper<ID2D1BitmapBrush> brush;
			if (brush_def.image) {
				com_check(_d2d_device_context->CreateBitmapBrush(
					_details::cast_bitmap(*brush_def.image)._bitmap.get(),
					D2D1::BitmapBrushProperties(), // TODO extend modes
					brush.get_ref()
				));
			}
			return brush;
		}
		/// Returns an empty \ref _details::com_wrapper.
		_details::com_wrapper<ID2D1Brush> _create_brush(const ui::brush_parameters::none&) {
			return _details::com_wrapper<ID2D1Brush>();
		}
		/// Creates a \p ID2D1Brush from the given \ref ui::brush specification.
		_details::com_wrapper<ID2D1Brush> _create_brush(const ui::generic_brush_parameters &b) {
			auto brush = std::visit([this](auto &&brush) {
				return _details::com_wrapper<ID2D1Brush>(_create_brush(brush));
				}, b.value);
			if (brush) {
				brush->SetTransform(_details::cast_matrix(b.transform));
			}
			return brush;
		}

		/// Creates an \p IDWriteTextLayout.
		std::unique_ptr<ui::formatted_text> _create_formatted_text_impl(
			std::basic_string_view<WCHAR> text, const ui::font_parameters &fmt, colord c,
			vec2d maxsize, ui::wrapping_mode wrap,
			ui::horizontal_text_alignment halign, ui::vertical_text_alignment valign
		) {
			// use new to access protedted constructor
			auto res = std::unique_ptr<formatted_text>(new formatted_text(*this));
			_details::com_wrapper<IDWriteTextFormat> format;
			com_check(_dwrite_factory->CreateTextFormat(
				_details::utf8_to_wstring(fmt.family).c_str(),
				nullptr,
				_details::cast_font_weight(fmt.weight),
				_details::cast_font_style(fmt.style),
				_details::cast_font_stretch(fmt.stretch),
				static_cast<FLOAT>(fmt.size),
				L"", // FIXME is this good practice?
				format.get_ref()
			));
			com_check(format->SetWordWrapping(_details::cast_wrapping_mode(wrap)));
			com_check(format->SetTextAlignment(_details::cast_horizontal_text_alignment(halign)));
			com_check(format->SetParagraphAlignment(_details::cast_vertical_text_alignment(valign)));
			com_check(_dwrite_factory->CreateTextLayout(
				text.data(), static_cast<UINT32>(text.size()),
				format.get(),
				static_cast<FLOAT>(maxsize.x), static_cast<FLOAT>(maxsize.y),
				res->_text.get_ref()
			));
			res->set_text_color(c, 0, std::numeric_limits<std::size_t>::max());
			return res;
		}

		/// Returns the \p IDXGIFactory associated with \ref _dxgi_device.
		_details::com_wrapper<IDXGIFactory2> _get_dxgi_factory() {
			_details::com_wrapper<IDXGIAdapter> adapter;
			com_check(_dxgi_device->GetAdapter(adapter.get_ref()));
			_details::com_wrapper<IDXGIFactory2> factory;
			com_check(adapter->GetParent(IID_PPV_ARGS(factory.get_ref())));
			return factory;
		}
		/// Creates a \p ID2DBitmap1 from a \p IDXGISwapChain1.
		_details::com_wrapper<ID2D1Bitmap1> _create_bitmap_from_swap_chain(
			IDXGISwapChain1 *chain, vec2d scaling_factor
		) {
			_details::com_wrapper<IDXGISurface> surface;
			_details::com_wrapper<ID2D1Bitmap1> bitmap;
			com_check(chain->GetBuffer(0, IID_PPV_ARGS(surface.get_ref())));
			com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
				surface.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
					D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
					static_cast<FLOAT>(scaling_factor.x * USER_DEFAULT_SCREEN_DPI),
					static_cast<FLOAT>(scaling_factor.y * USER_DEFAULT_SCREEN_DPI)
				),
				bitmap.get_ref()
			));
			return bitmap;
		}

		/// Creates a corresponding \p ID2D1HwndRenderTarget.
		void _new_window(ui::window_base &w) override {
			window &wnd = _details::cast_window(w);
			std::any &data = _get_window_data(wnd);
			_window_data actual_data;

			// create swap chain
			DXGI_SWAP_CHAIN_DESC1 swapchain_desc;
			swapchain_desc.Width = 0;
			swapchain_desc.Height = 0;
			swapchain_desc.Format = pixel_format;
			swapchain_desc.Stereo = false;
			swapchain_desc.SampleDesc.Count = 1;
			swapchain_desc.SampleDesc.Quality = 0;
			swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchain_desc.BufferCount = 2;
			swapchain_desc.Scaling = DXGI_SCALING_NONE;
			swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
			swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
			swapchain_desc.Flags = 0;
			com_check(_get_dxgi_factory()->CreateSwapChainForHwnd(
				_d3d_device.get(), wnd.get_native_handle(), &swapchain_desc,
				nullptr, nullptr, actual_data.swap_chain.get_ref()
			));
			// set data
			actual_data.target = _create_bitmap_from_swap_chain(
				actual_data.swap_chain.get(), wnd.get_scaling_factor()
			);
			data.emplace<_window_data>(actual_data);
			// resize buffer when the window size has changed
			wnd.size_changed += [this, pwnd = &wnd](ui::window_base::size_changed_info&) {
				auto &data = _window_data::get(*pwnd);
				data.target.reset(); // must release bitmap before resizing
				com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
				// recreate bitmap
				data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), pwnd->get_scaling_factor());
			};
			// reallocate buffer when the window scaling has changed
			wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window_base::scaling_factor_changed_info &p) {
				auto &data = _window_data::get(*pwnd);
				data.target.reset(); // must release bitmap before resizing
				com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
				// recreate bitmap
				data.target = _create_bitmap_from_swap_chain(data.swap_chain.get(), p.new_value);
			};
		}
		/// Releases all resources.
		void _delete_window(ui::window_base &w) override {
			_get_window_data(w).reset();
		}
	};


	void formatted_text::set_text_color(colord c, std::size_t beg, std::size_t len) {
		_details::com_wrapper<ID2D1SolidColorBrush> brush;
		_rend->_d2d_device_context->CreateSolidColorBrush(_details::cast_color(c), brush.get_ref());
		com_check(_text->SetDrawingEffect(brush.get(), _details::make_text_range(beg, len)));
	}
}
