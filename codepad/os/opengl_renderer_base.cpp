// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "opengl_renderer_base.h"

/// \file
/// Implementation of certain functions of codepad::os::opengl_renderer_base.

#include "../ui/manager.h"
#include "../ui/window.h"

using namespace codepad::ui;

namespace codepad::os {
	GLenum opengl_renderer_base::_blend_func_mapping[10] = {
		GL_ZERO,
		GL_ONE,
		GL_SRC_ALPHA,
		GL_ONE_MINUS_SRC_ALPHA,
		GL_DST_ALPHA,
		GL_ONE_MINUS_DST_ALPHA,
		GL_SRC_COLOR,
		GL_ONE_MINUS_SRC_COLOR,
		GL_DST_COLOR,
		GL_ONE_MINUS_DST_COLOR
	};

	void opengl_renderer_base::begin(const window_base &wnd) {
		vec2i sz = wnd.get_layout().size().convert<int>();
		_begin_render_target(_render_target_stackframe(
			true, static_cast<size_t>(sz.x), static_cast<size_t>(sz.y),
			_get_begin_window_func(wnd), _get_end_window_func(wnd)
		));
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		_gl_verify();
	}
}
