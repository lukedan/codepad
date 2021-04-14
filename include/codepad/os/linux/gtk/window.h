// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "codepad/ui/manager.h"
#include "codepad/ui/window.h"
#include "misc.h"

namespace codepad::os {
	class cairo_renderer;

	/// Implementation of GTK-based windows.
	class window_impl : public ui::_details::window_impl {
		friend ui::element;
		friend cairo_renderer;
	public:
		using native_handle_t = GtkWidget*;

		/// Creates a window, sets its attributes, and connects signals.
		explicit window_impl(ui::window&);
		/// Destroys the input method context and the window.
		~window_impl() {
			g_object_unref(_imctx);
			// calling g_object_unref(_wnd) will cause gtk to emit warnings for no apparent reason
			gtk_widget_destroy(_wnd);
		}

		/// Returns the native handle of this window.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return _wnd;
		}
	protected:
		void _set_caption(const std::u8string &cap) override {
			gtk_window_set_title(GTK_WINDOW(_wnd), reinterpret_cast<const gchar*>(cap.c_str()));
		}
		vec2d _get_position() const override {
			gint x, y;
			gtk_window_get_position(GTK_WINDOW(_wnd), &x, &y);
			return vec2d(x, y);
		}
		void _set_position(vec2d pos) override {
			gtk_window_move(GTK_WINDOW(_wnd), static_cast<gint>(pos.x), static_cast<gint>(pos.y));
		}
		vec2d _get_client_size() const override {
			// TODO scaling
			gint w, h;
			gtk_window_get_size(GTK_WINDOW(_wnd), &w, &h);
			return vec2d(w, h);
		}
		void _set_client_size(vec2d sz) override {
			// TODO scaling
			gint x = static_cast<gint>(sz.x), y = static_cast<gint>(sz.y);
			gtk_window_resize(GTK_WINDOW(_wnd), x, y);
			if (!gtk_window_get_resizable(GTK_WINDOW(_wnd))) {
				// this is because gtk_window_set_resizable(wnd, false) causes all gtk_window_resize() calls to
				// become ineffective, and we have to use this to control the window's size
				// TODO without this if, windows will carry this minimum size constraint even if they are sizable
				//      however with this if when set_client_size() is called before set_sizable() it won't work
				gtk_widget_set_size_request(_wnd, x, y);
			}
		}
		/// Gets and returns the scaling factor by calling \p gtk_widget_get_scale_factor().
		vec2d _get_scaling_factor() const override {
			gint scale = gtk_widget_get_scale_factor(_wnd);
			return vec2d(scale, scale);
		}

		void _activate() override {
			gtk_window_present(GTK_WINDOW(_wnd));
		}
		void _prompt_ready() override {
			gtk_window_set_urgency_hint(GTK_WINDOW(_wnd), true);
		}

		/// Shows the window by calling \p gtk_widget_show_now().
		void _show() override {
			// here we use show_now to avoid other calls failing due to the window being not ready. this however
			// means that some events may get processed in the mean time which may have unintended side effects
			gtk_widget_show_now(_wnd);
		}
		// use default implementation of show_and_activate()
		/// Hides the window by calling \p gtk_widget_hide().
		void _hide() override {
			gtk_widget_hide(_wnd);
		}

		/// Calls \p gtk_widget_queue_draw().
		void _invalidate_window_visuals() override {
			gtk_widget_queue_draw(_wnd);
		}

