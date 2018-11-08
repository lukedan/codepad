// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#include <gdk/gdk.h>

namespace codepad::os {
	namespace _details {
		void check_init_gtk();
		struct cursor_set {
			constexpr static size_t cursor_count = 13;
			const static GdkCursorType cursor_ids[cursor_count];
			cursor_set() {
				GdkDisplay *disp = gdk_display_get_default();
				for (size_t i = 0; i < cursor_count; ++i) {
					cursors[i] = gdk_cursor_new_for_display(disp, cursor_ids[i]);
				}
			}
			~cursor_set() {
				for (GdkCursor *cursor : cursors) {
					g_object_unref(cursor);
				}
			}
			GdkCursor *cursors[cursor_count];

			static cursor_set &get();
		};
	}
}
