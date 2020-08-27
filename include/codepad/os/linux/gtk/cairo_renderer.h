#pragma once

/// \file
/// Linux implementation of the Cairo renderer.

#include "../../../ui/cairo_renderer_base.h"
#include "window.h"

namespace codepad::os {
	/// Linux implementation of the Cairo renderer.
	class cairo_renderer : public ui::cairo::renderer_base {
	public:
		/// Pushes the new context onto the stack, using \ref window::_cairo_context.
		void begin_drawing(ui::window_base &wnd) override {
			auto *context = std::any_cast<cairo_t*>(&_get_window_data(wnd));
			assert_true_usage(context != nullptr, "manually beginning drawing to windows is not allowed");
			_render_stack.emplace(*context, &wnd);
		}
	protected:
		/// Does nothing.
		void _finish_drawing_to_target() override {
		}
		/// Does nothing.
		void _new_window(ui::window_base&) override {
		}
	};
}
