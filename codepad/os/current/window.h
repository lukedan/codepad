#pragma once

/// \file
/// Includes the window header for the current platform.

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/window.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/window.h"
#endif
