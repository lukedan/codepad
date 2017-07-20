#pragma once

#include <vector>

#include "../utilities/hotkey_registry.h"
#include "element.h"

namespace codepad {
	namespace ui {
		struct element_hotkey_group_data {
			element_hotkey_group_data() = default;
			element_hotkey_group_data(const element_hotkey_group *hr, element *p) : reg(hr), param(p) {
			}

			const element_hotkey_group *reg = nullptr;
			element *param = nullptr;
		};
		struct window_hotkey_info {
			window_hotkey_info(const std::function<void(element*)> &f, element *p) : callback(f), parameter(p) {
			}
			const std::function<void(element*)> &callback;
			element *const parameter;
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
					assert_true_usgerr(i->reg, "hotkey group has no registered target");
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
					intercept = intercept || i->_on_keypress(curg, first);
					if (i->state.is_trigger()) {
						window_hotkey_info hk(i->state.get_callback(), i->group.param);
						triggered.invoke(hk);
						if (!hk.cancelled) {
							hk.callback(hk.parameter);
						}
						i->state = element_hotkey_group::state();
						_gests.clear();
#ifdef CP_DETECT_USAGE_ERRORS
						for (auto j = _groups.begin(); j != _groups.end(); ++j) {
							assert_true_usgerr(j->state.is_empty(), "conflicting hotkey chains detected");
						}
#endif
						return true;
					} else if (!i->state.is_empty()) {
						all_emp = false;
					}
				}
				if (all_emp) {
					if (_gests.size() > 1) {
						CP_INFO("hotkey chain interrupted");
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
					return true;
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
}
