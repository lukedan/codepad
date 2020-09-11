// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous shared private functions.

#include <codepad/editors/manager.h>

namespace codepad::editors::_details {
	/// Returns the manager.
	manager &get_manager();
}
