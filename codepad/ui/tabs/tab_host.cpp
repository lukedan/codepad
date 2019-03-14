// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "tab_host.h"

/// \file
/// Implementation of certain functions related to tab hosts.

#include "manager.h"

namespace codepad::ui {
	void tab_host::_on_tab_removed(tab &t) {
		_tab_buttons_region->children().remove(*t._btn);
		get_tab_manager()._on_tab_detached(*this, t);
	}
}