		/// Calls \p gdk_window_set_functions() to set a hint for the window manager to show or hide the maximize
		/// button.
		///
		/// \todo This doesn't work before the window has been realized.
		void _set_display_maximize_button(bool disp) override {
			gdk_window_set_functions(
				gtk_widget_get_window(_wnd),
				static_cast<GdkWMFunction>((disp ? 0 : GDK_FUNC_ALL) | GDK_FUNC_MAXIMIZE)
			);
		}
		/// Calls \p gdk_window_set_functions() to set a hint for the window manager to show or hide the minimize
		/// button.
		///
		/// \todo This doesn't work before the window has been realized.
		void _set_display_minimize_button(bool disp) override {
			gdk_window_set_functions(
				gtk_widget_get_window(_wnd),
				static_cast<GdkWMFunction>((disp ? 0 : GDK_FUNC_ALL) | GDK_FUNC_MINIMIZE)
			);
		}
		void _set_display_caption_bar(bool) override {
			// TODO
		}
		/// Sets whether this window is decorated by calling \p gtk_window_set_decorated().
		void _set_display_border(bool disp) override {
			gtk_window_set_decorated(GTK_WINDOW(_wnd), disp);
		}
		/// Sets whether this window is resizable by calling \p gtk_window_set_resizable().
		void _set_sizable(bool size) override {
			gtk_window_set_resizable(GTK_WINDOW(_wnd), size);
		}
		/// Sets whether this window is always above other ordinary windows by calling \p gtk_window_set_keep_above.
		void _set_topmost(bool topmost) override {
			gtk_window_set_keep_above(GTK_WINDOW(_wnd), topmost);
		}
		/// Sets whether this winodw has a taskbar icon by calling \p gtk_window_set_skip_taskbar_hint() and
		/// \p gtk_window_set_skip_pager_hint().
		void _set_show_icon(bool show) override {
			GtkWindow *wnd = GTK_WINDOW(_wnd);
			gtk_window_set_skip_taskbar_hint(wnd, show);
			gtk_window_set_skip_pager_hint(wnd, show);
		}

		vec2d _screen_to_client(vec2d v) const override {
			GdkWindow *gdkwnd = gtk_widget_get_window(_wnd);
			gdouble x = v.x, y = v.y;
			if (gdk_window_get_effective_parent(gdkwnd) == nullptr) { // fast common situation
				gdk_window_coords_from_parent(gdkwnd, x, y, &x, &y);
			} else { // walk down the hierarchy
				std::vector<GdkWindow*> wnds;
				for (GdkWindow *cur = gdkwnd; cur != nullptr; cur = gdk_window_get_effective_parent(cur)) {
					wnds.push_back(cur);
				}
				for (auto i = wnds.rbegin(); i != wnds.rend(); ++i) {
					gdk_window_coords_from_parent(*i, x, y, &x, &y);
				}
			}
			// TODO scaling?
			return vec2d(x, y);
		}
		vec2d _client_to_screen(vec2d v) const override {
			gint x, y;
			// TODO scaling?
			gdk_window_get_root_coords(gtk_widget_get_window(_wnd), v.x, v.y, &x, &y);
			return vec2d(x, y);
		}

		void _set_mouse_capture() override {
			// here owner_events is set to false so that all events are sent to this particular window instead of only
			// to windows in the application
			//
			// TODO tab buttons are still freaking out when dragged
			GdkGrabStatus status = gdk_seat_grab(
				gdk_display_get_default_seat(gdk_display_get_default()),
				gtk_widget_get_window(_wnd), GDK_SEAT_CAPABILITY_ALL_POINTING, false,
				nullptr, nullptr, nullptr, nullptr
			);
			if (status != GDK_GRAB_SUCCESS) {
				logger::get().log_error(CP_HERE) << "grab failed: " << status;
				assert_true_sys(false);
			}
		}
		void _release_mouse_capture() override {
			gdk_seat_ungrab(gdk_display_get_default_seat(gdk_display_get_default()));
		}

		void _set_active_caret_position(rectd pos) override {
			recti rpos = pos.fit_grid_enlarge<int>();
			GdkRectangle rect{};
			rect.x = rpos.xmin;
			rect.y = rpos.ymin;
			rect.width = rpos.width();
			rect.height = rpos.height();
			gtk_im_context_set_cursor_location(_imctx, &rect);
		}
		void _interrupt_input_method() override {
			gtk_im_context_reset(_imctx);
		}
	protected:
		template<typename Inf, typename ...Args> inline static void _form_onevent(
			ui::window &w, void (ui::window::*handle)(Inf &), Args &&...args
		) {
			Inf inf(std::forward<Args>(args)...);
			(w.*handle)(inf);
		}

		// TODO on scaling factor changed

