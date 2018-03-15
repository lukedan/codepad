#pragma once

/// \file
/// Includes the font header for the current platform.

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/font.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/font.h"
#endif
