#pragma once

/// \file
/// Linux implementation of the Cairo renderer.

#include "../../../ui/cairo_renderer_base.h"
#include "window.h"

namespace codepad::os {
	/// Linux implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	protected:
		/// Calls \p gtk_widget_queue_draw() on the window.
		void _finish_drawing_to_target() override {
			// TODO window flickers when resized
			if (_render_stack.top().window) {
				gtk_widget_queue_draw(_details::cast_window(*_render_stack.top().window).get_native_handle());
			}
		}

		/// Creates a Cairo surface using \p gdk_window_create_similar_image_surface();
		ui::cairo::_details::gtk_object_ref<cairo_surface_t> _create_surface_for_window(
			ui::window_base &w
		) override {
			GdkWindow *wnd = gtk_widget_get_window(_details::cast_window(w).get_native_handle());
			gint scale = 1;
			int width = 0, height = 0;
			if (wnd) {
				scale = gdk_window_get_scale_factor(wnd);
				width = gdk_window_get_width(wnd) * scale;
				height = gdk_window_get_height(wnd) * scale;
			} // otherwise if wnd is nullptr, it means that the window has not been realized yet
			return ui::cairo::_details::make_gtk_object_ref_give(
				gdk_window_create_similar_image_surface(
					wnd, CAIRO_FORMAT_ARGB32, width, height, scale
				));
		}
		/// Draws the rendered image onto the given context.
		inline static gboolean _refresh_window_contents(GtkWidget*, cairo_t *cr, window *wnd) {
			auto &data = _window_data::get(*wnd);
			cairo_set_source_surface(cr, data.get_surface(), 0, 0);
			cairo_paint(cr);
			return true;
		}
		/// In addition to regular initialization, also listens to
		void _new_window(ui::window_base &w) override {
			renderer_base::_new_window(w);

			window &wnd = _details::cast_window(w);
			g_signal_connect(
				wnd.get_native_handle(), "draw",
				reinterpret_cast<GCallback>(_refresh_window_contents), &wnd
			);
		}
	};
}
