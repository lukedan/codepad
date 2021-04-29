// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/linux/gtk/window.h"

/// \file
/// Implementation of GTK windows.

namespace codepad::os {
	window_impl::window_impl(ui::window &w) : ui::_details::window_impl(w) {
		_wnd = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		// set gravity to static so that coordinates are relative to the client region
		gtk_window_set_gravity(GTK_WINDOW(_wnd), GDK_GRAVITY_STATIC);
		// this only applies for transient windows; destroy this window when its transient parent is destroyed
		gtk_window_set_destroy_with_parent(GTK_WINDOW(_wnd), true);
		// request tooltips so that we can receive hover events to show our own tooltips
		gtk_widget_set_has_tooltip(_wnd, true);

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
		_connect_signal(_wnd, "query-tooltip", _on_query_tooltip_event);
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

	gboolean window_impl::_on_delete_event(GtkWidget*, GdkEvent*, window_impl *wnd) {
		wnd->_window._on_close_request();
		return true;
	}

	gboolean window_impl::_on_motion_notify_event(GtkWidget*, GdkEvent *ev, window_impl *wnd) {
		if (!wnd->_window.is_mouse_over()) {
			wnd->_window._on_mouse_enter();
		}
		_form_onevent<ui::mouse_move_info>(
			wnd->_window, &ui::window::_on_mouse_move,
			wnd->_window._update_mouse_position(vec2d(ev->motion.x, ev->motion.y))
		);
		// update cursor
		ui::cursor c = wnd->_window.get_current_display_cursor();
		if (c == ui::cursor::not_specified) {
			c = ui::cursor::normal;
		}
		gdk_window_set_cursor(
			ev->any.window, _details::cursor_set::get().cursors[static_cast<int>(c)]
		);
		return true;
	}

	gboolean window_impl::_on_query_tooltip_event(
		GtkWidget*, int x, int y, gboolean keyboard_mode, GtkTooltip*, window_impl *wnd
	) {
		if (!keyboard_mode) {
			// TODO get modifier keys
			_form_onevent<ui::mouse_hover_info>(
				wnd->_window, &ui::window::_on_mouse_hover,
				ui::modifier_keys::none, wnd->_window._update_mouse_position(vec2d(x, y))
			);
		}
		// we always handle tooltips by ourselves
		return false;
	}

	gboolean window_impl::_on_button_press_event(GtkWidget*, GdkEvent *ev, window_impl *wnd) {
		if (ev->button.type == GDK_BUTTON_PRESS) {
			_form_onevent<ui::mouse_button_info>(
				wnd->_window, &ui::window::_on_mouse_down,
				_details::get_button_from_code(ev->button.button), _details::get_modifiers(ev->button),
				wnd->_window._update_mouse_position(vec2d(ev->button.x, ev->button.y))
			);
		}
		// returning false here will cause this handler to be executed twice
		return true;
	}

	gboolean window_impl::_on_button_release_event(GtkWidget*, GdkEvent *ev, window_impl *wnd) {
		_form_onevent<ui::mouse_button_info>(
			wnd->_window, &ui::window::_on_mouse_up,
			_details::get_button_from_code(ev->button.button), _details::get_modifiers(ev->button),
			wnd->_window._update_mouse_position(vec2d(ev->button.x, ev->button.y))
		);
		return true;
	}

	gboolean window_impl::_on_key_press_event(GtkWidget*, GdkEvent *event, window_impl *wnd) {
		ui::key k = _details::get_key_of_event(event);
		if (!wnd->_window.get_manager().get_scheduler().get_hotkey_listener().on_key_down(
			ui::key_gesture(k, _details::get_modifiers(event->key))
		)) {
			if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
				_form_onevent<ui::key_info>(wnd->_window, &ui::window::_on_key_down, k);
			}
		}
		return true;
	}

	gboolean window_impl::_on_key_release_event(GtkWidget*, GdkEvent *event, window_impl *wnd) {
		ui::key k = _details::get_key_of_event(event);
		if (!gtk_im_context_filter_keypress(wnd->_imctx, &event->key)) {
			_form_onevent<ui::key_info>(wnd->_window, &ui::window::_on_key_up, k);
		}
		return true;
	}

