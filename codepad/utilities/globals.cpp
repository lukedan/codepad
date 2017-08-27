#include "globals.h"
#include "../ui/font_family.h"
#include "../ui/commonelements.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"

namespace codepad { // TODO move singletons into functions
	globals *globals::_cur = nullptr;

	namespace ui {
		unsigned char content_host::_def_fnt_ts = 0;

#ifdef CP_DETECT_LOGICAL_ERRORS
		control_dispose_rec _dispose_rec;
#endif
	}

	namespace editor {
		constexpr ui::thickness tab_button::content_padding;

		namespace code {
			const ui::basic_pen *editor::_caretpen = nullptr;
			const ui::basic_brush *editor::_selbrush = nullptr;
			double editor::_lines_per_scroll = 3.0;

			const ui::basic_brush *minimap::_viewport_brush = nullptr;
		}
	}
}
