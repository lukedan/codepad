// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/renderer.h"

/// \file
/// Implementation of certain renderer-specific functions.

#include "codepad/ui/window.h"

namespace codepad::ui {
	std::any &renderer_base::_get_window_data(window_base &wnd) {
		return wnd._renderer_data;
	}
}
