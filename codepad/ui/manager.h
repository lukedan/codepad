#pragma once

/// \file manager.h
/// Manager of all GUI elements.

#include <map>
#include <set>
#include <deque>
#include <chrono>

#include "element.h"

namespace codepad::ui {
	/// Manages the update, layout, and rendering all GUI elements.
	class manager {
	public:
		constexpr static double
			/// Maximum expected time for all layout operations during a single frame.
			relayout_time_redline = 0.01,
			/// Maximum expected time for all rendering operations during a single frame.
			render_time_redline = 0.04;

		/// Destructor. Calls \ref dispose_marked_elements.
		~manager() {
			dispose_marked_elements();
		}

		/// Invalidates the layout of an element. If layout is in progress,
		/// this element is appended to the queue recording all elements whose layout are to be updated.
		/// Otherwise it's marked for layout calculation, which will take place during the next frame.
		void invalidate_layout(element &e) {
			if (_layouting) {
				_q.emplace_back(&e, true);
			} else {
				_targets[&e] = true;
			}
		}
		/// Marks the element for layout validation, meaning that its layout is valid but element::_finish_layout
		/// has not been called. Like \ref invalidate_layout, different operation will be performed depending on
		/// whether layout is in progress.
		void revalidate_layout(element &e) {
			if (_layouting) {
				_q.emplace_back(&e, false);
			} else {
				if (_targets.find(&e) == _targets.end()) {
					// otherwise invalidate_layout may have been called on the element
					_targets.insert({&e, false});
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

		/// Returns the element that has the focus.
		element *get_focused() const {
			return _focus;
		}
		/// Sets the currently focused element. The element must either be \p nullptr or belong to a window.
		/// This function should not be called recursively.
		void set_focus(element*);

		/// Returns the global \ref manager object.
		static manager &get();
	protected:
		std::map<element*, bool> _targets; ///< Stores the elements whose layout need updating.
		/// Stores the elements whose layout need updating during the calculation of layout.
		std::deque<std::pair<element*, bool>> _q;
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
		element *_focus = nullptr; ///< Pointer to the currently focused element.
	};
}
