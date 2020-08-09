// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/linux/gtk/window.h"

/// \file
/// Implementation of GTK windows.

namespace codepad::os {
	gboolean window::_on_delete_event(GtkWidget*, GdkEvent*, window *wnd) {
		wnd->_on_close_request();
		return true;
	}

	gboolean window::_on_motion_notify_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		if (!wnd->is_mouse_over()) {
			wnd->_on_mouse_enter();
		}
		_form_onevent<ui::mouse_move_info>(
			*wnd, &window::_on_mouse_move, wnd->_update_mouse_position(vec2d(ev->motion.x, ev->motion.y))
		);
		// update cursor
		ui::cursor c = wnd->get_current_display_cursor();
		if (c == ui::cursor::not_specified) {
			c = ui::cursor::normal;
		}
		gdk_window_set_cursor(
			ev->any.window, _details::cursor_set::get().cursors[static_cast<int>(c)]
		);
		return true;
	}

	gboolean window::_on_button_press_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		if (ev->button.type == GDK_BUTTON_PRESS) {
			GdkWindow *gdk_wnd = gtk_widget_get_window(wnd->_wnd);
			_form_onevent<ui::mouse_button_info>(
				*wnd, &window::_on_mouse_down,
				_details::get_button_from_code(ev->button.button), _details::get_modifiers(ev->button),
				wnd->_update_mouse_position(vec2d(ev->button.x, ev->button.y))
			);
		}
		// returning false here will cause this handler to be executed twice
		return true;
	}

	gboolean window::_on_button_release_event(GtkWidget*, GdkEvent *ev, window *wnd) {
		_form_onevent<ui::mouse_button_info>(
			*wnd, &window::_on_mouse_up,
			_details::get_button_from_code(ev->button.button), _details::get_modifiers(ev->button),
			wnd->_update_mouse_position(vec2d(ev->button.x, ev->button.y))
		);
		return true;
	}

	gboolean window::_on_key_press_event(GtkWidget*, GdkEvent *event, window *wnd) {
		ui::key k = _details::get_key_of_event(event);
		if (!wnd->get_manager().get_scheduler().get_hotkey_listener().on_key_down(
			ui::key_gesture(k, _details::get_modifiers(event->key))
		)) {
			if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
				_form_onevent<ui::key_info>(*wnd, &window::_on_key_down, k);
			}
		}
		return true;
	}

	gboolean window::_on_key_release_event(GtkWidget*, GdkEvent *event, window *wnd) {
		ui::key k = _details::get_key_of_event(event);
		if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
			_form_onevent<ui::key_info>(*wnd, &window::_on_key_up, k);
		}
		return true;
	}

	void window::_initialize(std::u8string_view cls) {
		window_base::_initialize(cls);

		// set gravity to static so that coordinates are relative to the client region
		gtk_window_set_gravity(GTK_WINDOW(_wnd), GDK_GRAVITY_STATIC);
		gtk_widget_set_app_paintable(_wnd, true);
		gtk_widget_add_events(
			_wnd,
			GDK_POINTER_MOTION_MASK |
			GDK_LEAVE_NOTIFY_MASK |
			GDK_BUTTON_PRESS_MASK |
			GDK_BUTTON_RELEASE_MASK |
			GDK_SCROLL_MASK |
			GDK_FOCUS_CHANGE_MASK
		);

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
		_connect_signal(_wnd, "grab-broken-event", _on_grab_broken_event);

		// setup IM context
		_imctx = gtk_im_multicontext_new();
		_connect_signal(_imctx, "commit", _on_im_commit);
		_connect_signal(_imctx, "preedit-changed", _on_im_preedit_changed);
		_connect_signal(_imctx, "preedit-end", _on_im_preedit_end);
		gtk_im_context_set_client_window(_imctx, gtk_widget_get_window(_wnd));
	}
}
