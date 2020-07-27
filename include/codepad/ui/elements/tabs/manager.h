// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager for tabs and tab hosts.

#include "../../../os/misc.h"
#include "../../../os/current/window.h"
#include "../../element.h"
#include "../../panel.h"
#include "split_panel.h"
#include "tab.h"
#include "host.h"

namespace codepad::ui::tabs {
	/// Information about the user dragging a \ref tab_button.
	struct tab_drag_update_info {
		/// Initializes all fields of this struct.
		explicit tab_drag_update_info(vec2d pos) : position(pos) {
		}
		const vec2d position; /// New position of the top-left corner of the \ref tab_button.
	};
	/// Contains related information when the user stops dragging a tab.
	struct tab_drag_ended_info {
		/// Initializes \ref dragging_tab.
		explicit tab_drag_ended_info(tab *t) : dragging_tab(t) {
		}
		const tab *dragging_tab; ///< The tab that the user was dragging.
	};

	/// Manages all \ref tab "tabs" and \ref host "tab_hosts".
	class tab_manager {
		friend tab;
		friend host;
	public:
		/// Constructor. Initializes \ref _drag_dest_selector and update tasks.
		tab_manager(ui::manager &man) : _manager(man) {
		}
		/// Disposes \ref _drag_dest_selector, and unregisters update tasks.
		~tab_manager() {
			assert_true_logical(
				_drag == nullptr, "dragging operation still in progress during tab manager disposal"
			);
		}

		/// Creates a new \ref tab in a \ref host in the last focused \ref window_base. If there are no windows,
		/// a new one is created.
		tab *new_tab();
		/// Creates a new \ref tab in the given \ref host and returns it. If the given \ref host is
		/// \p nullptr, a new window containing a new \ref host will be created, in which the \ref tab will be
		/// created.
		tab *new_tab_in(host* = nullptr);

		/// Returns the total number of windows managed by \ref tab_manager.
		std::size_t window_count() const {
			return _wndlist.size();
		}

		/// Returns \p true if there are no more \ref tab instances.
		bool empty() const {
			return window_count() == 0 && _drag == nullptr;
		}

		/// Splits the \ref host the given \ref tab is in into two \ref host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref host.
		///
		/// \param t The \ref tab.
		/// \param orient The orientation in which this \ref host should split.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref host while other remaining
		///                 tabs will be put in the bottom/right \ref host.
		void split_tab(tab &t, orientation orient, bool newfirst) {
			host *host = t.get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tab(*host, t, orient, newfirst);
		}
		/// Creates a new \ref os::window and a \ref host and moves the given tab into the newly created
		/// \ref host. The size of the tab will be kept unchanged.
		void move_tab_to_new_window(tab&);

		/// Updates all \ref host "tab_hosts" whose tabs have been closed or moved. This is mainly intended to
		/// automatically merge empty tab hosts when they are emptied.
		void update_changed_hosts();

		/// Returns \p true if the user's currently dragging a \ref tab.
		bool is_dragging_tab() const {
			return _drag != nullptr;
		}
		/// Returns the tab that's currently being dragged.
		tab *get_dragging_tab() const {
			return _drag;
		}

		/// Starts dragging a given \ref tab.
		///
		/// \param t The tab to be dragged.
		/// \param diff The offset from the top left corner of the \ref tab_button to the mouse cursor.
		/// \param layout The layout of the \ref tab's main region.
		void start_dragging_tab(tab &t, vec2d diff, rectd layout);

		info_event<tab_drag_ended_info> drag_ended; ///< Invoked when the user finishes dragging a \ref tab_button.
		/// Invoked while the user is dragging a \ref tab_button.
		info_event<tab_drag_update_info> drag_move_tab_button;
	protected:
		std::set<host*> _changed; ///< The set of \ref host "tab_hosts" whose children have changed.
		std::list<window_base*> _wndlist; ///< The list of windows, ordered according to their z-indices.

		// drag destination
		tab *_drag = nullptr; ///< The \ref tab that's currently being dragged.
		/// The destination \ref host of the \ref tab that's currently being dragged.
		host *_drag_destination = nullptr;
		/// \p true if the tab is being dragged in the tab button area of a \ref host.
		bool _dragging_in_host = false;
		// drag events & update
		/// Used when the tab is being dragged in a tab button region to unregister for the event when the tab should
		/// be detached.
		info_event<mouse_move_info>::token _mouse_move_token;
		info_event<mouse_button_info>::token _stop_drag_token; ///< Used to know when to stop dragging.
		info_event<>::token _capture_lost_token; ///< Used to listen to capture lost events and stop dragging.
		// drag ui
		window_base *_drag_tab_window = nullptr; ///< The window used to display the tab that's being dragged.
		drag_destination_selector *_drag_dest_selector = nullptr; ///< The \ref drag_destination_selector.
		// drag parameters
		/// The offset from the top left corner of the \ref tab_button to the mouse cursor.
		///
		/// \sa tab_button::_drag_pos
		vec2d _drag_offset;
		rectd _dragrect; ///< The boundaries of the main panel of \ref _drag, relative to the mouse cursor.

		ui::manager &_manager; ///< The \ref ui::manager that manages all tabs.


