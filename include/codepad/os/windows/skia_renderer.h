// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Base class of Skia renderers.

#include <gl/GL.h>

#include <skia/core/SkSurfaceProps.h>
#include <pango/pangowin32.h>

#include "codepad/ui/backends/skia_renderer_base.h"
#include "misc.h"

namespace codepad::os {
	/// Base class of the Skia renderer. Contains platform-independent code.
	class skia_renderer : public ui::skia::renderer_base {
	public:
		/// Provides the base renderer with a Win32 Pango font map.
		skia_renderer() : ui::skia::renderer_base(pango_win32_font_map_for_display()) {
		}
		/// Calls \p wglDeleteContext() to free \ref _gl_context.
		~skia_renderer() {
			_skia_context.reset();
			if (_gl_context) {
				_details::winapi_check(wglMakeCurrent(nullptr, nullptr));
				_details::winapi_check(wglDeleteContext(_gl_context));
			}
		}
	private:
		/// Renderer data associated with a window.
		struct _window_data {
			sk_sp<SkSurface> surface; ///< The skia surface.
		};

		PIXELFORMATDESCRIPTOR _pixel_format_descriptor{}; ///< Describes \ref _pixel_format.
		int _pixel_format = 0; ///< The pixel format chosen for windows.
		HGLRC _gl_context = nullptr; ///< The OpenGL rendering context.


		/// Creates a surface for the window, and registers handlers for when the surface needs to be recreated.
		/// Initializes \ref _gl_context if necessary.
		void _new_window(ui::window &wnd) override {
			window_impl &wnd_impl = _details::cast_window_impl(wnd.get_impl());

			HDC hdc = GetDC(wnd_impl.get_native_handle());
			_details::winapi_check(hdc);
			if (_gl_context == nullptr) {
				_pixel_format_descriptor.nSize = sizeof(_pixel_format_descriptor);
				_pixel_format_descriptor.nVersion = 1;
				_pixel_format_descriptor.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DEPTH_DONTCARE;
				_pixel_format_descriptor.iPixelType = PFD_TYPE_RGBA;
				_pixel_format_descriptor.cColorBits = 24; // this does not include the alpha bit planes
				_pixel_format_descriptor.cAlphaBits = 8;
				_pixel_format_descriptor.iLayerType = PFD_MAIN_PLANE;
				_pixel_format = ChoosePixelFormat(hdc, &_pixel_format_descriptor);
				_details::winapi_check(_pixel_format);

				_details::winapi_check(DescribePixelFormat(
					hdc, _pixel_format, sizeof(PIXELFORMATDESCRIPTOR), &_pixel_format_descriptor)
				);
				// TODO maybe log the actual parameters of the pixel format

				_details::winapi_check(SetPixelFormat(hdc, _pixel_format, &_pixel_format_descriptor));

				_gl_context = wglCreateContext(hdc);
				_details::winapi_check(_gl_context);

				_details::winapi_check(wglMakeCurrent(hdc, _gl_context));

				_skia_context = GrDirectContext::MakeGL();
			} else {
				_details::winapi_check(SetPixelFormat(hdc, _pixel_format, &_pixel_format_descriptor));
			}
			ReleaseDC(wnd_impl.get_native_handle(), hdc);

			auto &data = _get_window_data(wnd).emplace<_window_data>();
			data.surface = _create_surface_for_window(wnd, wnd.get_scaling_factor());
			// resize buffer when the window size has changed
			wnd.size_changed += [this, pwnd = &wnd](ui::window::size_changed_info&) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.surface.reset();
				data.surface = _create_surface_for_window(*pwnd, pwnd->get_scaling_factor());
				pwnd->invalidate_visual();
			};
			// reallocate buffer when the scaling factor has changed
			wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window::scaling_factor_changed_info &p) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.surface.reset();
				data.surface = _create_surface_for_window(*pwnd, p.new_value);
				pwnd->invalidate_visual();
			};
		}
		/// Since the \ref _window_data is automatically destroyed when the window closes, no clean-up is necessary.
		void _delete_window(ui::window&) override {
		}

		/// Returns \ref _window_data::surface.
		SkSurface *_get_surface_for_window(ui::window &wnd) const override {
			return _get_window_data_as<_window_data>(wnd).surface.get();
		}
		/// Invokes \p wglMakeCurrent() to start drawing to the given window.
		void _start_drawing_to_window(ui::window &wnd) override {
			window_impl &wnd_impl = _details::cast_window_impl(wnd.get_impl());
			HDC hdc = GetDC(wnd_impl.get_native_handle());
			_details::winapi_check(hdc);
			_details::winapi_check(wglMakeCurrent(hdc, _gl_context));
			ReleaseDC(wnd_impl.get_native_handle(), hdc);

			// TODO just retrieve the window size directly
			vec2d size = wnd.get_client_size(), scaling = wnd.get_scaling_factor();
			size.x *= scaling.x;
			size.y *= scaling.y;
			glViewport(0, 0, static_cast<GLsizei>(std::ceil(size.x)), static_cast<GLsizei>(std::ceil(size.y)));
		}
		/// Invokes \p SwapBuffers().
		void _finish_drawing_to_window(ui::window &wnd) override {
			_skia_context->flushAndSubmit(true);

			window_impl &wnd_impl = _details::cast_window_impl(wnd.get_impl());
			HDC hdc = GetDC(wnd_impl.get_native_handle());
			_details::winapi_check(hdc);
			_details::winapi_check(SwapBuffers(hdc));
			ReleaseDC(wnd_impl.get_native_handle(), hdc);

			GLenum error = glGetError();
			if (error != GL_NO_ERROR) {
				logger::get().log_error() << "OpenGL error: " << error;
				assert_true_sys(false);
			}
		}
	};
}
