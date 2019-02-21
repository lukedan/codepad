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
#include "font.h"

namespace codepad::ui {
	/// The type of a element state.
	enum class element_state_type {
		/// The state is mostly caused directly by user input, is usually not configurable, and usually have no
		/// impact on the element's layout (metrics).
		passive,
		/// The state is configurable, is usually not directly caused by user input, and usually influences the
		/// element's layout (metrics).
		configuration
	};
	/// Information about an element state.
	struct element_state_info {
		/// Default constructor.
		element_state_info() = default;
		/// Initializes all fields of the struct.
		element_state_info(element_state_id stateid, element_state_type t) : id(stateid), type(t) {
		}

		element_state_id id = normal_element_state_id; ///< The state's ID.
		element_state_type type = element_state_type::passive; ///< The state's type.
	};

	/// Manages the update, layout, and rendering of all GUI elements, and the registration and retrieval of
	/// \ref element_state_id "element_state_ids" and transition functions.
	class manager {
		friend class window_base;
	public:
		/// Wrapper of an \ref element's constructor. The element's constructor takes a string that indicates the
		/// element's class.
		using element_constructor = std::function<element*()>;

		/// Universal element states that are defined natively.
		struct predefined_states {
			element_state_id
				/// Indicates that the cursor is positioned over the element.
				mouse_over,
				/// Indicates that the primary mouse button has been pressed, and the cursor is positioned over the
				/// element and not over any of its children.
				mouse_down,
				/// Indicates that the element has the focus.
				focused,
				/// Indicates either this element or a child of this element has the focus. The child is not
				/// necessarily a direct child.
				child_focused,
				/// Indicates that this element is selected.
				selected,
				/// Typically used by \ref decoration "decorations" to render the fading animation of a disposed
				/// element.
				corpse,

				// visibility-related states
				/// Indicates that the element is not rendered, but the user may still be able to interact with it.
				render_invisible,
				/// Indicates that the element is not visible when performing hit tests.
				hittest_invisible,
				/// Indicates that the element is not accounted for while calculating layout.
				layout_invisible,

				/// Indicates that the element is in a vertical position, or that its children are laid out
				/// vertically.
				vertical;
		};


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
		/// Constructs and returns an element of the specified type, class, and \ref element_metrics. If no such type
		/// exists, \p nullptr is returned. To properly dispose of the element, use \ref scheduler::mark_for_disposal().
		element *create_element_custom(str_view_t type, str_view_t cls, const element_metrics &metrics) {
			auto it = _ctor_map.find(type);
			if (it == _ctor_map.end()) {
				return nullptr;
			}
			element *elem = it->second(); // the constructor must not use element::_manager
			elem->_manager = this;
			elem->_initialize(cls, metrics);
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
				type, cls, get_class_arrangements().get_or_default(cls).metrics
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

		/// Registers an element state with the given name and type.
		///
		/// \return ID of the state, or \ref normal_element_state_id if a state with the same name already exists.
		element_state_id register_state_id(const str_t &name, element_state_type type) {
			element_state_id res = 1 << _stateid_alloc;
			if (_stateid_map.emplace(name, element_state_info(res, type)).second) {
				++_stateid_alloc;
				_statename_map[res] = name;
				return res;
			}
			return normal_element_state_id;
		}
		/// Returns the \ref element_state_info corresponding to the given name. The state must have been previously
		/// registered.
		element_state_info get_state_info(const str_t &name) const {
			auto found = _stateid_map.find(name);
			assert_true_usage(found != _stateid_map.end(), "element state not found");
			return found->second;
		}
		/// Returns all predefined states.
		///
		/// \sa predefined_states
		const predefined_states &get_predefined_states() const {
			return _predef_states;
		}

		/// Registers the given transition function.
		void register_transition_function(str_t name, transition_function func) {
			auto[it, inserted] = _transfunc_map.emplace(std::move(name), std::move(func));
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

		/// Sets the renderer used for all managed elements. Normally this only needs to be called once during
		/// program startup.
		void set_renderer(std::unique_ptr<renderer_base> r) {
			_renderer = std::move(r);
		}
		/// Returns the current renderer. \ref _renderer must have been set with \ref set_renderer().
		renderer_base &get_renderer() {
			return *_renderer;
		}

		/// Sets the \ref font_manager used by this manager.
		void set_font_manager(std::unique_ptr<font_manager> man) {
			_font_manager = std::move(man);
			_default_ui_font = _font_manager->get_font(font_manager::get_default_ui_font_parameters());
		}
		/// Returns the font manager.
		font_manager &get_font_manager() {
			return *_font_manager;
		}

		/// Returns the registry of \ref class_visual "class_visuals" corresponding to all element classes.
		class_visuals_registry &get_class_visuals() {
			return _cvis;
		}
		/// Const version of \ref get_class_visuals().
		const class_visuals_registry &get_class_visuals() const {
			return _cvis;
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

		/// Returns the default UI \ref font.
		const std::shared_ptr<const font> &get_default_ui_font() const {
			return _default_ui_font;
		}
	protected:
		class_visuals_registry _cvis; ///< All visuals.
		class_arrangements_registry _carngs; ///< All arrangements.
		class_hotkeys_registry _chks; ///< All hotkeys.
		command_registry _commands; ///< All commands.
		/// Registry of constructors of all element types.
		std::map<str_t, element_constructor, std::less<>> _ctor_map;
		/// Mapping from names to transition functions.
		std::map<str_t, transition_function, std::less<>> _transfunc_map;
		/// Mapping from state names to state IDs.
		std::map<str_t, element_state_info> _stateid_map;
		/// Mapping from state IDs to state names.
		std::map<element_state_id, str_t> _statename_map;
		predefined_states _predef_states; ///< Predefined states.
		scheduler _scheduler; ///< The \ref scheduler.
		std::unique_ptr<renderer_base> _renderer; ///< The renderer.
		std::unique_ptr<font_manager> _font_manager; ///< The font manager.
		std::shared_ptr<const font> _default_ui_font; ///< The default font for the user interface.
		size_t _stateid_alloc = 0; ///< Records how many states have been registered.
	};
}
