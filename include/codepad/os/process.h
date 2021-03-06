// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Handling of processes.

#include "filesystem.h"

namespace codepad::os {
	/// Contains functions for starting processes.
	struct process {
		/// Starts a process, optionally with redirected stdin/stdout/stderr.
		[[nodiscard]] static std::error_code start_process(
			const std::filesystem::path &exec, const std::vector<std::u8string_view> &args,
			const file &stdin_redirect = file(),
			const file &stdout_redirect = file(),
			const file &stderr_redirect = file()
		);
	};
}
