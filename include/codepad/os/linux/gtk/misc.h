// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Common internal classes.

#include <gdk/gdk.h>

namespace codepad::os {
	namespace _details {
		struct cursor_set {
			constexpr static std::size_t cursor_count = 13;
			const static GdkCursorType cursor_ids[cursor_count];

			/// Initializes \ref cursors.
			cursor_set() {
				GdkDisplay *disp = gdk_display_get_default();
				for (std::size_t i = 0; i < cursor_count; ++i) {
					cursors[i] = gdk_cursor_new_for_display(disp, cursor_ids[i]);
				}
			}
			/// No move construction.
			cursor_set(cursor_set&&) = delete;
			/// No copy construction.
			cursor_set(const cursor_set&) = delete;
			/// No move assignment.
			cursor_set &operator=(cursor_set&&) = delete;
			/// No copy assignment.
			cursor_set &operator=(const cursor_set&) = delete;
			/// Frees \ref cursors.
			~cursor_set() {
				for (GdkCursor *cursor : cursors) {
					g_object_unref(cursor);
				}
			}

			GdkCursor *cursors[cursor_count]{};

			static cursor_set &get();
		};
	}
}
