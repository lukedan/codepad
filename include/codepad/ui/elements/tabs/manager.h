// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Manager for tabs and tab hosts.

#include <span>

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/elements/overriden_layout_panel.h"
#include "split_panel.h"
#include "tab.h"
#include "host.h"

namespace codepad::ui::tabs {
	/// Manages all \ref tab "tabs" and \ref host "tab_hosts".
	class tab_manager {
		friend tab;
		friend host;
	public:
		/// Information about a tab being dragged.
		struct dragging_tab {
			/// Default constructor.
			dragging_tab() = default;
			/// Initializes all fields of this struct.
			dragging_tab(tab &t, rectd off) : offset(off), target(&t) {
			}

			rectd offset; ///< The offset of the tab button relative to the mouse cursor.
			tab *target = nullptr; ///< The tab that's being dragged.
		};
		/// Information about the user dragging a \ref tab_button.
		struct drag_update_info {
			/// Initializes all fields of this struct.
			explicit drag_update_info(vec2d pos) : position(pos) {
			}
			const vec2d position; /// New position of the mouse cursor relative to the tab buttons region.
		};
		/// Contains related information when the user stops dragging a tab.
		struct drag_ended_info {
			/// Initializes \ref dragging_tab.
			explicit drag_ended_info(std::vector<dragging_tab> ts) : dragging_tabs(std::move(ts)) {
			}
			const std::vector<dragging_tab> dragging_tabs; ///< The tabs that the user was dragging.
		};

		/// Constructor. Initializes \ref _drag_dest_selector and update tasks.
		tab_manager(ui::manager &man) : _manager(man) {
		}
		/// Checks that dragging has stopped, and unregisters update tasks.
		~tab_manager() {
			assert_true_logical(
				_dragging_tabs.empty(), "dragging operation still in progress during tab manager disposal"
			);
			if (_update_hosts_task) {
				_manager.get_scheduler().cancel_synchronous_task(_update_hosts_task);
			}
		}

		/// Creates a new \ref tab in a \ref host in the last focused \ref window. If there are no windows,
		/// a new one is created.
		[[nodiscard]] tab *new_tab();
		/// Creates a new \ref tab in the given \ref host and returns it. If the given \ref host is
		/// \p nullptr, a new window containing a new \ref host will be created, in which the \ref tab will be
		/// created.
		[[nodiscard]] tab *new_tab_in(host* = nullptr);

		/// Returns the total number of windows managed by \ref tab_manager.
		[[nodiscard]] std::size_t window_count() const {
			return _wndlist.size();
		}

		/// Returns \p true if there are no more \ref tab instances.
		[[nodiscard]] bool empty() const {
			return window_count() == 0 && _dragging_tabs.empty();
		}

		/// Splits the \ref host the given \ref tab is in into two \ref host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref host.
		///
		/// \param t The \ref tab.
		/// \param orient The orientation in which this \ref host should split.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref host while other remaining
		///                 tabs will be put in the bottom/right \ref host.
		void split_tabs(std::span<tab *const> ts, orientation orient, bool newfirst) {
			host *host = ts[0]->get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tabs(*host, ts, orient, newfirst);
		}
		/// \overload
		void split_tabs(std::initializer_list<tab*> ts, orientation orient, bool newfirst) {
			split_tabs({ ts.begin(), ts.end() }, orient, newfirst);
		}
		/// Creates a new \ref window and a \ref host and moves the given tabs into the newly created
		/// \ref host. The size of the tab will be that of the first tab.
		///
		/// \return The newly created \ref host.
		host *move_tabs_to_new_window(std::span<tab *const>);
		/// \overload
		host *move_tabs_to_new_window(std::initializer_list<tab*> tabs) {
			return move_tabs_to_new_window({ tabs.begin(), tabs.end() });
		}

		/// Updates all \ref host "tab_hosts" whose tabs have been closed or moved. This is mainly intended to
		/// automatically merge empty tab hosts when they are emptied.
		void update_changed_hosts();

		/// Returns \p true if the user's currently dragging any \ref tab objects.
		[[nodiscard]] bool is_dragging_any_tab() const {
			return !_dragging_tabs.empty();
		}
		/// Returns the tabs that are currently being dragged.
		[[nodiscard]] std::span<const dragging_tab> get_dragging_tabs() const {
			return _dragging_tabs;
		}
		/// Returns \ref _active_dragging_tab.
		[[nodiscard]] std::size_t get_active_dragging_tab_index() const {
			return _active_dragging_tab;
		}
		/// Shorthand for \ref get_dragging_tabs() and \ref get_active_dragging_tab_index().
		[[nodiscard]] dragging_tab get_active_dragging_tab() const {
			return get_dragging_tabs()[get_active_dragging_tab_index()];
		}

		/// Starts dragging all selected tabs in the given host.
		///
		/// \param t The tab to be dragged.
		/// \param offset The offset of the top left corner of the panel that contains all \ref tab_button objects,
		///               relative to the mouse cursor.
		/// \param client_region The layout of the \ref tab's main region, relative to the mouse cursor.
		void start_dragging_selected_tabs(host &h, vec2d offset, rectd client_region);

		info_event<drag_ended_info> drag_ended; ///< Invoked when the user finishes dragging a \ref tab_button.
		/// Invoked while the user is dragging tab buttons.
		info_event<drag_update_info> drag_move_tab_buttons;
	protected:
		std::set<host*> _changed; ///< The set of \ref host "tab_hosts" whose children have changed.
		std::list<window*> _wndlist; ///< The list of windows, ordered according to their z-indices.
		scheduler::sync_task_token _update_hosts_task; ///< Token for the task for updating changed tab hosts.

