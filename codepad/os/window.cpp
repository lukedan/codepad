#include "window.h"

/// \file
/// Implementation of certain functions of \ref codepad::os::window_base.

#include "../ui/manager.h"

using namespace codepad::ui;

namespace codepad::os {
	void window_base::_on_got_window_focus() {
		ui::manager::get()._on_window_got_focus(*this);
		_focus->_on_got_focus();
		got_window_focus.invoke();
	}

	void window_base::_on_lost_window_focus() {
		ui::manager::get()._on_window_lost_focus(*this);
		_focus->_on_lost_focus();
		lost_window_focus.invoke();
	}

	void window_base::_dispose() {
		// special care taken
		auto i = _decos.begin(), j = i;
		for (; i != _decos.end(); i = j) {
			++j;
			delete *i;
		}
		if (ui::manager::get().get_focused_window() == this) {
			// TODO should it be _on_lost_window_focus() here?
			ui::manager::get()._on_window_lost_focus(*this);
		}
		renderer_base::get()._delete_window(*this);
		panel::_dispose();
	}
}
