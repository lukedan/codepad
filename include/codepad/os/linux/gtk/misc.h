// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Common internal classes.

#include <gdk/gdk.h>

#include "codepad/ui/misc.h"
#include "codepad/ui/hotkey_registry.h"

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

			GdkCursor *cursors[cursor_count]{}; ///< The list of cursors.

			/// Returns the static \ref cursor_set instance.
			static cursor_set &get();
		};


		/// Maximum number of key codes mapped to a single \ref ui::key value.
		constexpr static std::size_t max_keysym_mapping = 4;
		/// Mapping from \ref ui::key to GDK key codes.
		///
		/// \todo Complete the list of KeySyms.
		extern const guint keysym_mapping[ui::total_num_keys][max_keysym_mapping];
		/// Maps from GDK key codes to \ref ui::key enums.
		[[nodiscard]] ui::key get_mapped_key(guint ks);

		[[nodiscard]] ui::mouse_button get_button_from_code(guint code);
		[[nodiscard]] inline ui::key get_key_of_event(GdkEvent *event) {
			return get_mapped_key(event->key.keyval);
		}
		template <typename Ev> [[nodiscard]] inline ui::modifier_keys get_modifiers(const Ev &event) {
			ui::modifier_keys result = ui::modifier_keys::none;
			if (event.state & GDK_CONTROL_MASK) {
				result |= ui::modifier_keys::control;
			}
			if (event.state & GDK_SHIFT_MASK) {
				result |= ui::modifier_keys::shift;
			}
			if (event.state & GDK_MOD1_MASK) { // normally alt
				result |= ui::modifier_keys::alt;
			}
			if (event.state & GDK_HYPER_MASK) { // whatever
				result |= ui::modifier_keys::super;
			}
			return result;
		}
	}
}
