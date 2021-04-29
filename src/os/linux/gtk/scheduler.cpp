// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/linux/gtk/scheduler.h"

/// \file
/// Scheduler implementation.

namespace codepad::os {
	std::unique_ptr<ui::_details::scheduler_impl> create_scheduler_impl(ui::scheduler &sched) {
		return std::make_unique<scheduler_impl>(sched);
	}
}