	gboolean window_impl::_on_scroll_event(GtkWidget*, GdkEvent *event, window_impl *wnd) {
		logger::get().log_debug(CP_HERE) <<
			"scroll direction: " << event->scroll.direction <<
			"\nx = " << event->scroll.delta_x <<
			"\ny = " << event->scroll.delta_y;
		// ignore duplicate scroll events of regular mouse scroll events
		// gdk_event_get_pointer_emulated() did not work, this is taken from firefox:
		// https://searchfox.org/mozilla-central/source/widget/gtk/nsWindow.cpp, in nsWindow::OnScrollEvent()
		// FIXME not my fault smooth scrolling doesn't work
		if (event->scroll.direction != GDK_SCROLL_SMOOTH && event->scroll.time == wnd->_prev_scroll_timestamp) {
			logger::get().log_debug(CP_HERE) << "duplicate scroll event ignored";
			return true;
		}

		// stop kinetic scrolling
		if (!wnd->_kinetic_token.empty()) {
			wnd->_window.get_manager().get_scheduler().cancel_synchronous_task(wnd->_kinetic_token);
		}

		vec2d delta;
		switch (event->scroll.direction) {
		case GDK_SCROLL_UP:
			delta.y = -1.0;
			break;
		case GDK_SCROLL_DOWN:
			delta.y = 1.0;
			break;
		case GDK_SCROLL_LEFT:
			delta.x = -1.0;
			break;
		case GDK_SCROLL_RIGHT:
			delta.x = 1.0;
			break;
		case GDK_SCROLL_SMOOTH:
			delta = vec2d(event->scroll.delta_x, event->scroll.delta_y);
			// TODO doesn't work
			if (gdk_event_is_scroll_stop_event(event)) {
				if (event->scroll.time != wnd->_prev_scroll_timestamp) {
					double dt = (event->scroll.time - wnd->_prev_scroll_timestamp) / 1000.0;
					vec2d
						mouse(event->scroll.x, event->scroll.y),
						speed = vec2d(event->scroll.delta_x, event->scroll.delta_y) / dt;
					ui::scheduler::clock_t::time_point last_update = ui::scheduler::clock_t::now();
					wnd->_kinetic_token = wnd->_window.get_manager().get_scheduler().register_synchronous_task(
						last_update, &wnd->_window,
						[
							mouse, speed, last_update
						](ui::element *e) mutable -> std::optional<ui::scheduler::clock_t::time_point> {
							auto *wnd = dynamic_cast<ui::window*>(e);
							assert_true_logical(wnd, "invalid window handle");
							auto &impl = _details::cast_window_impl(wnd->get_impl());
							auto now = ui::scheduler::clock_t::now();
							double dt = std::chrono::duration<double>(now - last_update).count();
							// send scroll event
							_form_onevent<ui::mouse_scroll_info>(
								*wnd, &ui::window::_on_mouse_scroll,
								speed * dt, wnd->_update_mouse_position(mouse), true
							);
							// update velocity
							double acceleration_mag = dt;
							vec2d acceleration;
							vec2d absv = vec2d(std::abs(speed.x), std::abs(speed.y));
							acceleration.x =
								acceleration_mag > absv.x ?
								-speed.x :
								(speed.x > 0.0 ? -acceleration_mag : acceleration_mag);
							acceleration.y =
								acceleration_mag > absv.y ?
								-speed.y :
								(speed.y > 0.0 ? -acceleration_mag : acceleration_mag);
							speed += acceleration;
							// stop updating if scrolling has stopped
							last_update = now;
							if (acceleration_mag > absv.x && acceleration_mag > absv.y) { // stopped
								impl._kinetic_token = ui::scheduler::sync_task_token();
								return std::nullopt;
							} else {
								return now;
							}
						}
					);
				}
			}
			break;
		}
		_form_onevent<ui::mouse_scroll_info>(
			wnd->_window, &ui::window::_on_mouse_scroll, delta,
			wnd->_window._update_mouse_position(vec2d(event->scroll.x, event->scroll.y)),
			event->scroll.direction == GDK_SCROLL_SMOOTH
		);
		wnd->_prev_scroll_timestamp = event->scroll.time;
		return true;
	}
}

namespace codepad::ui {
	std::unique_ptr<_details::window_impl> window::_create_impl(window &wnd) {
		return std::make_unique<os::window_impl>(wnd);
	}
}
