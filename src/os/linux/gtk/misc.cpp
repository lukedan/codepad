// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/linux/gtk/misc.h"

namespace codepad::os::_details {
	const GdkCursorType cursor_set::cursor_ids[cursor_count] = {
		GDK_LEFT_PTR,
		GDK_WATCH,
		GDK_CROSSHAIR,
		GDK_HAND1,
		GDK_QUESTION_ARROW,
		GDK_XTERM,
		GDK_X_CURSOR,
		GDK_CROSS,
		GDK_TOP_LEFT_CORNER,
		GDK_SB_V_DOUBLE_ARROW,
		GDK_TOP_RIGHT_CORNER,
		GDK_SB_H_DOUBLE_ARROW,
		GDK_BLANK_CURSOR
	};

	cursor_set &cursor_set::get() {
		static cursor_set obj;
		return obj;
	}
}
