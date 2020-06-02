// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/logging.h"

/// \file
/// Implementation of the logger.

namespace codepad {
	std::unique_ptr<logger> &logger::_get_ptr() {
		static std::unique_ptr<logger> ptr;
		return ptr;
	}
}
