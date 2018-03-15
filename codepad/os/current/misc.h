#pragma once

/// \file
/// Includes the header containing miscellaneous items for the current platform.

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/misc.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/misc.h"
#endif
