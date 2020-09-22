// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/commands.h"

/// \file
/// Implementation of keyboard command detection.

namespace codepad::ui {
	void hotkey_listener::reset_groups(const std::vector<element_hotkey_group_data> &gp) {
		std::vector<_hotkey_group_state> hs;
		// filter out all empty entries
		for (const element_hotkey_group_data &data : gp) {
			if (data.reg) {
				hs.emplace_back(data);
			}
		}
		_reset_groups(std::move(hs));
	}

	bool hotkey_listener::on_key_down(key_gesture k) {
		bool
			first = _gests.empty(),
			all_emp = true, // true if all updated states are empty, i.e., end of hotkey chain
			intercept = false;
		for (_hotkey_group_state &st : _groups) {
			// updates the hotkey and checks whether it is intercepted
			// note that it can be intercepted even if the gesture's invalid
			// in which case the state will be reset
			intercept = st.on_keypress(k, first) || intercept;
			if (st.state.is_trigger()) { // reached leaf node, trigger
				hotkey_info hk(st.state.get_action(), st.group.param);
				st.state = hotkey_group::state(); // reset state
				for (_hotkey_group_state &j : _groups) {
					// all state should be empty, otherwise there are conflicts
					if (!j.state.is_empty()) {
						logger::get().log_warning(CP_HERE) << "found conflicting hotkey chains";
						j.state = hotkey_group::state(); // clear them
					}
				}
				triggered.invoke(hk); // invoke the event
				_gests.clear();
				return true;
			}
			if (!st.state.is_empty()) { // check if there's ongoing hotkeys
				all_emp = false;
			}
		}
		if (all_emp) {
			if (_gests.size() > 1) {
				logger::get().log_debug(CP_HERE) << "hotkey chain interrupted";
				chain_interrupted.invoke();
			}
			_gests.clear();
		} else {
			_gests.push_back(k);
		}
		return intercept;
	}

	void hotkey_listener::_reset_groups(std::vector<_hotkey_group_state> v) {
		_groups = std::move(v);
		if (!_gests.empty()) {
			// the focus has shifted when the user is midway through a hotkey
			chain_interrupted.invoke();
			_gests.clear(); // start anew
		}
	}


	bool hotkey_listener::_hotkey_group_state::on_keypress(key_gesture k, bool first) {
		if (!first && state.is_empty()) {
			// already out of matching gestures
			return false;
		}
		hotkey_group::state ns = group.reg->update_state(k, state);
		if (ns == state) { // no update
			return false;
		}
		state = ns;
		return true; // intercepted (accepted or rejected)
	}
}
