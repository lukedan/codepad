// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Includes the renderer header for the current platform.

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/renderer.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/renderer.h"
#endif
