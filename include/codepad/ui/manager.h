// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager of all GUI elements.

#include <map>
#include <set>
#include <chrono>
#include <functional>

#include "../apigen_definitions.h"
#include "../core/settings.h"
#include "element.h"
#include "window.h"
#include "renderer.h"
#include "scheduler.h"
#include "commands.h"
#include "config_parsers.h"

namespace codepad::ui {
	/// This manages various UI-related registries and resources, acting as the core of all UI code.
	class APIGEN_EXPORT_RECURSIVE manager {
		friend class window;
	public:
		/// Wrapper of an \ref element's constructor. The element's constructor takes a string that indicates the
		/// element's class.
		using element_constructor = std::function<element*()>;


		/// Constructor, registers predefined element states, transition functions, element types, and commands. Also
		/// bridges the \ref hotkey_listener and \ref command_registry.
		explicit manager(settings&);
		/// Destructor. Calls \ref scheduler::dispose_marked_elements().
		~manager() {
			_scheduler.dispose_marked_elements();
			// since the textures are allocated by the renderer, release them all first
			_textures.clear();
		}

		/// Registers a new element type for creation.
		void register_element_type(std::u8string type, element_constructor ctor) {
			_element_registry.emplace(std::move(type), std::move(ctor));
		}
		/// Overload of \ref register_element_type() that uses \p Elem::get_default_class() as the type name.
		template <typename Elem> void register_element_type() {
			register_element_type(
				std::u8string(Elem::get_default_class()),
				[]() {
					return new Elem();
				}
			);
		}
		/// Unregisters the given element type, returning \p false if no such type has been registered.
		bool unregister_element_type(std::u8string_view type) {
			auto it = _element_registry.find(type);
			if (it != _element_registry.end()) {
				_element_registry.erase(it);
				return true;
			}
			return false;
		}
		/// Overload of \ref unregister_element_type() that uses \p Elem::get_default_class() as the type name.
		template <typename Elem> bool unregister_element_type() {
			return unregister_element_type(Elem::get_default_class());
		}

		/// Constructs and returns an element of the specified type, class, and \ref element_configuration. This
		/// function does not handle event triggers and animations.
		element *create_element_custom_no_event_or_property(
			std::u8string_view type, std::u8string_view cls
		);
		/// Constructs an element by calling \ref create_element_custom_no_event_or_property(), then registers
		/// triggers and sets properties of the element if the element is not derived from \ref panel. The events are
		/// registered before properties are set so that they can be invoked accordingly.
		element *create_element_custom(
			std::u8string_view type, std::u8string_view cls, const element_configuration&
		);
		/// Calls \ref create_element_custom() to create an \ref element of the specified type and class, and with
		/// the default \ref element_configuration of that class.
		///
		/// \sa create_element_custom()
		element *create_element(std::u8string_view type, std::u8string_view cls) {
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
		void register_transition_function(std::u8string name, transition_function func) {
			auto [it, inserted] = _transfunc_map.emplace(std::move(name), std::move(func));
			if (!inserted) {
				logger::get().log_warning(CP_HERE) << "duplicate transition function name: " << name;
			}
		}
		/// Finds and returns the transition function corresponding to the given name. If none is found, \p nullptr
		/// is returned.
		transition_function find_transition_function(std::u8string_view name) const {
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
				// TODO implement proper assets and image sets
				auto loaded = get_renderer().load_bitmap(path, vec2d(1.0, 1.0));
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
		/// Returns whether there's a renderer available. Normally, users can assume that one is present.
		[[nodiscard]] bool has_renderer() const {
			return _renderer != nullptr;
		}
		/// Returns the current renderer. \ref _renderer must have been set with \ref set_renderer().
		[[nodiscard]] renderer_base &get_renderer() {
			return *_renderer;
		}

		/// Returns the registry of \ref class_arrangements "class_arrangementss" corresponding to all element
		/// classes.
		class_arrangements_registry &get_class_arrangements() {
			return _class_arrangements;
		}
		/// Const version of \ref get_class_arrangements().
		const class_arrangements_registry &get_class_arrangements() const {
			return _class_arrangements;
		}
		/// Returns the registry of \ref hotkey_group "hotkey_groups" corresponding to all element
		/// classes.
		class_hotkeys_registry &get_class_hotkeys() {
			return _class_hotkeys;
		}
		/// Const version of \ref get_class_hotkeys().
		const class_hotkeys_registry &get_class_hotkeys() const {
			return _class_hotkeys;
		}

		/// Finds the color that corresponds to the given name in the color scheme.
		std::optional<colord> find_color(std::u8string_view name) const {
			if (auto it = _color_scheme.find(name); it != _color_scheme.end()) {
				return it->second;
			}
			return std::nullopt;
		}
		/// Returns the current color scheme.
		const std::map<std::u8string, colord, std::less<>> &get_color_scheme() const {
			return _color_scheme;
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

		/// Returns the \ref settings object associated with this manager.
		settings &get_settings() const {
			return _settings;
		}

		/// Reloadss all visual configuration, including arrangements and the color scheme.
		template <typename ValueType> void reload_visuals(const ValueType &value) {
			_color_scheme.clear();
			_class_arrangements.mapping.clear();

			arrangements_parser<ValueType> parser(*this);
		}
	protected:
		class_arrangements_registry _class_arrangements; ///< All class arrangements.
		class_hotkeys_registry _class_hotkeys; ///< All class hotkeys.
		std::map<std::u8string, colord, std::less<>> _color_scheme; ///< The color scheme.

		command_registry _commands; ///< All commands.
		/// Registry of constructors of all element types.
		std::map<std::u8string, element_constructor, std::less<>> _element_registry;
		/// Mapping from names to transition functions.
		std::map<std::u8string, transition_function, std::less<>> _transfunc_map;

		/// The mapping between file names and textures.
		std::map<std::filesystem::path, std::shared_ptr<bitmap>> _textures; // TODO resource path

		scheduler _scheduler; ///< The \ref scheduler.
		std::unique_ptr<renderer_base> _renderer; ///< The renderer.
		settings &_settings; ///< The settings associated with this \ref manager.
	};


	template <
		typename T
	> inline std::shared_ptr<animation_definition_base> typed_animation_value_handler<T>::parse_keyframe_animation(
		const generic_keyframe_animation_definition &def
	) const {
		using animation = keyframe_animation_definition<T>;
		std::vector<typename animation::keyframe> keyframes;
		for (auto &&kf : def.keyframes) {
			if (auto target = parse(kf.target)) {
				keyframes.emplace_back(target.value(), kf.duration, kf.transition_func);
			} else {
				logger::get().log_error(CP_HERE) << "failed to parse keyframe target";
			}
		}
		return std::make_shared<animation>(std::move(keyframes), *this, def.repeat_times);
	}


	template <typename T> inline std::optional<T> managed_animation_value_handler<T>::parse_static(
		const json::value_storage &value, [[maybe_unused]] manager &m
	) {
		if constexpr (std::is_same_v<T, std::shared_ptr<bitmap>>) { // load textures
			if (auto path = value.get_value().cast<std::u8string_view>()) {
				return m.get_texture(path.value());
			}
			return std::nullopt;
		} else { // parse everything else directly
			return value.get_value().parse<T>(managed_json_parser<T>(m));
		}
	}
}
