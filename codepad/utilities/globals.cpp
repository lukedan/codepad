#include "globals.h"
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
}
