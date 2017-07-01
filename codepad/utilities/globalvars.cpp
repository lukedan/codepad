#include "../ui/font.h"
#include "../ui/manager.h"
#include "../ui/commonelements.h"
#include "../editor/docking.h"
#include "../editor/editor_code.h"

namespace codepad {
	ui::font::_library ui::font::_lib;

	namespace os {
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

		const ui::basic_pen *codebox_editor_code::_caretpen = nullptr;
		const ui::basic_brush *codebox_editor_code::_selbrush = nullptr;
		ui::font_family codebox_editor_code::_font;
		double codebox_editor_code::_lines_per_scroll = 3.0;

#ifdef __GNUC__
		constexpr ui::thickness tab_button::content_padding; // wtf?
#endif
	}
}
