#pragma once

#ifdef CP_PLATFORM_WINDOWS
#	include "../windows/renderer.h"
#elif defined CP_PLATFORM_UNIX
#	include "../linux/renderer.h"
#endif
