// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include <vector>

#include <gtk/gtk.h>

#include "codepad/core/misc.h"
#include "codepad/ui/misc.h"
#include "codepad/os/linux/gtk/misc.h"
#include "codepad/os/linux/gtk/window.h"

namespace codepad::os {
	void initialize(int argc, char **argv) {
		gtk_init(&argc, &argv);
	}


	std::vector<std::filesystem::path> file_dialog::show_open_dialog(
		const ui::window *parent, file_dialog::type type
	) {
		auto &parent_impl = _details::cast_window_impl(parent->get_impl());
		GtkWidget *dialog = gtk_file_chooser_dialog_new(
			nullptr, parent ? GTK_WINDOW(parent_impl.get_native_handle()) : nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
			"_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL
		);
		gtk_file_chooser_set_select_multiple(
			GTK_FILE_CHOOSER(dialog), type == file_dialog::type::multiple_selection
		);
		gint res = gtk_dialog_run(GTK_DIALOG(dialog));
		std::vector<std::filesystem::path> paths;
		if (res == GTK_RESPONSE_ACCEPT) {
			GSList *list = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
			for (GSList *iter = list; iter; iter = iter->next) {
				paths.emplace_back(reinterpret_cast<gchar*>(iter->data));
				g_free(iter->data);
			}
			g_slist_free(list);
		}
		gtk_widget_destroy(dialog);
		return paths;
	}

	/*texture load_image(renderer_base &renderer, const filesystem::path &path) {
		GError *err = nullptr;
		GdkPixbuf *buf = gdk_pixbuf_new_from_file(path.c_str(), &err);
		if (err) {
			logger::get().log_error(CP_HERE, "gdk error ", err->message);
			assert_true_sys(false, "cannot load image");
		}
		assert_true_usage(gdk_pixbuf_get_bits_per_sample(buf) == 8, "invalid bits per sample");
		auto
			width = static_cast<std::size_t>(gdk_pixbuf_get_width(buf)),
			height = static_cast<std::size_t>(gdk_pixbuf_get_height(buf));
		void *buffer = std::malloc(4 * width * height);
		auto *target = static_cast<unsigned char*>(buffer);
		auto has_alpha = static_cast<bool>(gdk_pixbuf_get_has_alpha(buf));
		const guchar *src = gdk_pixbuf_get_pixels(buf);
		int stride = gdk_pixbuf_get_rowstride(buf);
		for (std::size_t y = 0; y < height; ++y, src += stride) {
			const guchar *source = src;
			for (std::size_t x = 0; x < width; ++x, target += 4) {
				target[0] = source[0];
				target[1] = source[1];
				target[2] = source[2];
				if (has_alpha) {
					target[3] = source[3];
					source += 4;
				} else {
					target[3] = 255;
					source += 3;
				}
			}
		}
		texture tex = renderer.new_texture(width, height, buffer);
		std::free(buffer);
		g_object_unref(buf);
		return tex;
	}*/


	double system_parameters::get_drag_deadzone_radius() {
		return 5.0; // TODO
	}
}


namespace codepad::ui {
	bool scheduler::_main_iteration_system_impl(wait_type type) {
		if (type == wait_type::non_blocking) {
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

	/// A function that returns false. This is used as GLib sources, to create events that only happens once.
	gboolean _glib_call_once(gpointer) {
		return false;
	}
	void scheduler::_set_timer(clock_t::duration duration) {
		guint ms = std::chrono::duration_cast<std::chrono::duration<guint, std::milli>>(duration).count();
		g_timeout_add(ms, _glib_call_once, nullptr);
	}

	void scheduler::_wake_up() {
		g_idle_add(_glib_call_once, nullptr);
	}
}
