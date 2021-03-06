// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous private shared functions.

#include "codepad/tree_sitter/language_configuration.h"
#include "codepad/tree_sitter/manager.h"

namespace codepad::tree_sitter::_details {
	manager &get_manager();
}
