// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Handling of processes.

#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#elif defined(CP_PLATFORM_UNIX)
#	include <unistd.h>
#endif

#include "filesystem.h"

namespace codepad::os {
	/// Contains functions for starting processes.
	struct process {
		using id_t =
#ifdef CP_PLATFORM_WINDOWS
			DWORD;
#elif defined(CP_PLATFORM_UNIX)
			pid_t;
#endif

		/// Starts a process, optionally with redirected stdin/stdout/stderr.
		[[nodiscard]] static std::error_code start_process(
			const std::filesystem::path &exec, const std::vector<std::u8string_view> &args,
			const file &stdin_redirect = file(),
			const file &stdout_redirect = file(),
			const file &stderr_redirect = file()
		);

		/// Returns the ID of the current process.
		[[nodiscard]] static id_t get_current_process_id();
	};
}
