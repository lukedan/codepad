// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Scheduler implementation on Linux using GTK.

#include <gtk/gtk.h>

#include "codepad/ui/scheduler.h"

namespace codepad::os {
	/// Scheduler implementation for Linux using GTK.
	class scheduler_impl : public ui::_details::scheduler_impl {
	public:
		/// Initializes the base scheduler implementation.
		explicit scheduler_impl(ui::scheduler &s) : ui::_details::scheduler_impl(s) {
		}

		/// Handles an iteration using \p gtk_main_iteration_do().
		bool handle_event(ui::scheduler::wait_type type) override {
			if (type == ui::scheduler::wait_type::non_blocking) {
				if (gtk_events_pending()) {
					gtk_main_iteration_do(false);
					return true;
				}
				return false;
			}
			gtk_main_iteration_do(true);
			// FIXME an event may or may not have been processed
			//       however the scheduler doesn't really care in this case
			return true;
		}

		/// Creates a new timer event using \p g_timeout_add().
		void set_timer(ui::scheduler::clock_t::duration duration) override {
			guint ms = std::chrono::duration_cast<std::chrono::duration<guint, std::milli>>(duration).count();
			g_timeout_add(ms, _glib_call_once, nullptr);
		}

		/// Wakes the main thread up by adding a new idle event using \p g_idle_add().
		void wake_up() override {
			g_idle_add(_glib_call_once, nullptr);
		}
	protected:
		/// A function that returns false. This is used as GLib sources, to create events that only happens once.
		inline static gboolean _glib_call_once(gpointer) {
			return false;
		}
	};
}
