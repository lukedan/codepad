// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous utilities for Linux.

#include <system_error>

#include <cerrno>

namespace codepad::os {
	namespace _details {
		/// Returns a \p std::error_code from a custom \p errno value.
		[[nodiscard]] inline std::error_code get_error_code_errno_custom(int v) {
			return std::error_code(v, std::system_category());
		}
		/// Returns a \p std::error_code constructed from \p errno.
		[[nodiscard]] inline std::error_code get_error_code_errno() {
			return get_error_code_errno_custom(errno);
		}
	}
}
