// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

#ifdef CP_USE_GTK
#	include "gtk/renderer.h"
#else
#	include "x11/renderer.h"
#endif
