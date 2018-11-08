// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "../../../ui/manager.h"
#include "../../window.h"
#include "misc.h"

namespace codepad::os {
	class window : public window_base {
		friend class opengl_renderer;
		friend class software_renderer;
		friend class ui::element;
	public:
		using native_handle_t = GtkWidget*;

		explicit window(window *parent = nullptr) {
			_wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);
			// connect signals
			_connect_signal(_wnd, "delete_event", _on_delete_event);
			_connect_signal(_wnd, "leave-notify-event", _on_leave_notify_event);
			_connect_signal(_wnd, "motion-notify-event", _on_motion_notify_event);
			_connect_signal(_wnd, "size-allocate", _on_size_allocate);
			_connect_signal(_wnd, "button-press-event", _on_button_press_event);
			_connect_signal(_wnd, "button-release-event", _on_button_release_event);
			_connect_signal(_wnd, "focus-in-event", _on_focus_in_event);
			_connect_signal(_wnd, "focus-out-event", _on_focus_out_event);
			_connect_signal(_wnd, "key-press-event", _on_key_press_event);
			_connect_signal(_wnd, "key-release-event", _on_key_release_event);
			_connect_signal(_wnd, "scroll-event", _on_scroll_event);
			// set gravity to static so that coordinates are relative to the client region
			gtk_window_set_gravity(GTK_WINDOW(_wnd), GDK_GRAVITY_STATIC);
			gtk_window_set_focus_on_map(GTK_WINDOW(_wnd), true);
			gtk_widget_set_app_paintable(_wnd, true); // not sure if this is useful
			gtk_widget_add_events(
				_wnd,
				GDK_POINTER_MOTION_MASK |
					GDK_LEAVE_NOTIFY_MASK |
					GDK_BUTTON_PRESS_MASK |
					GDK_BUTTON_RELEASE_MASK |
					GDK_SCROLL_MASK |
					GDK_FOCUS_CHANGE_MASK
			);
		}

		void set_caption(const str_t &cap) override {
			gtk_window_set_title(GTK_WINDOW(_wnd), cap.c_str());
		}
		vec2i get_position() const override {
			gint x, y;
			gtk_window_get_position(GTK_WINDOW(_wnd), &x, &y);
			return vec2i(x, y);
		}
		void set_position(vec2i pos) override {
			gtk_window_move(GTK_WINDOW(_wnd), pos.x, pos.y);
		}
		vec2i get_client_size() const override {
			gint w, h;
			gtk_window_get_size(GTK_WINDOW(_wnd), &w, &h);
			return vec2i(w, h);
		}
		void set_client_size(vec2i sz) override {
			gtk_window_resize(GTK_WINDOW(_wnd), sz.x, sz.y);
		}

		void activate() override {
			gtk_window_present(GTK_WINDOW(_wnd));
		}
		void prompt_ready() override {
			gtk_window_set_urgency_hint(GTK_WINDOW(_wnd), true);
		}

		void set_display_maximize_button(bool disp) override {
			// TODO
		}
		void set_display_minimize_button(bool disp) override {
			// TODO
		}
		void set_display_caption_bar(bool disp) override {
			// TODO
		}
		void set_display_border(bool disp) override {
			// TODO
		}
		void set_sizable(bool size) override {
			gtk_window_set_resizable(GTK_WINDOW(_wnd), size);
		}

		bool hit_test_full_client(vec2i v) const override {
			GdkRectangle rect{};
			gdk_window_get_frame_extents(gtk_widget_get_window(_wnd), &rect);
			return recti::from_xywh(rect.x, rect.y, rect.width, rect.height).contains(v);
		}

		vec2i screen_to_client(vec2i v) const override {
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
			return vec2i(static_cast<int>(x), static_cast<int>(y));
		}
		vec2i client_to_screen(vec2i v) const override {
			gint x, y;
			gdk_window_get_root_coords(gtk_widget_get_window(_wnd), v.x, v.y, &x, &y);
			return vec2i(x, y);
		}

		void set_active_caret_position(rectd pos) override {
			recti rpos = pos.fit_grid_enlarge<int>();
			GdkRectangle rect{};
			rect.x = rpos.xmin;
			rect.y = rpos.ymin;
			rect.width = rpos.width();
			rect.height = rpos.height();
			gtk_im_context_set_cursor_location(_imctx, &rect);
		}
		void interrupt_input_method() override {
			gtk_im_context_reset(_imctx);
		}

