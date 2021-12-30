// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager of all GUI elements.

#include <map>
#include <set>
#include <chrono>
#include <functional>

#include "codepad/apigen_definitions.h"
#include "codepad/core/settings.h"
#include "async_task.h"
#include "element.h"
#include "element_parameters.h"
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

		/// Creates a new element tree specified by the given element type and class. The creation process consists
		/// of the following steps:
		/// - All elements in the tree are created:
		///   - The element type is found in \ref _element_registry, and used to create a concrete object.
		///   - Some fields are set: \ref element::_hotkeys and \ref element::_manager.
		///   - \ref element::_initialize() is invoked.
		///   - The element is added to its parent in the subtree.
		/// - Element references are handled. This is done in no particular order for all created elements.
		/// - Event handles are registered. This is done in no particular order for all created elements.
		/// - Element attributes are set. This is done in no particular order for all created elements.
		element *create_element(std::u8string_view type, std::u8string_view cls) {
			_construction_context ctx(*this);
			return ctx.create_tree(type, cls);
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
				logger::get().log_warning() << "duplicate transition function name: " << name;
			}
		}
		/// Finds and returns the transition function corresponding to the given name. If none is found, \p nullptr
		/// is returned.
		[[nodiscard]] transition_function find_transition_function(std::u8string_view name) const {
			auto it = _transfunc_map.find(name);
			if (it != _transfunc_map.end()) {
				return it->second;
			}
			return nullptr;
		}

		/// Returns the texture at the given path, loading it from disk when necessary.
		[[nodiscard]] std::shared_ptr<bitmap> get_texture(const std::filesystem::path &path) {
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
		[[nodiscard]] class_arrangements_registry &get_class_arrangements() {
			return _class_arrangements;
		}
		/// Const version of \ref get_class_arrangements().
		[[nodiscard]] const class_arrangements_registry &get_class_arrangements() const {
			return _class_arrangements;
		}
		/// Returns the registry of \ref hotkey_group "hotkey_groups" corresponding to all element
		/// classes.
		[[nodiscard]] class_hotkeys_registry &get_class_hotkeys() {
			return _class_hotkeys;
		}
		/// Const version of \ref get_class_hotkeys().
		[[nodiscard]] const class_hotkeys_registry &get_class_hotkeys() const {
			return _class_hotkeys;
		}

		/// Finds the color that corresponds to the given name in the color scheme.
		[[nodiscard]] std::optional<colord> find_color(std::u8string_view name) const {
			if (auto it = _color_scheme.find(name); it != _color_scheme.end()) {
				return it->second;
			}
			return std::nullopt;
		}
		/// Returns the current color scheme.
		[[nodiscard]] const std::unordered_map<
			std::u8string, colord, string_hash<>, std::equal_to<>
		> &get_color_scheme() const {
			return _color_scheme;
		}

		/// Returns the \ref scheduler.
		[[nodiscard]] scheduler &get_scheduler() {
			return _scheduler;
		}
		/// \overload
		[[nodiscard]] const scheduler &get_scheduler() const {
			return _scheduler;
		}

		/// Returns the \ref async_task_scheduler.
		[[nodiscard]] async_task_scheduler &get_async_task_scheduler() {
			return _async_tasks;
		}
		/// \overload
		[[nodiscard]] const async_task_scheduler &get_async_task_scheduler() const {
			return _async_tasks;
		}

		/// Returns the registry of all commands used with hotkeys.
		[[nodiscard]] command_registry &get_command_registry() {
			return _commands;
		}
		/// \overload
		[[nodiscard]] const command_registry &get_command_registry() const {
			return _commands;
		}

		/// Returns the \ref settings object associated with this manager.
		[[nodiscard]] settings &get_settings() const {
			return _settings;
		}

		/// Reloadss all visual configuration, including arrangements and the color scheme.
		template <typename ValueType> void reload_visuals(const ValueType &value) {
			_color_scheme.clear();
			_class_arrangements.mapping.clear();

			arrangements_parser<ValueType> parser(*this);
		}
	protected:
		/// Context for element tree construction.
		struct _construction_context {
		public:
			/// Initializes \ref _manager.
			explicit _construction_context(manager &man) : _manager(man) {
			}

			/// Creates elements in the entire subtree. This is the first step described in \ref create_element().
			[[nodiscard]] element *create_tree(std::u8string_view type, std::u8string_view cls);
		protected:
			/// Data associated with an element's creation.
			struct _element_data {
				/// Default constructor.
				_element_data() = default;
				/// Initializes all fields of this struct.
				_element_data(
					element &e, element &root, const element_configuration *cls, const element_configuration &custom
				) : elem(&e), custom_subtree_root(&root), class_config(cls), custom_config(&custom) {
				}

				element
					*elem = nullptr, ///< Pointer to the element.
					*custom_subtree_root = nullptr; ///< Root element of the custom subtree, used for element lookup.
				const element_configuration
					*class_config = nullptr, ///< The configuration of the element based on its class.
					*custom_config = nullptr; ///< The additional custom configuration of the element as a child.
			};
			/// Mapping between element names and created elements.
			using _name_mapping = std::map<std::u8string, element*, std::less<>>;

			std::vector<_element_data> _created; ///< The list of all created elements.
			/// Scopes for element lookup. Each scope corresponds to one class in the class registry.
			std::map<std::uintptr_t, _name_mapping> _scopes;
			manager &_manager; ///< The \ref manager that is creating the element tree.

			/// Creates a single element, completing step 1 except for adding it to its parent.
			[[nodiscard]] element *_create_element(std::u8string_view type, std::u8string_view cls);
			/// Recursively creates children elements using \ref _create_element and adds them to the given
			/// \ref panel. All created elements (not including the given root) will be registered to the given
			/// scope.
			void _create_children_recursive(
				panel &current, const std::vector<class_arrangements::child>&,
				element &custom_subtree_root, _name_mapping&
			);

			/// Finds the target element specified by the name.
			[[nodiscard]] element *_find_element_by_name(element &root, const std::vector<std::u8string>&) const;
			/// Registers an trigger on the \p trigger element that will affect \p affected.
			static void _register_trigger_for(
				element &trigger, element &affected, const element_configuration::event_trigger&
			);

			/// Handles the given list of references.
			void _handle_references(
				element&, element &root, const std::vector<element_configuration::reference>&
			) const;
			/// Handles the given list of event triggers.
			void _handle_event_triggers(
				element&, element &roots, const std::vector<element_configuration::event_trigger>&
			) const;
			/// Handles the given list of properties.
			static void _handle_properties(element&, const std::vector<element_configuration::property_value>&);
		};

		class_arrangements_registry _class_arrangements; ///< All class arrangements.
		class_hotkeys_registry _class_hotkeys; ///< All class hotkeys.
		/// The color scheme used by class arrangements.
		std::unordered_map<std::u8string, colord, string_hash<>, std::equal_to<>> _color_scheme;

		command_registry _commands; ///< All commands.
		/// Registry of constructors of all element types.
		std::unordered_map<std::u8string, element_constructor, string_hash<>, std::equal_to<>> _element_registry;
		/// Mapping from names to transition functions.
		std::unordered_map<std::u8string, transition_function, string_hash<>, std::equal_to<>> _transfunc_map;

		/// The mapping between file names and textures.
		std::unordered_map<std::filesystem::path, std::shared_ptr<bitmap>> _textures; // TODO resource path

		scheduler _scheduler; ///< The \ref scheduler.
		async_task_scheduler _async_tasks; ///< The \ref async_task_scheduler.
		std::unique_ptr<renderer_base> _renderer; ///< The renderer.
		settings &_settings; ///< The settings associated with this \ref manager.
	};


	template <typename T, typename Lerp> inline std::shared_ptr<
		animation_definition_base
	> typed_animation_value_handler<T, Lerp>::parse_keyframe_animation(
		const generic_keyframe_animation_definition &def
	) const {
		using animation = keyframe_animation_definition<T>;
		std::vector<typename animation::keyframe> keyframes;
		for (auto &&kf : def.keyframes) {
			if (auto target = parse(kf.target)) {
				keyframes.emplace_back(target.value(), kf.duration, kf.transition_func);
			} else {
				logger::get().log_error() << "failed to parse keyframe target";
			}
		}
		return std::make_shared<animation>(std::move(keyframes), *this, def.repeat_times);
	}


	template <
		typename T, typename Lerp
	> inline std::optional<T> managed_animation_value_handler<T, Lerp>::parse_static(
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
