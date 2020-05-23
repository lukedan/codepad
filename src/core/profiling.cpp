// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "profiling.h"

/// \file
/// Implementation of certain profiling-related routines.

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


	performance_monitor::~performance_monitor() {
		auto dur = clock_t::now() - _beg_time;
		// TODO print duration directly after C++20
		auto sdur = std::chrono::duration_cast<std::chrono::duration<double>>(dur);
		if (dur > _expected && _cond != log_condition::never) {
			logger::get().log_info(CP_HERE) <<
				"operation took longer(" << sdur.count() << "s) than expected(" <<
				_expected.count() << "): " << _label;
		} else if (_cond == log_condition::always) {
			logger::get().log_debug(CP_HERE) << "operation took " << sdur.count() << "s: " << _label;
		}
	}

	void performance_monitor::log_time() {
		auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(clock_t::now() - _beg_time);
		logger::get().log_debug(CP_HERE) << _label << ": operation has been running for " << dur.count() << "s";
	}
}
