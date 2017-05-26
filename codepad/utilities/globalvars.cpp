#include "font.h"
#include "../ui/manager.h"
#include "../ui/commonelements.h"
#include "../editor/docking.h"
#include "../editor/codebox.h"

namespace codepad {
	font::_library font::_lib;

	namespace platform {
		renderer_base::_default_renderer renderer_base::_rend;
	}

	namespace ui {
		manager manager::_sman;

		unsigned char content_host::_def_fnt_ts = 0;
		const font *content_host::_def_fnt = nullptr;

#ifndef NDEBUG
		control_dispose_rec _dispose_rec;
#endif
	}

	namespace editor {
		dock_manager dock_manager::_dman;

		const ui::basic_pen *codebox::_caretpen = nullptr;
		const ui::basic_brush *codebox::_selbrush = nullptr;
		font_family codebox::_font;
	}
}
