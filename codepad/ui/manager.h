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
		friend class window_base;
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
				logger::get().log_warning(CP_HERE) << "duplicate transition function name: " << name;
			}
		}
		/// Finds and returns the transition function corresponding to the given name. If none is found, \p nullptr
		/// is returned.
		transition_function find_transition_function(str_view_t name) const {
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
		/// Returns the registry of \ref class_hotkey_group "element_hotkey_groups" corresponding to all element
		/// classes.
		class_hotkeys_registry &get_class_hotkeys() {
			return _class_hotkeys;
		}
		/// Const version of \ref get_class_hotkeys().
		const class_hotkeys_registry &get_class_hotkeys() const {
			return _class_hotkeys;
		}

		/// Finds the color that corresponds to the given name in the color scheme.
		std::optional<colord> find_color(str_view_t name) const {
			if (auto it = _color_scheme.find(name); it != _color_scheme.end()) {
				return it->second;
			}
			return std::nullopt;
		}
		/// Returns the current color scheme.
		const std::map<str_t, colord, std::less<>> &get_color_scheme() const {
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


		/// Returns the radius of the zone where a drag operation is ready but does not start. This function is
		/// platform-specific.
		static double get_drag_deadzone_radius();
	protected:
		class_arrangements_registry _class_arrangements; ///< All class arrangements.
		class_hotkeys_registry _class_hotkeys; ///< All class hotkeys.
		std::map<str_t, colord, std::less<>> _color_scheme; ///< The color scheme.

		command_registry _commands; ///< All commands.
		/// Registry of constructors of all element types.
		std::map<str_t, element_constructor, std::less<>> _ctor_map;
		/// Mapping from names to transition functions.
		std::map<str_t, transition_function, std::less<>> _transfunc_map;

		/// The mapping between file names and textures.
		std::map<std::filesystem::path, std::shared_ptr<bitmap>> _textures; // TODO resource path

		scheduler _scheduler; ///< The \ref scheduler.
		std::unique_ptr<renderer_base> _renderer; ///< The renderer.
		settings &_settings; ///< The settings associated with this \ref manager.
	};


	template <typename T> inline bool typed_animation_value_parser<T>::try_parse( // TODO use std::optional
		const json::value_storage &value, [[maybe_unused]] manager &m, T &res
	) const {
		if constexpr (std::is_same_v<T, std::shared_ptr<bitmap>>) { // load textures
			if (auto path = value.get_value().cast<str_view_t>()) {
				res = m.get_texture(path.value());
				return res != nullptr;
			}
			return false;
		} else { // parse everything else directly
			if (auto v = value.get_value().parse<T>()) {
				res = std::move(v.value());
				return true;
			}
			return false;
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
				logger::get().log_warning(CP_HERE) << "failed to parse keyframe target";
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


	template <
		typename Value
	> std::optional<brushes::bitmap_pattern> managed_json_parser<brushes::bitmap_pattern>::operator()(
		const Value &val
		) const {
		if (auto obj = val.template cast<typename Value::object_type>()) {
			if (auto image = obj->template parse_member<str_view_t>(u8"image")) {
				return brushes::bitmap_pattern(_manager.get_texture(image.value()));
			}
		}
		return std::nullopt;
	}


	template <
		typename Value
	> std::optional<transition_function> managed_json_parser<transition_function>::operator()(
		const Value &val
		) const {
		if (auto str = val.template cast<str_view_t>()) {
			if (auto func = _manager.find_transition_function(str.value())) {
				return func;
			}
			val.template log<log_level::error>(CP_HERE) << "unrecognized transition function: " << str.value();
		}
		return std::nullopt;
	}
}
