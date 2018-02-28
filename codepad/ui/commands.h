#pragma once

/// \file commands.h
/// Manages commands used by hotkeys.

#include <vector>

#include "../core/hotkey_registry.h"
#include "element.h"

namespace codepad::ui {
	/// Registry for all commands. Maps strings to functions that take
	/// pointers to \ref codepad::ui::element "elements" as arguments.
	class command_registry {
	public:
		/// Registers a command.
		///
		/// \param name The name used to identify this command.
		/// \param func The corresponding function.
		/// \return \p false if a command of the same name has already been registered.
		bool register_command(str_t name, std::function<void(element*)> func) {
			return _cmds.emplace(std::move(name), std::move(func)).second;
		}
		/// Unregisters a command. The command must have been registered.
		void unregister_command(const str_t &name) {
			auto it = _cmds.find(name);
			assert_true_usage(it != _cmds.end(), "unregistering inexistent command");
			_cmds.erase(it);
		}
		/// Finds the command with the given name. The command must have been registered.
		const std::function<void(element*)> &find_command(const str_t &name) {
			return _cmds.at(name);
		}
		/// Finds the command with the given name. Returns \p nullptr if none is found.
		const std::function<void(element*)> *try_find_command(const str_t &name) {
			auto it = _cmds.find(name);
			return it == _cmds.end() ? nullptr : &it->second;
		}

		/// Gets the global \ref command_registry.
		static command_registry &get();
	protected:
		/// The map that stores all registered commands.
		std::unordered_map<str_t, std::function<void(element*)>> _cmds;
	};

	/// Holds the information of an element and its corresponding \ref element_hotkey_group.
	/// Used with \ref window_hotkey_manager.
	struct element_hotkey_group_data {
		/// Default constructor.
		element_hotkey_group_data() = default;
		/// Constructs the struct with the given parameters.
		element_hotkey_group_data(const element_hotkey_group *hr, element *p) : reg(hr), param(p) {
		}

		const element_hotkey_group *reg = nullptr; ///< The element's corresponding hotkey group.
		element *param = nullptr; ///< An element.
	};
	/// Contains information about the user pressing a hotkey.
	struct window_hotkey_info {
		/// Constructs the info with corresponding data.
		window_hotkey_info(str_t cmd, element *p) : command(std::move(cmd)), parameter(p) {
		}
		const str_t command; ///< The command corresponding to the hotkey.
		element *const parameter = nullptr; ///< The element on which the hotkey is registered.
		bool cancelled = false; ///< Event handlers can set this to \p true to cancel the hotkey.
	};

	/// Manages the hotkeys. At any time only the hotkeys registered to the classes of certain elements are active.
	/// The elements include the currently focused element and all its parents.
	class window_hotkey_manager {
	public:
		/// Called when the focus has shifted to reset the set of
		/// active \ref element_hotkey_group "element_hotkey_groups".
		///
		/// \param gp The new set of active hotkey groups.
		void reset_groups(const std::vector<element_hotkey_group_data> &gp) {
			std::vector<_hotkey_group_state> hs;
			// filter out all empty entries
			for (auto i = gp.begin(); i != gp.end(); ++i) {
				if (i->reg) {
					hs.push_back(_hotkey_group_state(*i));
				}
			}
			_reset_groups(std::move(hs));
		}

		/// Returns the current set of valid user gestures.
		/// For example, if the user is halfway through a valid hotkey,
		/// this function returns all gestures that the user has made.
		/// If the user makes an invalid gesture, the function returns nothing.
		const std::vector<key_gesture> &get_chain() const {
			return _gests;
		}

		/// Called when a key event is received by a window.
		///
		/// \return \p true if the gesture is processed here and should not be processed as a key stroke.
		bool on_key_down(key_gesture k) {
			bool
				first = _gests.size() == 0,
				all_emp = true, // true if all updated states are empty, i.e., end of hotkey chain
				intercept = false;
			for (auto i = _groups.begin(); i != _groups.end(); ++i) {
				// updates the hotkey and checks whether it is intercepted
				// note that it can be intercepted even if the gesture's invalid
				// in which case the state will be reset
				intercept = i->on_keypress(k, first) || intercept;
				if (i->state.is_trigger()) { // reached leaf node, trigger
					window_hotkey_info hk(i->state.get_data(), i->group.param);
					i->state = element_hotkey_group::state(); // reset state
					for (auto j = _groups.begin(); j != _groups.end(); ++j) {
						// all state should be empty, otherwise there are conflicts
						if (!j->state.is_empty()) {
							logger::get().log_warning(CP_HERE, "found conflicting hotkey chains");
							j->state = element_hotkey_group::state(); // clear them
						}
					}
					triggered.invoke(hk); // invoke the event
					if (!hk.cancelled) {
						auto *pcmd = command_registry::get().try_find_command(hk.command);
						if (pcmd) {
							// invoke the command. i may be invalidated
							(*pcmd)(hk.parameter);
						} else {
							logger::get().log_warning(CP_HERE, "invalid command name");
						}
					}
					_gests.clear();
					return true;
				} else if (!i->state.is_empty()) { // check if there's ongoing hotkeys
					all_emp = false;
				}
			}
			if (all_emp) {
				if (_gests.size() > 1) {
					logger::get().log_info(CP_HERE, "hotkey chain interrupted");
					chain_interrupted.invoke();
				}
				_gests.clear();
			} else {
				_gests.push_back(k);
			}
			return intercept;
		}

		event<window_hotkey_info> triggered; ///< An event invoked whenever the user enters a hotkey.
		/// An event invoked when the user enters an invalid gesture and breaks the chain.
		event<void> chain_interrupted;
	protected:
		/// Records the state of a hotkey group.
		struct _hotkey_group_state {
			/// Default constructor.
			_hotkey_group_state() = default;
			/// Constructs the state with a given group data.
			_hotkey_group_state(element_hotkey_group_data d) : group(d) {
			}

			element_hotkey_group_data group; ///< The group to which the state belongs.
			element_hotkey_group::state state; ///< The state of the group.

			/// Invoked when the user enters a key gesture to update the state.
			///
			/// \param k The key gesture that the user entered.
			/// \param first Indicates whether this is the start of a hotkey.
			/// \return \p true if the gesture is intercepted.
			bool on_keypress(key_gesture k, bool first) {
				if (!first && state.is_empty()) {
					// already out of matching gestures
					return false;
				}
				element_hotkey_group::state ns = group.reg->update_state(k, state);
				if (ns == state) { // no update
					return false;
				}
				state = ns;
				return true; // intercepted (accepted or rejected)
			}
		};

		std::vector<_hotkey_group_state> _groups; ///< Currently active groups.
		std::vector<key_gesture> _gests; ///< Gestures entered by the user so far.

		/// Resets the active groups. Breaks the current hotkey if necessary.
		void _reset_groups(std::vector<_hotkey_group_state> v) {
			_groups = std::move(v);
			if (_gests.size() > 0) {
				// the focus has shifted when the user is midway through a hotkey
				chain_interrupted.invoke();
				_gests.clear(); // start anew
			}
		}
	};
}
