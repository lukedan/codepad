// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "font.h"

/// \file
/// Implementation of certain methods related to text rendering.

#include "manager.h"

namespace codepad::ui {
	font_manager::font_manager(manager &man) : _atlas(man.get_renderer()), _manager(man) {
	}
}
