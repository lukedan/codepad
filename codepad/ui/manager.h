#pragma once

/// \file
/// Manager of all GUI elements.

#include <map>
#include <set>
#include <queue>
#include <chrono>

#include "element.h"
#include "../os/window.h"

namespace codepad::ui {
	/// Manages the update, layout, and rendering of all GUI elements, and the registration and retrieval of
	/// \ref element_state_id "element_state_ids" and transition functions.
	class manager {
		friend class os::window_base;
	public:
		/// Universal element states that are defined natively.
		struct predefined_states {
			element_state_id
				/// Indicates that the element is not visible, but the user may still be able to interact with it.
				invisible,
				ghost, ///< Indicates that user cannot interact with the element, whether it is visible or not.
				mouse_over, ///< Indicates that the cursor is positioned over the element.
				/// Indicates that the primary mouse button has been pressed, and the cursor
				/// is positioned over the element and not over any of its children.
				mouse_down,
				focused, ///< Indicates that the element has the focus.
				/// Typically used by \ref decoration "decorations" to render the fading animation of a disposed
				/// element.
				corpse;
		};

		constexpr static double
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline = 0.01,
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline = 0.04;


		/// Constructor, registers predefined element states and transition functions.
		manager() {
#ifdef CP_CHECK_LOGICAL_ERRORS
			// initialize control_disposal_rec first because it needs to be disposed later than manager
			control_disposal_rec::get();
#endif

			_predef_states.invisible = get_or_register_state_id(CP_STRLIT("invisible"));
			_predef_states.ghost = get_or_register_state_id(CP_STRLIT("ghost"));
			_predef_states.mouse_over = get_or_register_state_id(CP_STRLIT("mouse_over"));
			_predef_states.mouse_down = get_or_register_state_id(CP_STRLIT("mouse_down"));
			_predef_states.focused = get_or_register_state_id(CP_STRLIT("focused"));
			_predef_states.corpse = get_or_register_state_id(CP_STRLIT("corpse"));

			_transfunc_map.emplace(CP_STRLIT("linear"), transition_functions::linear);
			_transfunc_map.emplace(CP_STRLIT("smoothstep"), transition_functions::smoothstep);
			_transfunc_map.emplace(
				CP_STRLIT("concave_quadratic"), transition_functions::concave_quadratic
			);
			_transfunc_map.emplace(
				CP_STRLIT("convex_quadratic"), transition_functions::convex_quadratic
			);
			_transfunc_map.emplace(CP_STRLIT("concave_cubic"), transition_functions::concave_cubic);
			_transfunc_map.emplace(CP_STRLIT("convex_cubic"), transition_functions::convex_cubic);
		}
		/// Destructor. Calls \ref dispose_marked_elements.
		~manager() {
			dispose_marked_elements();
		}

		/// Invalidates the layout of an element. If layout is in progress, this element is appended to the queue
		/// recording all elements whose layout are to be updated. Otherwise it's marked for layout calculation,
		/// which will take place during the next frame.
		void invalidate_layout(element&);
		/// Marks the element for layout validation, meaning that its layout is valid but element::_finish_layout
		/// has not been called. Like \ref invalidate_layout, different operation will be performed depending on
		/// whether layout is in progress.
		void revalidate_layout(element &e) {
			if (_layouting) {
				_q.emplace(&e, false);
			} else {
				if (_targets.find(&e) == _targets.end()) {
					// otherwise invalidate_layout may have been called on the element
					_targets.emplace(&e, false);
				}
			}
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

		/// Schedules the given element to be updated next frame.
		void schedule_update(element &e) {
			_upd.insert(&e);
		}
		/// Calls element::_on_update on all elements that have been scheduled to be updated.
		void update_scheduled_elements();
		/// Returns the amount of time that has passed since the last
		/// time \ref update_scheduled_elements has been called, in seconds.
		double update_delta_time() const {
			return _upd_dt;
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


		/// Returns the state ID corresponding to the given name. If none is found,
		/// it is registered as a new state and the allocated ID is returned.
		element_state_id get_or_register_state_id(const str_t &name) {
			auto found = _stateid_map.find(name);
			if (found == _stateid_map.end()) {
				logger::get().log_info(CP_HERE, "registering state: ", convert_to_utf8(name));
				element_state_id res = 1 << _stateid_alloc;
				++_stateid_alloc;
				_stateid_map[name] = res;
				_statename_map[res] = name;
				return res;
			}
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
		const std::function<double(double)> *try_get_transition_func(const str_t &name) const {
			auto it = _transfunc_map.find(name);
			if (it != _transfunc_map.end()) {
				return &it->second;
			}
			return nullptr;
		}

		/// Returns the registry of \ref visual "visuals" corresponding to all element classes.
		class_visuals &get_class_visuals() {
			return _cvis;
		}
		/// Returns the registry of \ref element_hotkey_group "element_hotkey_groups" corresponding to all element classes.
		class_hotkeys &get_class_hotkeys() {
			return _chks;
		}


		/// Returns the global \ref manager object.
		static manager &get();
	protected:
		std::map<element*, bool> _targets; ///< Stores the elements whose layout need updating.
		/// Stores the elements whose layout need updating during the calculation of layout.
		std::queue<std::pair<element*, bool>> _q;
		bool _layouting = false; ///< Specifies whether layout calculation is underway.
		std::set<element*> _dirty; ///< Stores all elements whose visuals need updating.
		/// The time point when elements were last rendered.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastrender;
		double _min_render_interval = 0.0; ///< The minimum interval between consecutive re-renders.
		std::set<element*> _del; ///< Stores all elements that are to be disposed of.
		std::set<element*> _upd; ///< Stores all elements that are to be updated.
		/// The time point when elements were last updated.
		std::chrono::time_point<std::chrono::high_resolution_clock> _lastupdate;
		double _upd_dt = 0.0; ///< The duration since elements were last updated.
		os::window_base *_focus_wnd = nullptr; ///< Pointer to the currently focused \ref os::window_base.

		/// Mapping from names to transition functions.
		std::map<str_t, std::function<double(double)>> _transfunc_map;
		/// Mapping from state names to state IDs.
		std::map<str_t, element_state_id> _stateid_map;
		/// Mapping from state IDs to state names.
		///
		/// \todo Not sure if this is useful.
		std::map<element_state_id, str_t> _statename_map;
		predefined_states _predef_states; ///< Predefined states.
		int _stateid_alloc = 0; ///< Records how many states have been registered.
		class_visuals _cvis; ///< All visuals.
		class_hotkeys _chks; ///< All hotkeys.


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
}