		/// Creates a new window and registers necessary event handlers.
		window_base *_new_window();
		/// Deletes the given window managed by this \ref tab_manager. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_window(window_base&);
		/// Creates a new \ref tab instance not attached to any \ref host. Use this instead of
		/// \ref ui::manager::create_element<tab>() so that the \ref tab is correctly registered to this manager.
		tab *_new_detached_tab() {
			tab *t = _manager.create_element<tab>();
			t->_tab_manager = this;
			return t;
		}
		/// Creates a new \ref host instance. Use this instead of \ref ui::manager::create_element<host>()
		/// so that the \ref host is correctly registered to this manager.
		host *_new_tab_host() {
			host *h = _manager.create_element<host>();
			h->_tab_manager = this;
			return h;
		}
		/// Prepares and marks a \ref host for disposal. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_tab_host(host&);
		/// Splits the given \ref host into halves, and returns the resulting \ref split_panel. The original
		/// \ref host will be detached from its parent.
		split_panel *_replace_with_split_panel(host&);

		/// Splits the given \ref host into halves, moving the given tab to one half and all others to the
		/// other half.
		///
		/// \sa split_tab
		void _split_tab(host&, tab&, orientation, bool newfirst);

		/// Moves the given \ref tab to a new window with the given layout, detaching it from its original parent.
		/// Note that the position of the window (and hence \p layout) is in screen coordinates.
		void _move_tab_to_new_window(tab&, rectd layout);

		/// Detaches \ref _drag_dest_selector from its parent if it has one.
		void _try_detach_destination_selector() {
			if (_drag_dest_selector->parent() != nullptr) {
				assert_true_logical(
					_drag_dest_selector->parent() == _drag_destination,
					"wrong parent for position selector"
				);
				_drag_destination->_set_drag_dest_selector(nullptr);
			}
		}

		/// Iterates through all \ref host "tab_hosts" in a given window, in a DFS-like fashion.
		///
		/// \param base The window.
		/// \param cb A callable object. It can either return \p void, or a \p bool indicating whether to continue.
		template <typename T> inline static void _enumerate_hosts(window_base *base, T &&cb) {
			assert_true_logical(base->children().size() == 1, "window must have only one child");
			std::vector<element*> hsts;
			hsts.push_back(*base->children().items().begin());
			while (!hsts.empty()) {
				element *ce = hsts.back();
				hsts.pop_back();
				auto *hst = dynamic_cast<host*>(ce);
				if (hst) {
					if constexpr (std::is_same_v<decltype(cb(*static_cast<host*>(nullptr))), bool>) {
						if (!cb(*hst)) {
							break;
						}
					} else {
						cb(*hst);
					}
				} else {
					auto *sp = dynamic_cast<split_panel*>(ce);
					assert_true_logical(sp, "corrupted element tree");
					hsts.push_back(sp->get_child1());
					hsts.push_back(sp->get_child2());
				}
			}
		}


		// dragging-related functions
		/// Called when starting to drag a tab in a tab button area or when the user drags a tab into the tab button
		/// area of a \ref host. This function sets \ref _drag_destination and \ref _dragging_in_host, captures the
		/// mouse, and registers event listeners for mouse movement. The tab should have already been added to the
		/// host before calling this function.
		void _start_dragging_in_host(host&);
		/// Called when dragging a tab in a tab button area and the mouse moves. This is called when the mouse moves.
		/// If the mouse is no longer inside the tab button region, this function calls \ref _exit_dragging_in_host()
		/// and \ref _start_dragging_free().
		void _update_dragging_in_host(mouse_move_info&);
		/// Called when the user stops dragging in a \ref host or when the tab is dragged away from one. This
		/// function unregisters event handlers and releases mouse capture.
		void _exit_dragging_in_host() {
			_drag->get_button().mouse_move -= _mouse_move_token;
			_drag->get_window()->release_mouse_capture();
		}

		/// Called when starting to drag a new tab or when the user drags a tab out of the tab buttons area. This
		/// function updates \ref _drag_destination and \ref _dragging_in_host, sets \ref _drag_tab_window up as the
		/// visual indicator, and registers for relevant events.
		///
		/// \param topleft The position of the tab button's top left corner in screen coordinates. This will be used
		///                directly for the position of \ref _drag_tab_window.
		void _start_dragging_free(vec2d topleft);
		/// Updates the tab when dragging free. This is called when the mouse moves.
		void _update_dragging_free(mouse_move_info&);
		/// Called when the user stops dragging the tab freely, either because the user stops dragging entirely or
		/// because the tab is dragged into a tab button region of a \ref host.
		void _exit_dragging_free() {
			_try_detach_destination_selector();
			_drag_tab_window->release_mouse_capture();
			_drag_tab_window->hide();
			_drag_tab_window->children().remove(_drag->get_button());
			_drag->get_button().mouse_move -= _mouse_move_token;
		}

		/// Stops dragging.
		void _stop_dragging();

		/// Updates the position of \ref _drag by putting it before the right tab and setting the correct offset.
		///
		/// \param pos The position of the mouse cursor, relative to the area that contains all
		///            \ref tab_button "tab_buttons"
		void _update_drag_tab_position(vec2d pos) {
			drag_move_tab_button.invoke_noret(pos - _drag_offset);
		}

		/// Called when a \ref tab is removed from a \ref host. Inserts the \ref host to \ref _changed to
		/// update it afterwards, and schedules \ref update_changed_hosts() to be called.
		void _on_tab_detached(host &host, tab&) {
			_changed.insert(&host);
			update_changed_hosts();
		}
	};
}
