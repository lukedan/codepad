// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// URI utilities.

#include <string>
#include <filesystem>

namespace codepad::lsp::uri {
	/// Converts a windows path to a URI.
	[[nodiscard]] std::u8string from_windows_path(const std::u8string&);
	/// Converts a unix path to a URI.
	[[nodiscard]] std::u8string from_unix_path(const std::u8string&);

	/// Converts a path of the current OS to a URI.
	[[nodiscard]] std::u8string from_current_os_path(const std::u8string&);
	/// \overload
	[[nodiscard]] inline std::u8string from_current_os_path(const std::filesystem::path &p) {
		return from_current_os_path(p.u8string());
	}


	/// Converts a URI to a windows path.
	[[nodiscard]] std::u8string to_windows_path(const std::u8string&);
	/// Converts a URI to a unix path.
	[[nodiscard]] std::u8string to_unix_path(const std::u8string&);

	/// Converts a URI to a path of the current OS.
	[[nodiscard]] std::u8string to_current_os_path(const std::u8string&);
}
