// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "window.h"

/// \file
/// Implementation of certain functions of \ref codepad::os::window_base.

#include <vector>

#include "manager.h"

using namespace std;

namespace codepad::ui {
	void window_base::set_window_focused_element(element &e) {
		assert_true_logical(e.get_window() == this, "corrupted element tree");
		if (&e != _focus) {
			element *oldfocus = _focus;
			_focus = &e;
			vector<element_hotkey_group_data> gps;
			for (element *cur = _focus; cur != nullptr; cur = cur->parent()) {
				gps.emplace_back(cur->_config.hotkey_config, cur);
			}
			hotkey_manager.reset_groups(gps);
			oldfocus->_on_lost_focus();
			e._on_got_focus();
		}
	}

	void window_base::register_decoration(decoration &dec) {
		assert_true_usage(dec._wnd == nullptr, "the decoration has already been registered to another window");
		dec._wnd = this;
		_decos.push_back(&dec);
		dec._tok = --_decos.end();
		get_manager().schedule_visual_config_update(*this);
	}

	void window_base::_on_prerender() {
		get_manager().get_renderer().begin(*this);
		panel::_on_prerender();
	}

	void window_base::_on_postrender() {
		panel::_on_postrender();
		get_manager().get_renderer().end();
	}

	void window_base::_on_got_window_focus() {
		get_manager()._on_window_got_focus(*this);
		_focus->_on_got_focus();
		got_window_focus.invoke();
	}

	void window_base::_on_lost_window_focus() {
		get_manager()._on_window_lost_focus(*this);
		_focus->_on_lost_focus();
		lost_window_focus.invoke();
	}

	void window_base::_initialize(const str_t &cls, const ui::element_metrics &metrics) {
		panel::_initialize(cls, metrics);
		get_manager().get_renderer()._new_window(*this);
	}

	void window_base::_dispose() {
		// special care taken
		auto i = _decos.begin(), j = i;
		for (; i != _decos.end(); i = j) {
			++j;
			delete *i;
		}
		if (get_manager().get_focused_window() == this) {
			// TODO should it be _on_lost_window_focus() here?
			get_manager()._on_window_lost_focus(*this);
		}
		get_manager().get_renderer()._delete_window(*this);
		panel::_dispose();
	}

	void window_base::_on_size_changed(size_changed_info &p) {
		get_manager().notify_layout_change(*this);
		size_changed(p);
	}

	bool window_base::_on_update_visual_configurations(double time) {
		bool stationary = true;
		for (auto i = _decos.begin(); i != _decos.end(); ) {
			if ((*i)->_vis_config.update(get_manager().update_delta_time())) {
				if (((*i)->get_state() & get_manager().get_predefined_states().corpse) != 0) {
					auto j = i;
					++i;
					delete *j; // the decoration will remove itself from _decos in its destructor
					continue;
				}
			} else {
				stationary = false;
			}
			++i;
		}
		return panel::_on_update_visual_configurations(time) && stationary;
	}

	void window_base::_custom_render() {
		panel::_custom_render();
		for (decoration *dec : _decos) {
			dec->_vis_config.render(get_manager().get_renderer(), dec->_layout);
		}
	}
}
