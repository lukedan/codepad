#pragma once

#include <gtk/gtk.h>

#include "../../window.h"
#include "misc.h"

namespace codepad::os {
	/// \todo Window resizing is problematic.
	class window : public window_base {
		friend class opengl_renderer;
		friend class software_renderer;
		friend class ui::element;
	public:
		using native_handle_t = GdkWindow*;

		void set_caption(const str_t &cap) override {
			gdk_window_set_title(_wnd, convert_to_utf8(cap).c_str());
		}
		vec2i get_position() const override {
			gint x, y;
			gdk_window_get_origin(_wnd, &x, &y);
			return vec2i(x, y);
		}
		/// \todo \p gdk_window_move is relative to the parent window.
		/// \todo Calls prior to the mapping of the window will still get wrong results. Not fully tested otherwise.
		void set_position(vec2i pos) override {
			gint wx, wy, cx, cy;
			gdk_window_get_root_origin(_wnd, &wx, &wy);
			gdk_window_get_origin(_wnd, &cx, &cy);
			gdk_window_move(_wnd, pos.x + wx - cx, pos.y + wy - cy);
		}
		vec2i get_client_size() const override {
			return vec2i(gdk_window_get_width(_wnd), gdk_window_get_height(_wnd));
		}
		void set_client_size(vec2i sz) override {
			gdk_window_resize(_wnd, sz.x, sz.y);
		}

		void activate() override {
			gdk_window_raise(_wnd);
			gdk_window_focus(_wnd, GDK_CURRENT_TIME);
		}
		void prompt_ready() override {
			gdk_window_set_urgency_hint(_wnd, true);
		}

		void set_display_maximize_button(bool disp) override {
			_set_decoration_bit(GDK_DECOR_MAXIMIZE, disp);
		}
		void set_display_minimize_button(bool disp) override {
			_set_decoration_bit(GDK_DECOR_MINIMIZE, disp);
		}
		void set_display_caption_bar(bool disp) override {
			_set_decoration_bit(GDK_DECOR_TITLE, disp);
		}
		void set_display_border(bool disp) override {
			_set_decoration_bit(GDK_DECOR_BORDER, disp);
		}
		void set_sizable(bool size) override {
			gtk_window_set_resizable(GTK_WINDOW(_wnd), size);
		}

		bool hit_test_full_client(vec2i v) const override {
			GdkRectangle rect{};
			gdk_window_get_frame_extents(_wnd, &rect);
			return recti::from_xywh(rect.x, rect.y, rect.width, rect.height).contains(v);
		}

		vec2i screen_to_client(vec2i v) const override {
			gdouble x = v.x, y = v.y;
			if (gdk_window_get_effective_parent(_wnd) == nullptr) { // fast common situation
				gdk_window_coords_from_parent(_wnd, x, y, &x, &y);
			} else { // walk down the hierarchy
				std::vector<GdkWindow*> wnds;
				for (GdkWindow *cur = _wnd; cur != nullptr; cur = gdk_window_get_effective_parent(cur)) {
					wnds.push_back(cur);
				}
				for (auto i = wnds.rbegin(); i != wnds.rend(); ++i) {
					gdk_window_coords_from_parent(*i, x, y, &x, &y);
				}
			}
			return vec2i(static_cast<int>(x), static_cast<int>(y));
		}
		vec2i client_to_screen(vec2i v) const override {
			gint x, y;
			gdk_window_get_root_coords(_wnd, v.x, v.y, &x, &y);
			return vec2i(x, y);
		}

		void set_mouse_capture(ui::element &elem) override {
			window_base::set_mouse_capture(elem);
			gdk_seat_grab(
				gdk_display_get_default_seat(gdk_display_get_default()),
				_wnd, GDK_SEAT_CAPABILITY_ALL_POINTING, true,
				nullptr, nullptr, nullptr, nullptr
			);
		}
		void release_mouse_capture() override {
			window_base::release_mouse_capture();
			gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default()));
		}

		native_handle_t get_native_handle() const {
			return _wnd;
		}

		inline static str_t get_default_class() {
			return CP_STRLIT("window");
		}
	protected:
		const static gchar *_window_pointer_key;

		explicit window(window *parent = nullptr) {
			_details::check_init_gtk();
			GdkWindowAttr attribs{};
			attribs.window_type = parent ? GDK_WINDOW_CHILD : GDK_WINDOW_TOPLEVEL;
			attribs.event_mask =
				GDK_POINTER_MOTION_MASK |
					GDK_BUTTON_PRESS_MASK|
					GDK_BUTTON_RELEASE_MASK |
					GDK_KEY_PRESS_MASK |
					GDK_KEY_RELEASE_MASK |
					GDK_LEAVE_NOTIFY_MASK |
					GDK_STRUCTURE_MASK |
					GDK_FOCUS_CHANGE_MASK |
					GDK_SMOOTH_SCROLL_MASK;
			attribs.width = 800;
			attribs.height = 600;
			attribs.wclass = GDK_INPUT_OUTPUT;
			_wnd = gdk_window_new(parent ? parent->_wnd : nullptr, &attribs, 0);
			gdk_window_set_focus_on_map(_wnd, true);
			gdk_window_set_event_compression(_wnd, false);
			g_object_set_data(G_OBJECT(_wnd), _window_pointer_key, this);
		}

		/// \todo see if this works
		void _set_decoration_bit(GdkWMDecoration bit, bool val) {
			if (val) {
				gdk_window_set_decorations(_wnd, bit);
			} else {
				gdk_window_set_decorations(_wnd, static_cast<GdkWMDecoration>(GDK_DECOR_ALL | bit));
			}
		}

		static bool _idle();
		void _on_update() override {
			while (_idle()) {
			}
			_update_drag();
			ui::manager::get().schedule_update(*this);
		}
		void _on_prerender() override {
			window_base::_on_prerender();
		}

		void _initialize() override {
			window_base::_initialize();
			ui::manager::get().schedule_update(*this);
			gdk_window_show(_wnd);
		}
		void _dispose() override {
			gdk_window_destroy(_wnd);
			g_object_unref(_wnd);
			window_base::_dispose();
		}

		GdkWindow *_wnd = nullptr;
		gint _width = 0, _height = 0;
	};
}