		void set_mouse_capture(ui::element &elem) override {
			window_base::set_mouse_capture(elem);
			gdk_seat_grab(
				gdk_display_get_default_seat(gdk_display_get_default()),
				gtk_widget_get_window(_wnd), GDK_SEAT_CAPABILITY_ALL_POINTING, true,
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
		template<typename Inf, typename ...Args> inline static void _form_onevent(
			window &w, void (window::*handle)(Inf &), Args &&...args
		) {
			Inf inf(std::forward<Args>(args)...);
			(w.*handle)(inf);
		}

		static gboolean _on_delete_event(GtkWidget*, GdkEvent*, window*);
		inline static gboolean _on_leave_notify_event(GtkWidget *widget, GdkEvent *ev, window *wnd) {
			wnd->_on_mouse_leave();
			return true;
		}
		static gboolean _on_motion_notify_event(GtkWidget*, GdkEvent*, window*);
		inline static void _on_size_allocate(GtkWidget *widget, GdkRectangle *rect, window *wnd) {
			wnd->_layout = rectd(0.0, static_cast<double>(rect->width), 0.0, static_cast<double>(rect->height));
			_form_onevent<size_changed_info>(*wnd, &window::_on_size_changed, vec2i(rect->width, rect->height));
		}
		static gboolean _on_button_press_event(GtkWidget*, GdkEvent*, window*);
		static gboolean _on_button_release_event(GtkWidget*, GdkEvent*, window*);
		inline static gboolean _on_focus_in_event(GtkWidget*, GdkEvent*, window *wnd) {
			wnd->_on_got_window_focus();
			return true;
		}
		inline static gboolean _on_focus_out_event(GtkWidget*, GdkEvent*, window *wnd) {
			wnd->_on_lost_window_focus();
			return true;
		}
		inline static gboolean _on_grab_broken_event(GtkWidget*, GdkEvent*, window *wnd) {
			wnd->_on_lost_window_capture();
			return true;
		}
		static gboolean _on_key_press_event(GtkWidget*, GdkEvent*, window*);
		static gboolean _on_key_release_event(GtkWidget*, GdkEvent*, window*);
		/// \todo Horizontal scrolling is ignored.
		inline static gboolean _on_scroll_event(GtkWidget*, GdkEvent *event, window *wnd) {
			_form_onevent<ui::mouse_scroll_info>(
				*wnd, &window::_on_mouse_scroll, -event->scroll.delta_y, vec2d(event->scroll.x, event->scroll.y)
			);
			return true;
		}

		inline static void _on_im_commit(GtkIMContext*, gchar *str, window *wnd) {
			ui::text_info inf(str);
			wnd->_on_keyboard_text(inf);
		}
		/// \todo Also notify of attributes and cursor position.
		inline static void _on_im_preedit_changed(GtkIMContext*, window *wnd) {
			gchar *str = nullptr;
			PangoAttrList *attrs = nullptr;
			gint cursor_pos;
			gtk_im_context_get_preedit_string(wnd->_imctx, &str, &attrs, &cursor_pos);
			{
				PangoAttrIterator *iter = pango_attr_list_get_iterator(attrs);
				do {
					PangoAttribute
						*style = pango_attr_iterator_get(iter, PANGO_ATTR_STYLE),
						*weight = pango_attr_iterator_get(iter, PANGO_ATTR_WEIGHT),
						*underline = pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);
					// TODO process the styles
				} while (pango_attr_iterator_next(iter));
				pango_attr_iterator_destroy(iter);
			}
			_form_onevent<ui::composition_info>(
				*wnd, &window::_on_composition, str
			);
			g_free(str);
			pango_attr_list_unref(attrs);
		}
		inline static void _on_im_preedit_end(GtkIMContext*, window *wnd) {
			wnd->_on_composition_finished();
		}

		template<typename Obj, typename Func> void _connect_signal(Obj obj, const gchar *name, Func callback) {
			g_signal_connect(obj, name, reinterpret_cast<GCallback>(callback), this);
		}

		void _on_update() override {
			while (gtk_events_pending()) {
				gtk_main_iteration();
			}
			ui::manager::get().schedule_update(*this);
		}

		void _on_got_window_focus() override {
			gtk_im_context_focus_in(_imctx);
			window_base::_on_got_window_focus();
		}
		void _on_lost_window_focus() override {
			gtk_im_context_focus_out(_imctx);
			window_base::_on_lost_window_focus();
		}

		void _initialize(const str_t &cls, const ui::element_metrics &metrics) override {
			window_base::_initialize(cls, metrics);
			ui::manager::get().schedule_update(*this);
			gtk_window_present(GTK_WINDOW(_wnd));
			// setup IM context
			_imctx = gtk_im_multicontext_new();
			_connect_signal(_imctx, "commit", _on_im_commit);
			_connect_signal(_imctx, "preedit-changed", _on_im_preedit_changed);
			_connect_signal(_imctx, "preedit-end", _on_im_preedit_end);
			gtk_im_context_set_client_window(_imctx, gtk_widget_get_window(_wnd));
		}
		void _dispose() override {
			window_base::_dispose();
			g_object_unref(_imctx);
			// calling g_object_unref(_wnd) will cause gtk to emit warnings for no apparent reason
			gtk_widget_destroy(_wnd);
		}

		GtkWidget *_wnd = nullptr;
		GtkIMContext *_imctx = nullptr;
	};
}
