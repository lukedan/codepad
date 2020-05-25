// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "../dynamic_library.h"

/// \file
/// Implementation of dynamic libraries on Linux.

#include <dlfcn.h>

namespace codepad::os {
	const dynamic_library::native_handle_t dynamic_library::empty_handle = nullptr;

	dynamic_library::symbol_t dynamic_library::find_symbol_raw(const std::u8string &symbol) const {
		symbol_t result = dlsym(_handle, reinterpret_cast<const char*>(symbol.c_str()));
		if (result == nullptr) {
			logger::get().log_warning(CP_HERE) << "find symbol failed: " << dlerror();
		}
		return result;
	}

	dynamic_library::native_handle_t dynamic_library::_load_impl(const std::filesystem::path &path) {
		native_handle_t handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
		if (handle == nullptr) {
			logger::get().log_warning(CP_HERE) << "failed to load dynamic library: " << dlerror();
		}
		return handle;
	}

	void dynamic_library::_unload_impl(native_handle_t handle) {
		if (dlclose(handle) != 0) {
			logger::get().log_warning(CP_HERE) << "failed to close dynamic library: " << dlerror();
		}
	}
}
