// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous platform-seecific types and function definitions.
/// These functions must be implemented on a per-platform basis.

#include "../core/misc.h"

namespace codepad::os {
	class window_base;

	/// The style of the cursor.
	enum class cursor {
		normal, ///< The normal cursor.
		busy, ///< The `busy' cursor.
		crosshair, ///< The `crosshair' cursor.
		hand, ///< The `hand' cursor, usually used to indicate a link.
		help, ///< The `help' cursor.
		text_beam, ///< The `I-beam' cursor, usually used to indicate an input field.
		denied, ///< The `denied' cursor.
		arrow_all, ///< A cursor with arrows to all four directions.
		arrow_northeast_southwest, ///< A cursor with arrows to the top-right and bottom-left directions.
		arrow_north_south, ///< A cursor with arrows to the top and bottom directions.
		arrow_northwest_southeast, ///< A cursor with arrows to the top-left and bottom-right directions.
		arrow_east_west, ///< A cursor with arrows to the left and right directions.
		invisible, ///< An invisible cursor.

		not_specified ///< An unspecified cursor.
	};

	namespace input {
		/// Represents a button of the mouse.
		enum class mouse_button {
			primary, ///< The primary button. For the right-handed layout, this is the left button.
			tertiary, ///< The middle button.
			secondary ///< The secondary button. For the right-handed layout, this is the right button.
		};
		/// Represents a key on the keyboard.
		///
		/// \todo Document all keys, add support for more keys (generic super key, symbols, etc.).
		enum class key {
			cancel,
			xbutton_1, xbutton_2,
			backspace, ///< The `backspace' key.
			tab, ///< The `tab' key.
			clear,
			enter, ///< The `enter' key.
			shift, ///< The `shift' key.
			control, ///< The `control' key.
			alt, ///< The `alt' key.
			pause,
			capital_lock, ///< The `caps lock' key.
			escape, ///< The `escape' key.
			convert,
			nonconvert,
			space, ///< The `space' key.
			page_up, ///< The `page up' key.
			page_down, ///< The `page down' key.
			end, ///< The `end' key.
			home, ///< The `home' key.
			left, ///< The left arrow key.
			up, ///< The up arrow key.
			right, ///< The right arrow key.
			down, ///< The down arrow key.
			select,
			print,
			execute,
			snapshot,
			insert, ///< The `insert' key.
			del, ///< The `delete' key.
			help,
			a, b, c, d, e, f, g, h, i, j, k, l, m,
			n, o, p, q, r, s, t, u, v, w, x, y, z,
			left_super, ///< The left `super' (win) key.
			right_super, ///< The right `super' (win) key.
			apps,
			sleep,
			multiply, ///< The `multiply' key.
			add, ///< The `add' key.
			separator, ///< The `separator' key.
			subtract, ///< The `subtract' key.
			decimal, ///< The `decimal' key.
			divide, ///< The `divide' key.
			f1, f2, f3, f4,
			f5, f6, f7, f8,
			f9, f10, f11, f12,
			num_lock, ///< The `num lock' key.
			scroll_lock, ///< The `scroll lock' key.
			left_shift, ///< The left `shift' key.
			right_shift, ///< The right `shift' key.
			left_control, ///< The left `control' key.
			right_control, ///< The right `control' key.
			left_alt, ///< The left `alt' key.
			right_alt, ///< The right `alt' key.

			max_value ///< The total number of supported keys.
		};
		/// The total number of supported keys.
		constexpr size_t total_num_keys = static_cast<size_t>(key::max_value);
		/// Returns if a mouse button is currently pressed.
		bool is_mouse_button_down(mouse_button);

		/// Returns the position of the mouse in screen coordinates.
		vec2i get_mouse_position();
		/// Sets the mouse position in screen coordinates.
		void set_mouse_position(vec2i);
	}

	/// Specifies the type of a file dialog.
	enum class file_dialog_type {
		single_selection, ///< Only one file can be selected.
		multiple_selection ///< Multiple files can be selected.
	};
	/// Shows an open file dialog that asks the user to select one or multiple files.
	///
	/// \return The list of files that the user selects, or empty if the user cancels the operation.
	std::vector<std::filesystem::path> open_file_dialog(const window_base*, file_dialog_type);

	/// Initializes platform-specific aspects of the program with the given command line arguments.
	void initialize(int, char**);
}
