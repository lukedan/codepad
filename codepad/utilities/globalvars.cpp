#include "../utilities/tasks.h"
#include "../ui/font.h"
#include "../ui/manager.h"
#include "../ui/commonelements.h"
#include "../editors/docking.h"
#include "../editors/code/editor.h"
#include "../editors/code/components.h"

namespace codepad { // TODO move singletons into functions
	logger &logger::get() {
		static logger _default;
		return _default;
	}

	async_task_pool &async_task_pool::get() {
		static async_task_pool _default;
		return _default;
	}

	callback_buffer &callback_buffer::get() {
		static callback_buffer _default;
		return _default;
	}

	namespace os {
		renderer_base::_default_renderer renderer_base::_rend;
	}

	namespace ui {
		font::_library font::_lib;

		manager manager::_sman;

		unsigned char content_host::_def_fnt_ts = 0;
		const font *content_host::_def_fnt = nullptr;

#ifndef NDEBUG
		control_dispose_rec _dispose_rec;
#endif
	}

	namespace editor {
		dock_manager dock_manager::_dman;

		constexpr ui::thickness tab_button::content_padding;

		namespace code {
			const ui::basic_pen *editor::_caretpen = nullptr;
			const ui::basic_brush *editor::_selbrush = nullptr;
			ui::font_family editor::_font;
			double editor::_lines_per_scroll = 3.0;

			const ui::basic_brush *minimap::_viewport_brush = nullptr;
		}
	}
}
