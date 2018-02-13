#pragma once

#include <vector>

#include "../core/hotkey_registry.h"
#include "element.h"

namespace codepad::ui {
	class command_registry {
	public:
		bool register_command(str_t name, std::function<void(element*)> func) {
			return _cmds.emplace(std::move(name), std::move(func)).second;
		}
		void unregister_command(const str_t &name) {
			auto it = _cmds.find(name);
			assert_true_usage(it != _cmds.end(), "unregistering inexistent command");
			_cmds.erase(it);
		}
		const std::function<void(element*)> &find_command(const str_t &name) {
			return _cmds.at(name);
		}
		const std::function<void(element*)> *try_find_command(const str_t &name) {
			auto it = _cmds.find(name);
			return it == _cmds.end() ? nullptr : &it->second;
		}

		static command_registry &get();
	protected:
		std::unordered_map<str_t, std::function<void(element*)>> _cmds;
	};

	struct element_hotkey_group_data {
		element_hotkey_group_data() = default;
		element_hotkey_group_data(const element_hotkey_group *hr, element *p) : reg(hr), param(p) {
		}

		const element_hotkey_group *reg = nullptr;
		element *param = nullptr;
	};
	struct window_hotkey_info {
		window_hotkey_info(str_t cmd, element *p) : command(std::move(cmd)), parameter(p) {
		}
		const str_t command;
		element *const parameter = nullptr;
		bool cancelled = false;
	};

	class window_hotkey_manager {
	public:
		void reset_groups(const std::vector<element_hotkey_group_data> &gp) {
			std::vector<_hotkey_group_state> hs;
			for (auto i = gp.begin(); i != gp.end(); ++i) {
				if (i->reg) {
					hs.push_back(_hotkey_group_state(*i));
				}
			}
			_reset_groups(hs);
		}
		void reset_groups_prefiltered(std::vector<element_hotkey_group_data> gp) {
#ifdef CP_DETECT_USAGE_ERRORS
			for (auto i = gp.begin(); i != gp.end(); ++i) {
				assert_true_usage(i->reg, "hotkey group has no registered target");
			}
#endif
			std::vector<_hotkey_group_state> hs;
			for (auto i = gp.begin(); i != gp.end(); ++i) {
				hs.push_back(_hotkey_group_state(*i));
			}
			_reset_groups(hs);
		}

		const std::vector<key_gesture> &get_chain() const {
			return _gests;
		}

		bool on_key_down(os::input::key k) {
			bool first = _gests.size() == 0, all_emp = true, intercept = false;
			key_gesture curg = key_gesture::get_current(k);
			_gests.push_back(curg);
			for (auto i = _groups.begin(); i != _groups.end(); ++i) {
				intercept = i->_on_keypress(curg, first) || intercept;
				if (i->state.is_trigger()) {
					window_hotkey_info hk(i->state.get_data(), i->group.param);
					i->state = element_hotkey_group::state();
#ifdef CP_DETECT_USAGE_ERRORS
					for (auto j = _groups.begin(); j != _groups.end(); ++j) {
						assert_true_usage(j->state.is_empty(), "conflicting hotkey chains detected");
					}
#endif
					triggered.invoke(hk);
					if (!hk.cancelled) {
						auto *pcmd = command_registry::get().try_find_command(hk.command);
						if (pcmd) {
							(*pcmd)(hk.parameter); // i may be invalidated since here
						} else {
							logger::get().log_warning(CP_HERE, "invalid command name");
						}
					}
					_gests.clear();
					return true;
				} else if (!i->state.is_empty()) {
					all_emp = false;
				}
			}
			if (all_emp) {
				if (_gests.size() > 1) {
					logger::get().log_info(CP_HERE, "hotkey chain interrupted");
					chain_interrupted.invoke();
				}
				_gests.clear();
			}
			return intercept;
		}

		event<window_hotkey_info> triggered;
		event<void> chain_interrupted;
	protected:
		struct _hotkey_group_state {
			_hotkey_group_state() = default;
			_hotkey_group_state(element_hotkey_group_data d) : group(d) {
			}

			element_hotkey_group_data group;
			element_hotkey_group::state state;

			bool _on_keypress(key_gesture k, bool first) {
				if (!first && state.is_empty()) {
					return false;
				}
				element_hotkey_group::state ns = group.reg->update_state(k, state);
				if (ns == state) {
					return false;
				}
				state = ns;
				return !state.is_empty();
			}
		};

		std::vector<_hotkey_group_state> _groups;
		std::vector<key_gesture> _gests;

		void _reset_groups(std::vector<_hotkey_group_state> v) {
			_groups = std::move(v);
			if (_gests.size() > 0) {
				chain_interrupted.invoke();
			}
			_gests.clear();
		}
	};
}
