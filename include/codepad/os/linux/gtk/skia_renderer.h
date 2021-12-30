// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Skia renderer implementation using GTK.

#include <GL/gl.h>

#include <skia/gpu/gl/GrGLInterface.h>

#include <pango/pangoft2.h>

#include "codepad/ui/backends/skia_renderer_base.h"

namespace codepad::os {
	/// Skia renderer for GTK.
	class skia_renderer : public ui::skia::renderer_base {
	public:
		/// Initializes the renderer base using a FreeType font map.
		skia_renderer() : renderer_base(pango_ft2_font_map_new()) {
		}
	protected:
		/// Skia data associated with a \ref window.
		struct _window_data {
			sk_sp<SkSurface> surface; ///< The Skia surface.
			GtkGLArea *gl_area = nullptr; ///< The OpenGL viewport.
			/// The renderer. This is necessary because we only have one pointer for each GTK callback.
			skia_renderer *renderer = nullptr;
		};

		/// Initializes the Skia surface for the window.
		inline static void _on_gl_area_realize(GtkGLArea *area, ui::window *wnd) {
			auto &data = _get_window_data_as<_window_data>(*wnd);
			gtk_gl_area_make_current(area);
			if (GError *error = gtk_gl_area_get_error(area)) {
				assert_true_sys(false, "GL error");
			}
			if (!data.renderer->_skia_context) {
				int major, minor;
				gdk_gl_context_get_version(gtk_gl_area_get_context(area), &major, &minor);
				logger::get().log_debug() << "OpenGL version: " << major << "." << minor;
				data.renderer->_skia_context = GrDirectContext::MakeGL();
			}
			data.surface = data.renderer->_create_surface_for_window(*wnd, wnd->get_scaling_factor());
		};
		/// Renders the window.
		inline static gboolean _on_gl_area_render(GtkGLArea *area, GdkGLContext *context, ui::window *wnd) {
			auto &data = _get_window_data_as<_window_data>(*wnd);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			_details::cast_window_impl(wnd->get_impl())._on_render();
			data.renderer->_skia_context->flush();
			if (GError *error = gtk_gl_area_get_error(area)) {
				assert_true_sys(false, "GL error");
			}
			return true;
		}

		/// Creates a new \p GtkGLArea and adds it to the window.
		void _new_window(ui::window &wnd) override {
			window_impl &wnd_impl = _details::cast_window_impl(wnd.get_impl());
			auto &data = _get_window_data(wnd).emplace<_window_data>();
			data.gl_area = GTK_GL_AREA(gtk_gl_area_new());
			data.renderer = this;

			g_signal_connect(
				data.gl_area, "realize",
				reinterpret_cast<GCallback>(_on_gl_area_realize), &wnd
			);
			g_signal_connect(
				data.gl_area, "render",
				reinterpret_cast<GCallback>(_on_gl_area_render), &wnd
			);

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

			gtk_container_add(GTK_CONTAINER(wnd_impl.get_native_handle()), GTK_WIDGET(data.gl_area));
			gtk_widget_show(GTK_WIDGET(data.gl_area));
		}
		void _delete_window(ui::window&) override {
			// TODO anything needs to be done?
		}

		/// Returns \ref _window_data::surface.
		SkSurface *_get_surface_for_window(ui::window &wnd) const override {
			return _get_window_data_as<_window_data>(wnd).surface.get();
		}

		/// Calls \p gtk_gl_area_make_current() and \p glViewport().
		void _start_drawing_to_window(ui::window &wnd) override {
			auto &data = _get_window_data_as<_window_data>(wnd);
			gtk_gl_area_make_current(data.gl_area);
			// adjust viewport size
			gint width, height;
			gtk_window_get_size(
				GTK_WINDOW(_details::cast_window_impl(wnd.get_impl()).get_native_handle()), &width, &height
			);
			glViewport(0, 0, width, height);
		}
		/// Does nothing.
		void _finish_drawing_to_window(ui::window&) override {
		}
	};
}
