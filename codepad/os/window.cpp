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
				gps.push_back(element_hotkey_group_data(cur->_config.hotkey_config, cur));
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

	void window_base::_on_update() {
		panel::_on_update();
		bool needupdate = false, newvisual = false;
		for (auto i = _decos.begin(); i != _decos.end(); ) {
			if (!(*i)->_vis_config.get_state().all_stationary) {
				newvisual = true;
				if ((*i)->_vis_config.update(manager::get().update_delta_time())) {
					if (test_bits_all((*i)->get_state(), manager::get().get_predefined_states().corpse)) {
						auto j = i;
						++i;
						delete *j;
						continue;
					}
				} else {
					needupdate = true;
				}
			}
			++i;
		}
		if (newvisual) { // otherwise nothing's been updated
			invalidate_visual();
			if (needupdate) {
				ui::manager::get().schedule_update(*this);
			}
		}
	}

	void window_base::_custom_render() {
		panel::_custom_render();
		for (decoration *dec : _decos) {
			dec->_vis_config.render(dec->_layout);
		}
	}
}