		static gboolean _on_delete_event(GtkWidget*, GdkEvent*, window_impl*);
		inline static gboolean _on_leave_notify_event(GtkWidget*, GdkEvent*, window_impl *wnd) {
			wnd->_window._on_mouse_leave();
			return true;
		}
		static gboolean _on_motion_notify_event(GtkWidget*, GdkEvent*, window_impl*);
		inline static void _on_size_allocate(GtkWidget*, GdkRectangle *rect, window_impl *wnd) {
			wnd->_window._layout = rectd(
				0.0, static_cast<double>(rect->width), 0.0, static_cast<double>(rect->height)
			);
			_form_onevent<ui::window::size_changed_info>(
				wnd->_window, &ui::window::_on_size_changed, vec2d(rect->width, rect->height)
			);
		}
		static gboolean _on_button_press_event(GtkWidget*, GdkEvent*, window_impl*);
		static gboolean _on_button_release_event(GtkWidget*, GdkEvent*, window_impl*);
		inline static gboolean _on_focus_in_event(GtkWidget*, GdkEvent*, window_impl *wnd) {
			gtk_im_context_focus_in(wnd->_imctx);
			wnd->_window.get_manager().get_scheduler().set_focused_element(&wnd->_window);
			return true;
		}
		inline static gboolean _on_focus_out_event(GtkWidget*, GdkEvent*, window_impl *wnd) {
			gtk_im_context_focus_out(wnd->_imctx);
			wnd->_window.get_manager().get_scheduler().set_focused_element(nullptr);
			return true;
		}
		inline static gboolean _on_grab_broken_event(GtkWidget*, GdkEvent*, window_impl *wnd) {
			wnd->_window._on_lost_window_capture();
			return true;
		}
		static gboolean _on_key_press_event(GtkWidget*, GdkEvent*, window_impl*);
		static gboolean _on_key_release_event(GtkWidget*, GdkEvent*, window_impl*);
		static gboolean _on_scroll_event(GtkWidget*, GdkEvent*, window_impl*);

		inline static void _on_im_commit(GtkIMContext*, gchar *str, window_impl *wnd) {
			ui::text_info inf(reinterpret_cast<const char8_t*>(str));
			wnd->_window._on_keyboard_text(inf);
		}
		/// \todo Also notify of attributes and cursor position.
		inline static void _on_im_preedit_changed(GtkIMContext*, window_impl *wnd) {
			gchar *str = nullptr;
			PangoAttrList *attrs = nullptr;
			gint cursor_pos;
			gtk_im_context_get_preedit_string(wnd->_imctx, &str, &attrs, &cursor_pos);
			{
				PangoAttrIterator *iter = pango_attr_list_get_iterator(attrs);
				do {
					/*PangoAttribute
						*style = pango_attr_iterator_get(iter, PANGO_ATTR_STYLE),
						*weight = pango_attr_iterator_get(iter, PANGO_ATTR_WEIGHT),
						*underline = pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);*/
					// TODO process the styles
				} while (pango_attr_iterator_next(iter));
				pango_attr_iterator_destroy(iter);
			}
			_form_onevent<ui::composition_info>(
				wnd->_window, &ui::window::_on_composition, reinterpret_cast<const char8_t*>(str)
			);
			g_free(str);
			pango_attr_list_unref(attrs);
		}
		inline static void _on_im_preedit_end(GtkIMContext*, window_impl *wnd) {
			wnd->_window._on_composition_finished();
		}

		template<typename Obj, typename Func> void _connect_signal(Obj obj, const gchar *name, Func callback) {
			g_signal_connect(obj, name, reinterpret_cast<GCallback>(callback), this);
		}

		GtkWidget *_wnd = nullptr; ///< The \p GtkWindow.
		GtkIMContext *_imctx = nullptr; ///< GTK context for input methods.

		/// Timestamp of the previous scroll event used for eliminating duplicate events.
		guint32 _prev_scroll_timestamp = 0;
		ui::scheduler::task_token _kinetic_token; ///< Token for updating kinetic scrolling.
	};

	namespace _details {
		/// Casts a \ref ui::window_base to a \ref window.
		inline window_impl &cast_window_impl(ui::_details::window_impl &w) {
			auto *wnd = dynamic_cast<window_impl*>(&w);
			assert_true_usage(wnd, "invalid window implementation type");
			return *wnd;
		}
	}
}
