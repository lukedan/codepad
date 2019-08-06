// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager of all GUI elements.

#include <map>
#include <set>
#include <chrono>
#include <functional>

#include "element.h"
#include "window.h"
#include "renderer.h"
#include "scheduler.h"
#include "commands.h"

namespace codepad::ui {
	/// Manages the update, layout, and rendering of all GUI elements, and the registration and retrieval of
	/// \ref element_state_id "element_state_ids" and transition functions.
	class manager {
		friend class window_base;
	public:
		/// Wrapper of an \ref element's constructor. The element's constructor takes a string that indicates the
		/// element's class.
		using element_constructor = std::function<element*()>;


		/// Constructor, registers predefined element states, transition functions, element types, and commands. Also
		/// bridges the \ref hotkey_listener and \ref command_registry.
		manager();
		/// Destructor. Calls \ref scheduler::dispose_marked_elements().
		~manager() {
			_scheduler.dispose_marked_elements();
		}

		/// Similar to \ref register_element_type(const str_t&, element_constructor) but for built-in classes.
		template <typename Elem> void register_element_type() {
			register_element_type(str_t(Elem::get_default_class()), []() {
				return new Elem();
				});
		}
		/// Registers a new element type for creation.
		void register_element_type(str_t type, element_constructor constructor) {
			_ctor_map.emplace(std::move(type), std::move(constructor));
		}
		/// Constructs and returns an element of the specified type, class, and \ref element_configuration. If no
		/// such type exists, \p nullptr is returned. To properly dispose of the element, use
		/// \ref scheduler::mark_for_disposal().
		element *create_element_custom(str_view_t type, str_view_t cls, const element_configuration &config) {
			auto it = _ctor_map.find(type);
			if (it == _ctor_map.end()) {
				return nullptr;
			}
			element *elem = it->second(); // the constructor must not use element::_manager
			elem->_manager = this;
			elem->_initialize(cls, config);
#ifdef CP_CHECK_USAGE_ERRORS
			assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
#endif
			return elem;
		}
		/// Calls \ref create_element_custom() to create an \ref element of the specified type and class, and with
		/// the default \ref element_metrics of that class.
		///
		/// \sa create_element_custom()
		element *create_element(str_view_t type, str_view_t cls) {
			return create_element_custom(
				type, cls, get_class_arrangements().get_or_default(cls).configuration
			);
		}
		/// Creates an element of the given type. The type name and class are both obtained from
		/// \p Elem::get_default_class().
		///
		/// \sa create_element_custom()
		/// \todo Wait for when reflection is in C++ to replace get_default_class().
		template <typename Elem> Elem *create_element() {
			element *elem = create_element(Elem::get_default_class(), Elem::get_default_class());
			Elem *res = dynamic_cast<Elem*>(elem);
			assert_true_logical(res, "incorrect get_default_class() method");
			return res;
		}

		/// Registers the given transition function.
		void register_transition_function(str_t name, transition_function func) {
			auto [it, inserted] = _transfunc_map.emplace(std::move(name), std::move(func));
			if (!inserted) {
				logger::get().log_warning(CP_HERE, "duplicate transition function name: ", name);
			}
		}
		/// Finds and returns the transition function corresponding to the given name. If none is found, \p nullptr
		/// is returned.
		transition_function try_get_transition_func(str_view_t name) const {
			auto it = _transfunc_map.find(name);
			if (it != _transfunc_map.end()) {
				return it->second;
			}
			return nullptr;
		}

		/// Returns the texture at the given path, loading it from disk when necessary.
		std::shared_ptr<bitmap> get_texture(const std::filesystem::path &path) {
			auto it = _textures.find(path);
			if (it == _textures.end()) {
				auto loaded = get_renderer().load_bitmap(path);
				auto pair = _textures.emplace(path, std::move(loaded));
				assert_true_logical(pair.second, "failed to register loaded bitmap");
				it = pair.first;
			}
			return it->second;
		}

