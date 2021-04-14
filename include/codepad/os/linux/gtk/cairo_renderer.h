#pragma once

/// \file
/// Linux implementation of the Cairo renderer.

#include "codepad/ui/backends/cairo_renderer_base.h"
#include "window.h"

namespace codepad::os {
	/// Linux implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	public:
		std::shared_ptr<ui::bitmap> load_bitmap(const std::filesystem::path &filename, vec2d scale) override {
			// TODO
			return nullptr;
		}

		/// Pushes the new context onto the stack, using \ref window::_cairo_context.
		void begin_drawing(ui::window &wnd) override {
			auto *context = _get_window_data_as<_window_data>(wnd).context.get();
			assert_true_usage(context != nullptr, "manually beginning drawing to windows is not allowed");
			_render_stack.emplace(context, &wnd);
		}
	protected:
		/// Event handler of the `draw' signal. Sets \ref _renderer_data to the given \p cairo_t, invokes
		/// \ref _on_render(), and resets \ref _renderer_data.
		inline static gboolean _on_draw_event(GtkWidget*, cairo_t *cr, ui::window *wnd) {
			auto &data = _get_window_data_as<_window_data>(*wnd);
			data.context.set_share(cr);
			wnd->_on_render();
			data.context.reset();
			return true;
		}

		/// Does nothing.
		void _finish_drawing_to_target() override {
		}

		/// Creates a similar surface using \p gdk_window_create_similar_surface().
		ui::_details::gtk_object_ref<cairo_surface_t> _create_similar_surface(
			ui::window &wnd, int width, int height
		) override {
			auto &wnd_impl = _details::cast_window_impl(wnd.get_impl());
			return ui::_details::make_gtk_object_ref_give(gdk_window_create_similar_surface(
				gtk_widget_get_window(wnd_impl.get_native_handle()),
				CAIRO_CONTENT_COLOR_ALPHA, width, height
			));
		}

		/// Connects the \p draw signal to \ref _on_draw_event().
		void _new_window(ui::window &wnd) override {
			renderer_base::_new_window(wnd);
			auto &wnd_impl = _details::cast_window_impl(wnd.get_impl());
			g_signal_connect(
				wnd_impl.get_native_handle(), "draw",
				reinterpret_cast<GCallback>(_on_draw_event), &wnd
			);
		}
	};
}
