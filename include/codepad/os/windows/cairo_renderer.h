// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Windows implementation of the Cairo renderer.

#include <cairo-win32.h>

#include "../../ui/backends/cairo_renderer_base.h"

namespace codepad::os {
	/// Windows implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	public:
		/// Pushes the window and its context onto the stack.
		void begin_drawing(ui::window_base &w) override {
			auto &data = _get_window_data_as<_window_data>(w);
			_render_stack.emplace(data.context.get(), &w);
		}
	protected:
		/// Flushes the surface if the target is a window.
		void _finish_drawing_to_target() override {
			if (_render_stack.top().window) {
				cairo_surface_flush(cairo_get_target(_render_stack.top().context));
			}
		}

		// TODO if you drag the window to the corner of the screen so that it's only partially visible, resize it,
		//      then drag it back, the resulting surface will incorrectly have a smaller size
		/// Creates a cairo context from a newly created Win32 surface.
		ui::_details::gtk_object_ref<cairo_t> _create_context_for_window(
			ui::window_base &w, vec2d scaling
		) {
			auto surface = ui::_details::make_gtk_object_ref_give(cairo_win32_surface_create_with_format(
				GetDC(_details::cast_window(w).get_native_handle()), CAIRO_FORMAT_ARGB32
			));
			cairo_surface_set_device_scale(surface.get(), scaling.x, scaling.y);
			auto result = ui::_details::make_gtk_object_ref_give(cairo_create(surface.get()));
			return result;
		}

		/// Creates a Cairo surface for the window, and listens to specific events to resize the surface as needed.
		void _new_window(ui::window_base &wnd) override {
			_window_data &data = _get_window_data(wnd).emplace<_window_data>();

			// create context
			data.context = _create_context_for_window(wnd, wnd.get_scaling_factor());
			// resize buffer when the window size has changed
			wnd.size_changed += [this, pwnd = &wnd](ui::window_base::size_changed_info&) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, pwnd->get_scaling_factor());
				pwnd->invalidate_visual();
			};
			// reallocate buffer when the window scaling has changed
			wnd.scaling_factor_changed += [this, pwnd = &wnd](ui::window_base::scaling_factor_changed_info &p) {
				auto &data = _get_window_data_as<_window_data>(*pwnd);
				data.context.reset();
				data.context = _create_context_for_window(*pwnd, p.new_value);
				pwnd->invalidate_visual();
			};
		}
	};
}
