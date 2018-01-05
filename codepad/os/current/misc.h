#pragma once

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/misc.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/misc.h"
#endif
