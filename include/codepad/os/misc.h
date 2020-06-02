// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous platform-seecific types and function definitions.
/// These functions must be implemented on a per-platform basis.

#include <optional>

#include "../core/misc.h"
#include "../ui/misc.h"

namespace codepad::ui {
	class window_base;
}

namespace codepad::os {
	/// Initializes platform-specific aspects of the program with the given command line arguments.
	void initialize(int, char**);

	/// Contains functions related to file dialogs.
	class APIGEN_EXPORT_RECURSIVE file_dialog {
	public:
		/// Specifies the type of a file dialog.
		enum class type {
			single_selection, ///< Only one file can be selected.
			multiple_selection ///< Multiple files can be selected.
		};

		/// Shows an open file dialog that asks the user to select one or multiple files.
		///
		/// \return The list of files that the user selects, or empty if the user cancels the operation.
		static std::vector<std::filesystem::path> show_open_dialog(const ui::window_base*, type);
	};

	/// Contains functions associated with clipboards.
	class APIGEN_EXPORT_RECURSIVE clipboard {
	public:
		/// Copies the given text to the clipboard.
		static void set_text(std::u8string_view);
		/// Retrieves text from the clipboard.
		static std::optional<std::u8string> get_text();
		/// Returns if there's text in the clipboard.
		static bool is_text_available();
	};
}
