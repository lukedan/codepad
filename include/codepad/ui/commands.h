// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manages commands used by hotkeys.

#include <vector>

#include "hotkey_registry.h"
#include "element.h"

namespace codepad::ui {
	/// Registry for all commands. Maps strings to functions that take
	/// pointers to \ref codepad::ui::element "elements" as arguments.
	class APIGEN_EXPORT_RECURSIVE command_registry {
	public:
		/// Registers a command.
		///
		/// \param name The name used to identify this command.
		/// \param func The corresponding function. This will not be modified if there's already a command with the
		///             same name in this registry, otherwise the function is moved into this registry.
		/// \return \p false if a command of the same name has already been registered, in which case the new
		///         command will be discarded.
		bool register_command(const std::u8string &name, std::function<void(element*)> &func) {
			return _cmds.try_emplace(name, std::move(func)).second;
		}
		/// Unregisters a command.
		///
		/// \return The function associated with this command, or \p nullptr if no such command is found.
		std::function<void(element*)> unregister_command(const std::u8string &name) {
			auto it = _cmds.find(name);
			if (it == _cmds.end()) {
				return nullptr;
			}
			std::function<void(element*)> func = std::move(it->second);
			_cmds.erase(it);
			return func;
		}
		/// Finds the command with the given name. The command must have been registered.
		const std::function<void(element*)> &find_command(const std::u8string &name) {
			return _cmds.at(name);
		}
		/// Finds the command with the given name. Returns \p nullptr if none is found.
		const std::function<void(element*)> *try_find_command(const std::u8string &name) {
			auto it = _cmds.find(name);
			return it == _cmds.end() ? nullptr : &it->second;
		}

		/// Wraps a function that accepts a certain type of element into a function that accepts a \ref element.
		template <typename Elem> [[nodiscard]] inline static std::function<void(element*)> convert_type(
			std::function<void(Elem*)> f
		) {
			static_assert(std::is_base_of_v<element, Elem>, "invalid element type");
			return[func = std::move(f)](element *e) {
				Elem *te = dynamic_cast<Elem*>(e);
				if (e != nullptr && te == nullptr) { // not the right type
					logger::get().log_warning(CP_HERE) <<
						"callback with invalid element type " << demangle(typeid(*e).name()) <<
						", expected " << demangle(typeid(Elem).name());
					return;
				}
				func(te);
			};
		}
	protected:
		/// The map that stores all registered commands.
		std::unordered_map<std::u8string, std::function<void(element*)>> _cmds;
	};

	/// Holds the information of an element and its corresponding \ref hotkey_group.
	/// Used with \ref window_hotkey_manager.
	struct element_hotkey_group_data {
		/// Default constructor.
		element_hotkey_group_data() = default;
		/// Constructs the struct with the given parameters.
		element_hotkey_group_data(const hotkey_group *hr, element *p) : reg(hr), param(p) {
		}

		const hotkey_group *reg = nullptr; ///< The element's corresponding hotkey group.
		element *param = nullptr; ///< An element.
	};
	/// Contains information about the user pressing a hotkey.
	struct hotkey_info {
		/// Constructs the info with corresponding data.
		hotkey_info(std::u8string cmd, element *p) : command(std::move(cmd)), parameter(p) {
		}
		const std::u8string command; ///< The command corresponding to the hotkey.
		element *const parameter = nullptr; ///< The element on which the hotkey is registered.
	};

	/// Manages and monitors hotkeys. At any time only the hotkeys registered to the classes of certain elements are
	/// active. The elements include the currently focused element and all its parents.
	class hotkey_listener {
	public:
		/// Called when the focus has shifted to reset the set of active \ref hotkey_group "hotkey_groups".
		void reset_groups(const std::vector<element_hotkey_group_data>&);

		/// Returns the current set of valid user gestures. For example, if the user is halfway through a valid
		/// hotkey chain, this function returns all gestures that the user has made. If the user inputs an invalid
		/// gesture, the current chain will be interrupted and this function will return nothing until another valid
		/// gesture is nade.
		const std::vector<key_gesture> &get_chain() const {
			return _gests;
		}

		/// Called when a key event is received by a window.
		///
		/// \return \p true if the gesture is accepted and consumed here and should not be processed as a key stroke.
		bool on_key_down(key_gesture);

		info_event<hotkey_info> triggered; ///< An event invoked whenever the user enters a hotkey.
		/// An event invoked when the user enters an invalid gesture and breaks the chain.
		info_event<> chain_interrupted;
	protected:
		/// Records the state of a hotkey group.
		struct _hotkey_group_state {
			/// Default constructor.
			_hotkey_group_state() = default;
			/// Constructs the state with a given group data.
			_hotkey_group_state(element_hotkey_group_data d) : group(d) {
			}

			element_hotkey_group_data group; ///< The group to which the state belongs.
			hotkey_group::state state; ///< The state of the group.

			/// Invoked when the user enters a key gesture to update the state.
			///
			/// \param k The key gesture that the user entered.
			/// \param first Indicates whether this is the start of a hotkey.
			/// \return \p true if the gesture is intercepted.
			bool on_keypress(key_gesture k, bool first);
		};

		std::vector<_hotkey_group_state> _groups; ///< Currently active groups.
		std::vector<key_gesture> _gests; ///< Gestures entered by the user so far.

		/// Resets the active groups. Breaks the current hotkey if necessary.
		void _reset_groups(std::vector<_hotkey_group_state>);
	};
}
