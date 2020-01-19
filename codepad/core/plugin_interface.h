// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Functions only exposed to plugins. This function should not be included anywhere besides <cc>main.cpp</cc> and
/// <cc>api_export.h</cc>.

#include "settings.h"
#include "../ui/manager.h"

namespace codepad {
	inline settings *global_settings; ///< The global \ref settings object.
	inline ui::manager *global_manager; ///< The global \ref ui::manager object.

	/// Returns \ref global_settings.
	inline APIGEN_EXPORT settings &get_settings() {
		return *global_settings;
	}
	/// Returns \ref global_manager.
	inline APIGEN_EXPORT ui::manager &get_manager() {
		return *global_manager;
	}

	/// Creates a \ref str_t from a C string.
	inline APIGEN_EXPORT str_t std_string_from_c_string(const char *str) {
		return str_t(str);
	}
	/// Creates a \ref str_view_t from a C string.
	inline APIGEN_EXPORT str_view_t std_string_view_from_c_string(const char *str) {
		return str_view_t(str);
	}
};
