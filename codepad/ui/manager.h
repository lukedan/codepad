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
#include "../os/window.h"

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
		friend class os::window_base;
	public:
		/// Wrapper of an \ref element's constructor. The element's constructor takes a string that indicates the
		/// element's class.
		using element_constructor = std::function<element*()>;

		/// Universal element states that are defined natively.
		struct predefined_states {
			element_state_id
				/// Indicates that the cursor is positioned over the element.
				mouse_over,
				/// Indicates that the primary mouse button has been pressed, and the cursor
				/// is positioned over the element and not over any of its children.
				mouse_down,
				/// Indicates that the element has the focus.
				focused,
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

		constexpr static double
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline = 0.01,
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline = 0.04;


		/// Constructor, registers predefined element states, transition functions, and element types.
		manager();
		/// Destructor. Calls \ref dispose_marked_elements.
		~manager() {
			dispose_marked_elements();
		}

		/// Invalidates the layout of an element. Its parent will be notified to recalculate its layout.
		void invalidate_layout(element&);
		/// Invalidates the layout of all children of a \ref panel_base.
		void invalidate_children_layout(panel_base&);
		/// Marks the element for layout validation, meaning that its layout is valid but
		/// \ref element::_on_layout_changed() has not been called. Like \ref invalidate_layout, different operation
		/// will be performed depending on whether layout is in progress.
		void notify_layout_change(element &e) {
			assert_true_logical(!_layouting, "layout notifications are handled automatically");
			_layout_notify.emplace(&e);
		}
		/// Calculates the layout of all elements with invalidated layout.
		/// The calculation is recursive; that is, after a parent's layout has been changed,
		/// all its children are automatically marked for layout calculation.
		void update_invalid_layout();

		/// Marks the given element for re-rendering. This will re-render the whole window,
		/// but even if the visual of multiple elements in the window is invalidated,
		/// the window is still rendered once.
		void invalidate_visual(element &e) {
			_dirty.insert(&e);
		}
		/// Re-renders the windows that contain elements whose visuals are invalidated.
		void update_invalid_visuals();
		/// Immediately re-render the window containing the given element.
		void update_visual_immediate(element&);

		/// Schedules the given element's visual configurations to be updated.
		void schedule_visual_config_update(element &elem) {
			_visualcfg_update.emplace(&elem);
		}
		/// Schedules the given element's \ref metrics_configuration to be updated.
		void schedule_metrics_config_update(element &elem) {
			if (!elem._config.metrics_config.get_state().all_stationary) {
				_metricscfg_update.emplace(&elem);
			}
		}
		/// Schedules the given element to be updated next frame.
		///
		/// \todo Remove this.
		void schedule_update(element &e) {
			_upd.insert(&e);
		}
		/// Updates all elements that are scheduled to be updated by \ref schedule_visual_config_update(),
		/// \ref schedule_metrics_config_update(), and \ref schedule_update().
		void update_scheduled_elements();
		/// Returns the amount of time that has passed since the last
		/// time \ref update_scheduled_elements has been called, in seconds.
		double update_delta_time() const {
			return _upd_dt;
		}

		/// Similar to \ref register_element_type(const str_t&, element_constructor) but for built-in classes.
		template <typename Elem> void register_element_type() {
			register_element_type(Elem::get_default_class(), []() {
				return new Elem();
			});
		}
		/// Registers a new element type for creation.
		void register_element_type(const str_t &type, element_constructor constructor) {
			_ctor_map.emplace(type, std::move(constructor));
		}
		/// Constructs and returns an element of the specified type, class, and \ref element_metrics. If no such type
		/// exists, \p nullptr is returned.
		element *create_element_custom(const str_t &type, const str_t &cls, const element_metrics &metrics) const {
			auto it = _ctor_map.find(type);
			if (it == _ctor_map.end()) {
				return nullptr;
			}
			element *elem = it->second();
			elem->_initialize(cls, metrics);
#ifdef CP_CHECK_USAGE_ERRORS
			assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
#endif
			return elem;
		}
		/// Calls \ref create_element_custom to create an \ref element of the specified type and class, but with
		/// the default metrics of that class.
		element *create_element(const str_t &type, const str_t &cls) {
			return create_element_custom(
				type, cls, get_class_arrangements().get_arrangements_or_default(cls).metrics
			);
		}
		/// Creates an element of the given type. The type name and class are both obtained from
		/// \p Elem::get_default_class().
		///
		/// \todo Wait for when reflection is in C++ to replace get_default_class().
		template <typename Elem> Elem *create_element() {
			element *elem = create_element(Elem::get_default_class(), Elem::get_default_class());
			Elem *res = dynamic_cast<Elem*>(elem);
			assert_true_logical(res, "incorrect get_default_class() method");
			return res;
		}
		/// Marks the given element for disposal. The element is only disposed when \ref dispose_marked_elements
		/// is called. It is safe to call this multiple times before the element's actually disposed.
		void mark_disposal(element &e) {
			_del.insert(&e);
		}
		/// Disposes all elements that has been marked for disposal. Other elements that are not marked
		/// previously but are marked for disposal during the process are also disposed.
		void dispose_marked_elements();

		/// Simply calls \ref update_invalid_layout and \ref update_invalid_visuals.
		void update_layout_and_visual() {
			update_invalid_layout();
			update_invalid_visuals();
		}
		/// Simply calls \ref dispose_marked_elements, \ref update_scheduled_elements,
		/// and \ref update_layout_and_visual.
		void update() {
			performance_monitor mon(CP_STRLIT("Update UI"));
			dispose_marked_elements();
			update_scheduled_elements();
			update_layout_and_visual();
		}

		/// Returns the current minimum rendering interval that indicates how long it must be
		/// between two consecutive re-renders.
		double get_minimum_rendering_interval() const {
			return _min_render_interval;
		}
		/// Sets the minimum rendering interval.
		void set_minimum_rendering_interval(double dv) {
			_min_render_interval = dv;
		}

		/// Returns the \ref os::window_base that has the focus.
		os::window_base *get_focused_window() const {
			return _focus_wnd;
		}
		/// Returns the \ref element that has the focus, or \p nullptr.
		element *get_focused_element() const {
			if (_focus_wnd != nullptr) {
				return _focus_wnd->get_window_focused_element();
			}
			return nullptr;
		}
		/// Sets the currently focused element. When called, this function also interrupts any ongoing composition.
		/// The element must belong to a window. This function should not be called recursively.
		void set_focused_element(element&);

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
		/// Finds and returns the transition function corresponding to the given name. If none is found, \p nullptr
		/// is returned.
		///
		/// \todo Implement ways to register transition functions.
		transition_function try_get_transition_func(const str_t &name) const {
			auto it = _transfunc_map.find(name);
			if (it != _transfunc_map.end()) {
				return it->second;
			}
			return nullptr;
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


		/// Returns the global \ref manager object.
		static manager &get();
	protected:
		/// Stores the elements whose \ref element::_on_layout_changed() need to be called.
		std::set<element*> _layout_notify;
		/// Stores the panels whose children's layout need computing.
		std::set<panel_base*> _children_layout_scheduled;

		/// Stores the set of elements whose \ref element::_on_update_visual_configurations() are to be called.
		std::set<element*> _visualcfg_update;
		/// Stores the set of elements whose \ref metrics_configuration need updating.
		std::set<element*> _metricscfg_update;

		std::set<element*> _dirty; ///< Stores all elements whose visuals need updating.
		std::set<element*> _del; ///< Stores all elements that are to be disposed of.
		std::set<element*> _upd; ///< Stores all elements that are to be updated.
		/// The time point when elements were last rendered.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastrender;
		/// The time point when elements were last updated.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastupdate;
		double _min_render_interval = 0.0; ///< The minimum interval between consecutive re-renders.
		double _upd_dt = 0.0; ///< The duration since elements were last updated.
		os::window_base *_focus_wnd = nullptr; ///< Pointer to the currently focused \ref os::window_base.
		bool _layouting = false; ///< Specifies whether layout calculation is underway.

		class_visuals_registry _cvis; ///< All visuals.
		class_arrangements_registry _carngs; ///< All arrangements.
		class_hotkeys_registry _chks; ///< All hotkeys.
		/// Registry of constructors of all element types.
		std::map<str_t, element_constructor> _ctor_map;
		/// Mapping from names to transition functions.
		std::map<str_t, transition_function> _transfunc_map;
		/// Mapping from state names to state IDs.
		std::map<str_t, element_state_info> _stateid_map;
		/// Mapping from state IDs to state names.
		std::map<element_state_id, str_t> _statename_map;
		predefined_states _predef_states; ///< Predefined states.
		size_t _stateid_alloc = 0; ///< Records how many states have been registered.


		/// Called when a \ref os::window_base is focused. Sets \ref _focus_wnd accordingly.
		void _on_window_got_focus(os::window_base &wnd) {
			_focus_wnd = &wnd;
		}
		/// Called when a \ref os::window_base loses the focus. Clears \ref _focus_wnd if necessary.
		void _on_window_lost_focus(os::window_base &wnd) {
			if (_focus_wnd == &wnd) {
				_focus_wnd = nullptr;
			}
		}
	};


	template <typename T> inline void ui_config_parser::parse_animation(
		animated_property<T> &ani, const json::value_t &obj
	) {
		if (obj.IsObject()) {
			json::value_t::ConstMemberIterator mem;
			mem = obj.FindMember(CP_STRLIT("to"));
			if (mem != obj.MemberEnd()) {
				ani.to = json_object_parsers::parse<T>(mem->value);
			} else {
				logger::get().log_warning(CP_HERE, "no \"to\" property found in animation");
			}
			mem = obj.FindMember(CP_STRLIT("from"));
			if (mem != obj.MemberEnd()) {
				ani.has_from = true;
				ani.from = json_object_parsers::parse<T>(mem->value);
			} else {
				ani.has_from = false;
			}
			json::try_get(obj, CP_STRLIT("has_from"), ani.has_from);
			json::try_get(obj, CP_STRLIT("auto_reverse"), ani.auto_reverse);
			json::try_get(obj, CP_STRLIT("repeat"), ani.repeat);
			json::try_get(obj, CP_STRLIT("duration"), ani.duration);
			json::try_get(obj, CP_STRLIT("reverse_duration_scale"), ani.reverse_duration_scale);
			mem = obj.FindMember(CP_STRLIT("transition"));
			if (mem != obj.MemberEnd()) {
				if (mem->value.IsString()) {
					transition_function
						fptr = manager::get().try_get_transition_func(json::get_as_string(mem->value));
					if (fptr == nullptr) {
						ani.transition_func = transition_functions::linear;
						logger::get().log_warning(
							CP_HERE, "invalid transition function: ", json::get_as_string(mem->value)
						);
					} else {
						ani.transition_func = fptr;
					}
				} else {
					logger::get().log_warning(CP_HERE, "invalid transition function");
				}
			}
		} else {
			ani = animated_property<T>();
			ani.to = json_object_parsers::parse<T>(obj);
		}
	}
}
