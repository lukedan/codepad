#include "window.h"

/// \file
/// Implementation of certain functions of \ref codepad::os::window_base.

#include <vector>

#include "../ui/manager.h"

using namespace std;

using namespace codepad::ui;

namespace codepad::os {
	void window_base::set_window_focused_element(element &e) {
		assert_true_logical(e.get_window() == this, "corrupted element tree");
		if (&e != _focus) {
			element *oldfocus = _focus;
			_focus = &e;
			vector<element_hotkey_group_data> gps;
			for (element *cur = _focus; cur != nullptr; cur = cur->parent()) {
				const element_hotkey_group
					*gp = manager::get().get_class_hotkeys().find(cur->get_class());
				gps.push_back(element_hotkey_group_data(gp, cur));
			}
			hotkey_manager.reset_groups(gps);
			oldfocus->_on_lost_focus();
			e._on_got_focus();
		}
	}

	void window_base::_on_got_window_focus() {
		manager::get()._on_window_got_focus(*this);
		_focus->_on_got_focus();
		got_window_focus.invoke();
	}

	void window_base::_on_lost_window_focus() {
		manager::get()._on_window_lost_focus(*this);
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
		if (manager::get().get_focused_window() == this) {
			// TODO should it be _on_lost_window_focus() here?
			manager::get()._on_window_lost_focus(*this);
		}
		renderer_base::get()._delete_window(*this);
		panel::_dispose();
	}

	void window_base::_custom_render() {
		panel::_custom_render();
		// render decorations
		bool has_active = false;
		for (auto i = _decos.begin(); i != _decos.end(); ) {
			if ((*i)->_st.update_and_render((*i)->_layout)) {
				has_active = true;
			} else {
				if (test_bit_all(
					(*i)->get_state(), manager::get().get_predefined_states().corpse
				)) { // a dead corpse
					auto j = i;
					++j;
					delete *i; // the entry in _decos will be automatically removed here
					i = j;
					continue;
				}
			}
			++i;
		}
		if (has_active) {
			invalidate_visual();
		}
	}
}
