#include "globals.h"
#include "../os/renderer.h"
#include "../ui/font_family.h"
#include "../ui/commonelements.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"

namespace codepad {
	globals *globals::_cur = nullptr;

	unsigned char ui::content_host::_def_fnt_ts = 0;

	constexpr ui::thickness editor::tab_button::content_padding;

	double editor::code::editor::_lines_per_scroll = 3.0;

	double editor::code::minimap::_target_height = 2.0;

	GLenum os::opengl_renderer_base::_blend_func_mapping[10] = {
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
}
