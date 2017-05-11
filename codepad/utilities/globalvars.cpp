#include "font.h"
#include "../ui/manager.h"
#include "../ui/commonelements.h"
#include "../editor/docking.h"

namespace codepad {
	font::_library font::_lib;

	namespace platform {
		renderer_base::_default_renderer renderer_base::_rend;
	}

	namespace ui {
		manager manager::_sman;

		unsigned char content_host::_def_fnt_ts = 0;
		const font *content_host::_def_fnt = nullptr;

		control_dispose_rec _dispose_rec;
	}

	namespace editor {
		dock_manager dock_manager::_dman;
	}
}
