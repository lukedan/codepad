// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous platform-seecific types and function definitions.
/// These functions must be implemented on a per-platform basis.

#include "../core/misc.h"
#include "../ui/misc.h"

namespace codepad::ui {
	class window_base;
}

namespace codepad::os {
	/// Returns if a mouse button is currently pressed.
	bool is_mouse_button_down(ui::mouse_button);

	/// Returns the position of the mouse in screen coordinates.
	vec2i get_mouse_position();
	/// Sets the mouse position in screen coordinates.
	void set_mouse_position(vec2i);

	/// Specifies the type of a file dialog.
	enum class file_dialog_type {
		single_selection, ///< Only one file can be selected.
		multiple_selection ///< Multiple files can be selected.
	};
	/// Shows an open file dialog that asks the user to select one or multiple files.
	///
	/// \return The list of files that the user selects, or empty if the user cancels the operation.
	std::vector<std::filesystem::path> open_file_dialog(const ui::window_base*, file_dialog_type);

	/// Initializes platform-specific aspects of the program with the given command line arguments.
	void initialize(int, char**);
}
