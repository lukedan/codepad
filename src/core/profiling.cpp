// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/profiling.h"

/// \file
/// Implementation of certain profiling-related routines.

namespace codepad {
	performance_monitor::~performance_monitor() {
		auto dur = clock_t::now() - _beg_time;
		if (dur > _expected && _cond != log_condition::never) {
			logger::get().log_info() <<
				"operation took longer(" << dur << ") than expected(" << _expected << "): " << _label;
		} else if (_cond == log_condition::always) {
			logger::get().log_debug() << "operation took " << dur << ": " << _label;
		}
	}

	void performance_monitor::log_time() {
		auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(clock_t::now() - _beg_time);
		logger::get().log_debug() << _label << ": operation has been running for " << dur.count() << "s";
	}
}
