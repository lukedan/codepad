// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/misc.h"

/// \file
/// Implementation of miscellaneous functions.

#ifdef __GNUC__
#	include <cxxabi.h>
#endif

namespace codepad {
	std::u8string demangle(const char *s) {
#ifdef _MSC_VER
		return std::u8string(reinterpret_cast<const char8_t*>(s));
#elif defined(__GNUC__)
		int st;
		char *result = abi::__cxa_demangle(s, nullptr, nullptr, &st);
		assert_true_sys(st == 0, "demangling failed");
		std::u8string res(reinterpret_cast<const char8_t*>(result));
		std::free(result);
		return res;
#endif
	}
}