		// drag destination
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
		window *_drag_tab_window = nullptr; ///< The window used to display the tabs that are being dragged.
		overriden_layout_panel *_drag_tab_panel = nullptr; ///< Child of \ref _drag_tab_window containing all 
		drag_destination_selector *_drag_dest_selector = nullptr; ///< The \ref drag_destination_selector.
		// drag parameters
		/// Tabs that are currently being dragged. These will be in the same order as they were originally in the
		/// host before dragging started.
		std::vector<dragging_tab> _dragging_tabs;
		std::size_t _active_dragging_tab = 0; ///< The index of the active tab in \ref _dragging_tabs.
		rectd _drag_button_rect; ///< The boundaries of \ref _drag_tab_window, relative to the mouse cursor.
		/// The boundaries of the "client" area of the tabs being dragged, relative to the mouse cursor.
		rectd _drag_client_rect;

		ui::manager &_manager; ///< The \ref ui::manager that manages all tabs.


		/// Creates a new window and registers necessary event handlers.
		window *_new_window();
		/// Deletes the given window managed by this \ref tab_manager. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_window(window&);
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

		/// Splits the given \ref host into halves, moving the specified tabs to one half and all others to the
		/// other half.
		///
		/// \return The new \ref host and the old one.
		/// \sa split_tab
		std::pair<host*, host*> _split_tabs(host&, std::span<tab *const>, orientation, bool newfirst);
		/// \overload
		std::pair<host*, host*> _split_tabs(
			host &h, std::initializer_list<tab*> tabs, orientation ori, bool newfirst
		) {
			return _split_tabs(h, { tabs.begin(), tabs.end() }, ori, newfirst);
		}

		/// Moves the given \ref tab to a new window with the given layout, detaching them from their original
		/// parent. Note that the position of the window (and hence \p layout) is in screen coordinates.
		host *_move_tabs_to_new_window(std::span<tab *const>, rectd layout);
		/// \overload
		host *_move_tabs_to_new_window(std::initializer_list<tab*> tabs, rectd layout) {
			return _move_tabs_to_new_window({ tabs.begin(), tabs.end() }, layout);
		}

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
		template <typename T> inline static void _enumerate_hosts(window *base, T &&cb) {
			std::stack<split_panel*> split_panels;

			/// Indicates how a single child has been handled.
			enum class _child_info {
				stop_enumeration, ///< The callback function has indicated that enumeration should be stopped.
				continue_enumeration, ///< The callback has not indicated that enumeration should be stopped.
				/// This child is not a type known to the tab system; otherwise enumeration should continue.
				dont_care
			};
			auto handle_child = [&](element *e) {
				if (auto *hst = dynamic_cast<host*>(e)) {
					if constexpr (std::is_same_v<decltype(cb(*static_cast<host*>(nullptr))), bool>) {
						if (!cb(*hst)) {
							return _child_info::stop_enumeration;
						}
					} else {
						cb(*hst);
					}
					return _child_info::continue_enumeration;
				}
				if (auto *split = dynamic_cast<split_panel*>(e)) {
					split_panels.emplace(split);
					return _child_info::continue_enumeration;
				}
				return _child_info::dont_care;
			};

			for (auto *child : base->children().items()) {
				if (handle_child(child) == _child_info::stop_enumeration) {
					return;
				}
			}
			while (!split_panels.empty()) {
				split_panel *sp = split_panels.top();
				split_panels.pop();
				if (handle_child(sp->get_child1()) == _child_info::stop_enumeration) {
					return;
				}
				if (handle_child(sp->get_child2()) == _child_info::stop_enumeration) {
					return;
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
			get_active_dragging_tab().target->get_button().mouse_move -= _mouse_move_token;
			get_active_dragging_tab().target->get_window()->release_mouse_capture();
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
			for (const auto &t : _dragging_tabs) {
				_drag_tab_panel->children().remove(t.target->get_button());
			}
			get_active_dragging_tab().target->get_button().mouse_move -= _mouse_move_token;
		}

		/// Stops dragging.
		void _stop_dragging();

		/// Updates the position of \ref _drag by putting it before the right tab and setting the correct offset.
		///
		/// \param pos The position of the mouse cursor, relative to the area that contains all
		///            \ref tab_button "tab_buttons"
		void _update_drag_tab_position(vec2d pos) {
			drag_move_tab_buttons.construct_info_and_invoke(pos);
		}

		/// Called when a \ref tab is removed from a \ref host. Inserts the \ref host to \ref _changed to
		/// update it afterwards, and schedules \ref update_changed_hosts() to be called.
		void _on_tab_detached(host &host, tab&) {
			_changed.insert(&host);
			if (_update_hosts_task.empty()) {
				_update_hosts_task = _manager.get_scheduler().register_synchronous_task(
					scheduler::clock_t::now(), nullptr,
					[this](element*) -> std::optional<scheduler::clock_t::time_point> {
						_update_hosts_task = scheduler::sync_task_token();
						update_changed_hosts();
						return std::nullopt;
					}
				);
			}
		}

		/// Called when a host is being disposed to remove it from \ref _changed.
		void _on_host_disposing(host &hst) {
			_changed.erase(&hst);
		}
	};
}
