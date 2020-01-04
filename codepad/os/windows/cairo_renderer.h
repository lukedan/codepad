// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Windows implementation of the Cairo renderer.

#ifdef CP_USE_CAIRO

#	include <cairo-win32.h>

#	include "../../ui/cairo_renderer_base.h"

namespace codepad::os {
	/// Windows implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	protected:
		/// Flushes the surface if the target is a window.
		void _finish_drawing_to_target() override {
			if (_render_stack.top().window) {
				cairo_surface_flush(cairo_get_target(_render_stack.top().context));
			}
		}

		// TODO if you drag the window to the corner of the screen so that it's only partially visible, resize it, then
		//      drag it back, the resulting surface will incorrectly have a smaller size
		/// Creates a Win32 surface.
		ui::cairo::_details::cairo_object_ref<cairo_surface_t> _create_surface_for_window(
			ui::window_base &w
		) override {
			return ui::cairo::_details::make_cairo_object_ref_give(cairo_win32_surface_create_with_format(
				GetDC(_details::cast_window(w).get_native_handle()), CAIRO_FORMAT_ARGB32
			));
		}
	};
}

#endif
