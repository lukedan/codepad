// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous shared private functions.

#include <codepad/core/plugins.h>

#include "codepad/editors/manager.h"

namespace codepad::editors::_details {
	/// Returns the manager.
	[[nodiscard]] manager &get_manager();
	/// Returns the context of this plugin.
	[[nodiscard]] const plugin_context &get_plugin_context();

	/// Returns the list of built-in commands.
	[[nodiscard]] std::list<ui::command_registry::stub> get_builtin_commands(const plugin_context&);
}
