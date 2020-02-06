// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of dynamic library loader on Windows.

#include "../dynamic_library.h"

#include <Windows.h>

#include "misc.h"
#include "../../core/logging.h"

namespace codepad::os {
	const dynamic_library::native_handle_t dynamic_library::empty_handle = nullptr;

	dynamic_library::native_handle_t dynamic_library::_load_impl(const std::filesystem::path &path) {
		native_handle_t res = LoadLibrary(path.c_str());
		if (res == nullptr) {
			logger::get().log_warning(CP_HERE) <<
				"failed to load dll: " << GetLastError();
		}
		return res;
	}

	void dynamic_library::_unload_impl(native_handle_t handle) {
		_details::winapi_check(FreeLibrary(handle));
	}

	dynamic_library::symbol_t dynamic_library::find_symbol_raw(const str_t &name) const {
		return GetProcAddress(_handle, name.c_str());
	}
}
