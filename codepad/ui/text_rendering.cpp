#include "text_rendering.h"

/// \file
/// Implementation of certain methods related to text rendering.

#include "manager.h"

namespace codepad::ui {
	font_manager::font_manager(manager &man) : _manager(man), _atlas(man.get_renderer()) {
	}
}
