// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// The Direct2D renderer backend.

#include <stack>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>

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
		inline static D2D1_MATRIX_3X2_F cast_matrix(matd3x3 m) {
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
		inline static D2D1_COLOR_F cast_color(colord c) {
			return D2D1::ColorF(
				static_cast<FLOAT>(c.r),
				static_cast<FLOAT>(c.g),
				static_cast<FLOAT>(c.b),
				static_cast<FLOAT>(c.a)
			);
		}
		/// Converts a \ref rectd to a \p D2D1_RECT_F.
		inline static D2D1_RECT_F cast_rect(rectd r) {
			return D2D1::RectF(
				static_cast<FLOAT>(r.xmin),
				static_cast<FLOAT>(r.ymin),
				static_cast<FLOAT>(r.xmax),
				static_cast<FLOAT>(r.ymax)
			);
		}
		/// Converts a \ref vec2d to a \p D2D1_POINT_2F.
		inline static D2D1_POINT_2F cast_point(vec2d pt) {
			return D2D1::Point2F(static_cast<FLOAT>(pt.x), static_cast<FLOAT>(pt.y));
		}
	}

	/// A Direct2D bitmap.
	class bitmap : public ui::bitmap {
		friend render_target;
		friend renderer;
	public:
		/// Default constructor.
		bitmap() = default;

		/// Returns the size of the bitmap.
		vec2d get_size() const override {
			D2D1_SIZE_F sz = _bitmap->GetSize();
			return vec2d(sz.width, sz.height);
		}
		/// Returns whether \ref _bitmap is emtpy.
		bool empty() const override {
			return _bitmap.empty();
		}
	protected:
		_details::com_wrapper<ID2D1Bitmap1> _bitmap; ///< The bitmap.

		/// Initializes \ref _bitmap directly.
		explicit bitmap(_details::com_wrapper<ID2D1Bitmap1> src) : _bitmap(src) {
		}
	};

	/// A Direct2D bitmap render target.
	class render_target : public ui::render_target {
		friend renderer;
	public:
		/// Returns the \ref bitmap corresponding to this \ref render_target.
		std::unique_ptr<ui::bitmap> get_bitmap() const override {
			return std::make_unique<bitmap>(bitmap(_bitmap));
		}
		/// Returns whether \ref _target is empty.
		bool empty() const override {
			return _bitmap.empty();
		}
	protected:
		_details::com_wrapper<ID2D1Bitmap1> _bitmap; ///< The render target.
		_details::com_wrapper<ID3D11Texture2D> _texture; ///< The Direct3D texture.
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
		void add_arc(vec2d to, double radius, ui::sweep_direction dir, ui::arc_type type) override {
			_on_stroke();
			_last_point = _details::cast_point(to);
			_sink->AddArc(D2D1::ArcSegment(
				_last_point,
				D2D1::SizeF(static_cast<FLOAT>(radius), static_cast<FLOAT>(radius)),
				0.0f,
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
		void _start(ID2D1Factory * factory) {
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
			return std::move(_geom);
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
		/// Casts a \ref ui::window_base to a \ref window.
		inline static window &cast_window(ui::window_base &w) {
			auto *wnd = dynamic_cast<window*>(&w);
			assert_true_usage(wnd, "invalid window type");
			return *wnd;
		}
		/// Casts a \ref ui::render_target to a \ref render_target.
		inline static render_target &cast_render_target(ui::render_target &t) {
			auto *rt = dynamic_cast<render_target*>(&t);
			assert_true_usage(rt, "invalid render target type");
			return *rt;
		}
		inline static bitmap &cast_bitmap(ui::bitmap &b) {
			auto *bmp = dynamic_cast<bitmap*>(&b);
			assert_true_usage(bmp, "invalid bitmap type");
			return *bmp;
		}
	}


	/// The Direct2D renderer backend.
	class renderer : public ui::renderer_base {
	public:
		constexpr static DXGI_FORMAT pixel_format = DXGI_FORMAT_B8G8R8A8_UNORM; ///< The default pixel format.

		/// Initializes \ref _d2d_factory.
		renderer() {
			com_check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, _d2d_factory.get_ref()));
			D3D_FEATURE_LEVEL supported_feature_levels[]{
				D3D_FEATURE_LEVEL_11_1,
				D3D_FEATURE_LEVEL_11_0,
				D3D_FEATURE_LEVEL_10_1,
				D3D_FEATURE_LEVEL_10_0,
				D3D_FEATURE_LEVEL_9_3,
				D3D_FEATURE_LEVEL_9_2,
				D3D_FEATURE_LEVEL_9_1
			};
			com_check(D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
				supported_feature_levels,
				ARRAYSIZE(supported_feature_levels),
				D3D11_SDK_VERSION,
				_d3d_device.get_ref(),
				nullptr, // discarded right now
				nullptr
			));
			com_check(_d3d_device->QueryInterface(_dxgi_device.get_ref()));
			com_check(_d2d_factory->CreateDevice(_dxgi_device.get(), _d2d_device.get_ref()));
			com_check(_d2d_device->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE, _d2d_device_context.get_ref()
			));
		}

		/// Creates a render target of the given size.
		std::unique_ptr<ui::render_target> create_render_target(vec2d size) override {
			auto res = std::make_unique<render_target>();
			_details::com_wrapper<IDXGISurface> surface;

			D3D11_TEXTURE2D_DESC texture_desc;
			texture_desc.Width = size.x;
			texture_desc.Height = size.y; // TODO dpi
			texture_desc.MipLevels = 1;
			texture_desc.ArraySize = 1;
			texture_desc.Format = pixel_format;
			texture_desc.SampleDesc.Count = 1;
			texture_desc.SampleDesc.Quality = 0;
			texture_desc.Usage = D3D11_USAGE_DEFAULT;
			texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			texture_desc.CPUAccessFlags = 0;
			texture_desc.MiscFlags = 0;
			com_check(_d3d_device->CreateTexture2D(&texture_desc, nullptr, res->_texture.get_ref()));
			com_check(res->_texture->QueryInterface(surface.get_ref()));
			com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
				surface.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_TARGET,
					D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
					0.0f, 0.0f // TODO dpi
				),
				res->_bitmap.get_ref()
			));
			return res;
		}

		/// Loads a \ref bitmap from disk.
		std::unique_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &bmp) override {
			_details::com_wrapper<IWICBitmapSource> img = wic_image_loader::get().load_image(bmp);
			_details::com_wrapper<IWICBitmapSource> converted;
			_details::com_wrapper<ID2D1Bitmap1> res;
			com_check(WICConvertBitmapSource(GUID_WICPixelFormat32bppPBGRA, img.get(), converted.get_ref()));
			com_check(_d2d_device_context->CreateBitmapFromWicBitmap(converted.get(), res.get_ref()));
			return std::make_unique<bitmap>(bitmap(std::move(res)));
		}

		/// Starts drawing to the given window.
		void begin_drawing(ui::window_base &w) override {
			auto &data = _window_data::get(_details::cast_window(w));
			_begin_draw_impl(data.target.get());
			_present_chains.emplace(data.swap_chain.get());
		}
		/// Starts drawing to the given \ref render_target.
		void begin_drawing(ui::render_target &r) override {
			_begin_draw_impl(_details::cast_render_target(r)._bitmap.get());
		}
		/// Finishes drawing.
		void end_drawing() override {
			assert_true_usage(!_render_stack.empty(), "begin_drawing/end_drawing calls mismatch");
			assert_true_usage(_render_stack.top().matrices.size() == 1, "push_matrix/pop_matrix calls mismatch");
			_render_stack.pop();
			if (_render_stack.empty()) {
				com_check(_d2d_device_context->EndDraw()); // TODO handle device lost?
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
		virtual void push_matrix(matd3x3 m) {
			_render_stack.top().matrices.push(_details::cast_matrix(m));
			_update_transform();
		}
		/// Multiplies the current matrix with the given matrix and pushes it onto the stack.
		virtual void push_matrix_mult(matd3x3 m) {
			D2D1_MATRIX_3X2_F mat = _details::cast_matrix(m);
			_render_target_stackframe &frame = _render_stack.top();
			frame.matrices.push(frame.matrices.top() * mat);
			_update_transform();
		}
		/// Pops a matrix from the stack.
		virtual void pop_matrix() {
			_render_stack.top().matrices.pop();
			_update_transform();
		}

		/// Clears the current surface using the given color.
		void clear(colord color) override {
			_d2d_device_context->Clear(_details::cast_color(color));
		}

		/// Draws a \ref ellipse_geometry.
		void draw_ellipse(
			vec2d center, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			ID2D1EllipseGeometry *geom;
			com_check(_d2d_factory->CreateEllipseGeometry(
				D2D1::Ellipse(
					_details::cast_point(center), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
				),
				&geom
			));
			_draw_geometry(_details::make_com_wrapper_give(geom), brush, pen);
		}
		/// Draws a \ref rectangle_geometry.
		void draw_rectangle(
			rectd rect, const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			ID2D1RectangleGeometry *geom;
			com_check(_d2d_factory->CreateRectangleGeometry(_details::cast_rect(rect), &geom));
			_draw_geometry(_details::make_com_wrapper_give(geom), brush, pen);
		}
		/// Draws a \ref rounded_rectangle_geometry.
		void draw_rounded_rectangle(
			rectd region, double radiusx, double radiusy,
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			ID2D1RoundedRectangleGeometry *geom;
			com_check(_d2d_factory->CreateRoundedRectangleGeometry(
				D2D1::RoundedRect(
					_details::cast_rect(region), static_cast<FLOAT>(radiusx), static_cast<FLOAT>(radiusy)
				),
				&geom
			));
			_draw_geometry(_details::make_com_wrapper_give(geom), brush, pen);
		}

		/// Calls \ref path_geometry_builder::_start() and returns \ref _path_builder.
		ui::path_geometry_builder &start_path() override {
			_path_builder._start(_d2d_factory.get());
			return _path_builder;
		}
		/// Calls \ref path_geometry_builder::_end() and draws the returned geometry.
		void end_and_draw_path(
			const ui::generic_brush_parameters &brush, const ui::generic_pen_parameters &pen
		) override {
			_draw_geometry(_path_builder._end(), brush, pen);
		}
	protected:
		/// Stores window-specific data.
		struct _window_data {
			_details::com_wrapper<IDXGISwapChain1> swap_chain; ///< The DXGI swap chain.
			_details::com_wrapper<ID2D1Bitmap1> target; ///< The target bitmap.

			/// Returns the \ref _window_data associated with the given window.
			inline static _window_data &get(window &wnd) {
				_window_data *d = std::any_cast<_window_data>(&_get_window_data(wnd));
				assert_true_usage(d, "window has no associated data");
				return *d;
			}
		};
		/// Stores information about a currently active render target.
		struct _render_target_stackframe {
			/// Initializes \ref target, and pushes an identity matrix onto the stack.
			_render_target_stackframe(ID2D1Image *t) : target(t) {
				matrices.push(D2D1::IdentityMatrix());
			}

			ID2D1Image *target = nullptr; ///< The render target.
			std::stack<D2D1_MATRIX_3X2_F> matrices; ///< The stack of matrices.
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

		/// Starts drawing to a \p ID2D1RenderTarget.
		void _begin_draw_impl(ID2D1Image *target) {
			_d2d_device_context->SetTarget(target);
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
			auto brush = std::visit([this](auto && brush) {
				return _details::com_wrapper<ID2D1Brush>(_create_brush(brush));
				}, b.value);
			if (brush) {
				brush->SetTransform(_details::cast_matrix(b.transform));
			}
			return brush;
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
		_details::com_wrapper<ID2D1Bitmap1> _create_bitmap_from_swap_chain(IDXGISwapChain1 *chain) {
			_details::com_wrapper<IDXGISurface> surface;
			_details::com_wrapper<ID2D1Bitmap1> bitmap;
			com_check(chain->GetBuffer(0, IID_PPV_ARGS(surface.get_ref())));
			com_check(_d2d_device_context->CreateBitmapFromDxgiSurface(
				surface.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
					D2D1::PixelFormat(pixel_format, D2D1_ALPHA_MODE_PREMULTIPLIED),
					96.0f, 96.0f // TODO dpi
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
			actual_data.target = _create_bitmap_from_swap_chain(actual_data.swap_chain.get());
			data.emplace<_window_data>(actual_data);
			// listen to size_changed
			wnd.size_changed += [this, pwnd = &wnd](ui::size_changed_info & info) {
				auto &data = _window_data::get(*pwnd);
				data.target.reset(); // gotta release 'em all!
				com_check(data.swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));
				data.target = _create_bitmap_from_swap_chain(data.swap_chain.get()); // recreate bitmap
				InvalidateRect(pwnd->get_native_handle(), nullptr, true);
			};
		}
		/// Releases all resources.
		void _delete_window(ui::window_base &w) override {
			_get_window_data(w).reset();
		}
	};
}
