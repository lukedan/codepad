#include "font.h"
#include "../ui/manager.h"
#include "../ui/commonelements.h"

namespace codepad {
	font::_library font::_lib;
	namespace ui {
		manager manager::_sman;

		unsigned char text_element::_def_fnt_ts = 0;
		font *text_element::_def_fnt = nullptr;
	}
}
