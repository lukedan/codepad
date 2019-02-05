// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional classes that handle different element classes.

#include <map>

#include "../core/json.h"
#include "hotkey_registry.h"
#include "visual.h"
#include "arrangements.h"

namespace codepad::ui {
	/// This struct is used to handle the transition between different \ref visual_state or \ref metrics_state of the
	/// same class.
	///
	/// \todo Add proper mechanism to detect when \ref _ctrl or \ref _registry is out of date.
	template <typename StateValue> struct configuration {
	public:
		/// Default constructor.
		configuration() = default;
		/// Initializes the configuration with the given \ref state_mapping and \ref element_state_id. If the
		/// \ref element_state_id is not given, the state is set to \ref normal_element_state_id.
		explicit configuration(
			const state_mapping<StateValue> &reg, element_state_id state = normal_element_state_id,
			animation_time_point_t now = animation_clock_t::now()
		) : _registry(&reg) {
			_ctrl = &_registry->match_state_or_first(state);
			_state = typename StateValue::state(*_ctrl, now);
		}

		/// Returns the element's \p StateValue::state, i.e., its current state regarding to the value. Note that
		/// this is not used to obtain a \ref element_state_id.
		typename StateValue::state &get_state() {
			return _state;
		}
		/// \overload
		const typename StateValue::state &get_state() const {
			return _state;
		}
		/// Returns the value that controlls how \ref _state is obtained.
		const StateValue &get_controller() const {
			return *_ctrl;
		}

		/// Calls \p StateValue::render to render in the specified region. This function will successfully
		/// instantiate only if \p StateValue has a corresponding \p render() function.
		void render(renderer_base &r, rectd rgn) const {
			_ctrl->render(r, rgn, _state);
		}

		/// Updates \ref _state with \ref _registry if \ref _state is not stationary.
		///
		/// \return Whether \ref _state, after having been updated, is stationary.
		animation_duration_t update(animation_time_point_t now) {
			return _ctrl->update(_state, now);
		}

		/// Called when the state has been changed to update \ref _state and \ref _ctrl properly.
		void on_state_changed(element_state_id s, animation_time_point_t now = animation_clock_t::now()) {
			if (_registry) {
				_ctrl = &_registry->match_state_or_first(s);
				_state = typename StateValue::state(_state, now);
			}
		}
	protected:
		/// The current state.
		typename StateValue::state _state;
		/// The \ref state_mapping that contains the \p StateValue used to initialize \ref _state.
		const state_mapping<StateValue> *_registry = nullptr;
		/// The value used to initialize \ref _state.
		const StateValue *_ctrl = nullptr;
	};
	/// Stores the state of an \ref element's visual.
	using visual_configuration = configuration<visual_state>;
	/// Stores the state of an \ref element's metrics.
	using metrics_configuration = configuration<metrics_state>;

	/// Hotkey groups that are used with elements. Each hotkey corresponds to a command name.
	using class_hotkey_group = hotkey_group<str_t>;


	/// Registry of a certain attribute of each element class.
	template <typename T> class class_registry {
	public:
		std::map<str_t, T> mapping; ///< The mapping between class names and attribute values.

		/// Returns the attribute corresponding to the given class name. If noine exists, the default attribute (the
		/// one corresponding to an empty class name) is returned.
		T &get_or_default(const str_t &cls) {
			auto found = mapping.find(cls);
			return found == mapping.end() ? mapping[str_t()] : found->second;
		}
	};
	/// Registry of the visual of each element class.
	using class_visuals_registry = class_registry<class_visual>;
	/// Registry of the arrangements of each element class.
	using class_arrangements_registry = class_registry<class_arrangements>;

	/// Registry of \ref class_hotkey_group "element_hotkey_groups".
	class class_hotkeys_registry {
	public:
		std::map<str_t, class_hotkey_group> hotkeys; ///< The mapping between class names and hotkey groups.

		/// Finds the hotkey group corresponding to the class name given.
		///
		/// \return Pointer to the hotkey group. \p nullptr if none is found.
		const class_hotkey_group *try_get(const str_t &cls) const {
			auto it = hotkeys.find(cls);
			if (it != hotkeys.end()) {
				return &it->second;
			}
			return nullptr;
		}
	};


	/// Configuration of an element's state, layout, hotkeys, and visuals.
	struct element_configuration {
		visual_configuration visual_config; ///< Visual configurations.
		metrics_configuration metrics_config; ///< Layout configurations.
		const class_hotkey_group *hotkey_config = nullptr; ///< Hotkey configurations.

		/// Changes the state of the both configurations.
		void on_state_changed(element_state_id st, animation_time_point_t now = animation_clock_t::now()) {
			visual_config.on_state_changed(st, now);
			metrics_config.on_state_changed(st, now);
		}
		/// Checks if all animations have finished.
		bool all_stationary() const {
			return visual_config.get_state().all_stationary && metrics_config.get_state().all_stationary;
		}
		/// Updates \ref visual_config and \ref metrics_config.
		animation_duration_t update(animation_time_point_t now) {
			return std::min(visual_config.update(now), metrics_config.update(now));
		}
	};
}
