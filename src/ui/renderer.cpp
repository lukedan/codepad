// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "renderer.h"

/// \file
/// Implementation of certain renderer-specific functions.

#include "window.h"

using namespace std;

namespace codepad::ui {
	any &renderer_base::_get_window_data(window_base &wnd) {
		return wnd._renderer_data;
	}
}
