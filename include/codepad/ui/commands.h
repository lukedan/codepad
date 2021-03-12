// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manages commands used by hotkeys.

#include <vector>

#include "../core/event.h"
#include "hotkey_registry.h"

namespace codepad::ui {
	class element;

	using command = std::function<void(element*, const json::value_storage&)>; ///< Callback function for commands.
	/// Registry for all commands. Maps strings to functions that take
	/// pointers to \ref codepad::ui::element "elements" as arguments.
	class APIGEN_EXPORT_RECURSIVE command_registry {
	public:
		/// Used to easily register and unregister commands with error handling.
		struct stub {
			/// Initializes this command.
			stub(std::u8string n, command f) : _name(std::move(n)), _exec(std::move(f)) {
			}
			/// Move constructor.
			stub(stub &&src) noexcept :
				_name(std::move(src._name)), _exec(std::move(src._exec)), _registered(src._registered) {
				src._registered = false;
			}
			/// No copy construction.
			stub(const stub&) = delete;
			/// Move assignment.
			stub &operator=(stub &&src) noexcept {
				assert_true_usage(!_registered, "command must be manually unregistered before move assignment");
				_name = std::move(src._name);
				_exec = std::move(src._exec);
				_registered = src._registered;
				src._registered = false;
				return *this;
			}
			/// No copy assignment.
			stub &operator=(const stub&) = delete;
			/// Checks that the command is unregistered.
			~stub() {
				assert_true_usage(!_registered, "command must be manually unregistered before disposing");
			}

			/// Registers this command.
			void register_command(command_registry &reg) {
				if (reg.register_command(_name, _exec)) {
					_registered = true;
				} else {
					logger::get().log_warning(CP_HERE) << "failed to register command: " << _name;
				}
			}
			/// Unregisters this command.
			void unregister_command(command_registry &reg) {
				if (_registered) {
					_exec = reg.unregister_command(_name);
					_registered = false;
				}
			}
		private:
			std::u8string _name; ///< The name of this command.
			command _exec; ///< The function to be executed.
			bool _registered = false; ///< Whether this command has been registered.
		};


		/// Registers a command.
		///
		/// \param name The name used to identify this command.
		/// \param func The corresponding function. This will not be modified if there's already a command with the
		///             same name in this registry, otherwise the function is moved into this registry, leaving the
		///             original variable in a moved-from state.
		/// \return \p false if a command of the same name has already been registered, in which case the new
		///         command will be discarded.
		[[nodiscard]] bool register_command(std::u8string name, command &func) {
			return _cmds.try_emplace(std::move(name), std::move(func)).second;
		}
		/// Unregisters a command.
		///
		/// \return The function associated with this command, or \p nullptr if no such command is found.
		[[nodiscard]] command unregister_command(std::u8string_view name) {
			auto it = _cmds.find(name);
			if (it == _cmds.end()) {
				return nullptr;
			}
			command func = std::move(it->second);
			_cmds.erase(it);
			return func;
		}
		/// Finds the command with the given name. Returns \p nullptr if none is found.
		[[nodiscard]] const command *find_command(std::u8string_view name) const {
			auto it = _cmds.find(name);
			return it == _cmds.end() ? nullptr : &it->second;
		}

		/// Wraps a function that accepts a certain type of element into a function that accepts a \ref element.
		template <typename Elem> [[nodiscard]] inline static command convert_type(
			std::function<void(Elem&, const json::value_storage&)> f
		) {
			static_assert(std::is_base_of_v<element, Elem>, "invalid element type");
			return[func = std::move(f)](element *e, const json::value_storage &args) {
				Elem *te = dynamic_cast<Elem*>(e);
				if (e != nullptr && te == nullptr) { // not the right type
					logger::get().log_warning(CP_HERE) <<
						"callback with invalid element type " << demangle(typeid(*e).name()) <<
						", expected " << demangle(typeid(Elem).name());
					return;
				}
				func(*te, args);
			};
		}
	protected:
		/// The map that stores all registered commands.
		std::unordered_map<std::u8string, command, string_hash<>, std::equal_to<>> _cmds;
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
		hotkey_info(const hotkey_group::action &cmd, element *p) : command(cmd), subject(p) {
		}
		const hotkey_group::action &command; ///< The command corresponding to the hotkey.
		element *const subject = nullptr; ///< The element on which the hotkey is registered.
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
