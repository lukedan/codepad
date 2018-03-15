#pragma once

/// \file
/// Includes the renderer header for the current platform.

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/renderer.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/renderer.h"
#endif
