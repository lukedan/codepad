#pragma once

#ifdef CP_PLATFORM_WINDOWS
#	include "windows.h"
#elif defined CP_PLATFORM_UNIX
#	include "linux.h"
#endif