		/// Sets the renderer used for all managed elements. Normally this only needs to be called once during
		/// program startup.
		void set_renderer(std::unique_ptr<renderer_base> r) {
			_renderer = std::move(r);
		}
		/// Returns the current renderer. \ref _renderer must have been set with \ref set_renderer().
		renderer_base &get_renderer() {
			return *_renderer;
		}

		/// Returns the registry of \ref class_arrangements "class_arrangementss" corresponding to all element
		/// classes.
		class_arrangements_registry &get_class_arrangements() {
			return _carngs;
		}
		/// Const version of \ref get_class_arrangements().
		const class_arrangements_registry &get_class_arrangements() const {
			return _carngs;
		}
		/// Returns the registry of \ref class_hotkey_group "element_hotkey_groups" corresponding to all element
		/// classes.
		class_hotkeys_registry &get_class_hotkeys() {
			return _chks;
		}
		/// Const version of \ref get_class_hotkeys().
		const class_hotkeys_registry &get_class_hotkeys() const {
			return _chks;
		}

		/// Returns the \ref scheduler.
		scheduler &get_scheduler() {
			return _scheduler;
		}
		/// \overload
		const scheduler &get_scheduler() const {
			return _scheduler;
		}

		/// Returns the registry of all commands used with hotkeys.
		command_registry &get_command_registry() {
			return _commands;
		}
		/// \overload
		const command_registry &get_command_registry() const {
			return _commands;
		}
	protected:
		class_arrangements_registry _carngs; ///< All arrangements.
		class_hotkeys_registry _chks; ///< All hotkeys.
		command_registry _commands; ///< All commands.
		/// Registry of constructors of all element types.
		std::map<str_t, element_constructor, std::less<>> _ctor_map;
		/// Mapping from names to transition functions.
		std::map<str_t, transition_function, std::less<>> _transfunc_map;

		/// The mapping between file names and textures.
		std::map<std::filesystem::path, std::shared_ptr<bitmap>> _textures; // TODO resource path

		scheduler _scheduler; ///< The \ref scheduler.
		std::unique_ptr<renderer_base> _renderer; ///< The renderer.
	};


	template <typename T> inline bool typed_animation_value_parser<T>::try_parse(
		const json::value_storage &value, [[maybe_unused]] manager &m, T &res
	) const {
		if constexpr (std::is_same_v<T, std::shared_ptr<bitmap>>) { // load textures
			if (str_view_t path; json::try_cast(value.get_value(), path)) {
				res = m.get_texture(path);
				return res != nullptr;
			}
			return false;
		} else { // parse everything else directly
			return json::object_parsers::try_parse(value.get_value(), res);
		}
	}

	template <
		typename T
	> inline std::unique_ptr<animation_definition_base> typed_animation_value_parser<T>::parse_keyframe_animation(
		const generic_keyframe_animation_definition &def, manager &m
	) const {
		using animation = keyframe_animation_definition<T, default_lerp<T>>;
		std::vector<typename animation::keyframe> keyframes;
		for (auto &&kf : def.keyframes) {
			T target;
			if (!try_parse(kf.target, m, target)) {
				logger::get().log_warning(CP_HERE, "failed to parse keyframe target");
			}
			keyframes.emplace_back(target, kf.duration, kf.transition_func);
		}
		return std::make_unique<animation>(std::move(keyframes), def.repeat_times);
	}


	namespace animation_path::builder {
		template <
			typename Output, element_property_type Type
		> void element_member_access_subject<Output, Type>::set(Output val) {
			member_access_subject<element, Output>::set(std::move(val));
			element &e = this->_object;
			if constexpr (Type == element_property_type::affects_layout) {
				e.get_manager().get_scheduler().invalidate_layout(e);
			}
			e.get_manager().get_scheduler().invalidate_visual(e);
		}
	}
}
