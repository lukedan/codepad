#pragma once

#include "../../os/misc.h"
#include "../../os/current/window.h"
#include "../element.h"
#include "../panel.h"
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

	/// Manages all \ref tab "tabs" and \ref host "tab_hosts".
	class tab_manager {
		friend tab;
		friend host;
	public:
		/// Constructor. Initializes \ref _drag_dest_selector and update tasks.
		tab_manager(ui::manager &man) : _manager(man) {
			_update_hosts_token = _manager.get_scheduler().register_update_task([this]() {
				update_changed_hosts();
				});

			_drag_dest_selector = _manager.create_element<drag_destination_selector>();
		}
		/// Disposes \ref _drag_dest_selector, and unregisters update tasks.
		~tab_manager() {
			_manager.get_scheduler().mark_for_disposal(*_drag_dest_selector);

			_manager.get_scheduler().unregister_update_task(_update_hosts_token);
		}

		/// Creates a new \ref tab in a \ref host in the last focused \ref window_base. If there are no windows,
		/// a new one is created.
		tab *new_tab() {
			host *hst = nullptr;
			if (!_wndlist.empty()) {
				_enumerate_hosts(*_wndlist.begin(), [&hst](host &h) {
					hst = &h;
					return false;
					});
			}
			return new_tab_in(hst);
		}
		/// Creates a new \ref tab in the given \ref host and returns it. If the given \ref host is
		/// \p nullptr, a new window containing a new \ref host will be created, in which the \ref tab will be
		/// created.
		tab *new_tab_in(host *host = nullptr) {
			if (!host) {
				host = _new_tab_host();
				window_base *w = _new_window();
				w->children().add(*host);
				w->show_and_activate();
			}
			tab *t = _new_detached_tab();
			host->add_tab(*t);
			return t;
		}

		/// Returns the total number of windows managed by \ref tab_manager.
		std::size_t window_count() const {
			return _wndlist.size();
		}

		/// Returns \p true if there are no more \ref tab instances.
		bool empty() const {
			return window_count() == 0 && _drag == nullptr;
		}

		/// Sets the current \ref drag_destination_selector used among all \ref host "tab_hosts".
		void set_drag_destination_selector(drag_destination_selector *sel) {
			if (_drag_dest_selector) {
				_manager.get_scheduler().mark_for_disposal(*_drag_dest_selector);
			}
			_drag_dest_selector = sel;
		}
		/// Returns the current \ref drag_destination_selector used among all \ref host "tab_hosts".
		drag_destination_selector *get_drag_destination_selector() const {
			return _drag_dest_selector;
		}

		/// Splits the \ref host the given \ref tab is in into two \ref host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref host.
		///
		/// \param t The \ref tab.
		/// \param vertical The orientation in which this \ref host should split.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref host while other remaining
		///                 tabs will be put in the bottom/right \ref host.
		void split_tab(tab &t, orientation orient, bool newfirst) {
			host *host = t.get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tab(*host, t, orient, newfirst);
		}
		/// Creates a new \ref os::window and a \ref host and moves the given tab into the newly created
		/// \ref host. The size of the tab will be kept unchanged.
		void move_tab_to_new_window(tab &t) {
			rectd tglayout = t.get_layout();
			host *hst = t.get_host();
			window_base *wnd = t.get_window();
			if (hst != nullptr && wnd != nullptr) {
				tglayout = hst->get_layout().translated(wnd->get_position().convert<double>());
			}
			_move_tab_to_new_window(t, tglayout);
		}

		/// Updates all \ref host "tab_hosts" whose tabs have been closed or moved. This is mainly intended to
		/// automatically merge empty tab hosts when they are emptied.
		void update_changed_hosts() {
			std::set<host*> tmp_changes;
			std::swap(_changed, tmp_changes);
			while (!tmp_changes.empty()) {
				for (host *hst : tmp_changes) {
					if (hst->get_tab_count() == 0) {
						if (auto father = dynamic_cast<split_panel*>(hst->parent()); father) {
							// only merge when two empty hosts are side by side
							auto *other = dynamic_cast<host*>(
								hst == father->get_child1() ?
								father->get_child2() :
								father->get_child1()
								);
							if (other && other->get_tab_count() == 0) { // merge
								father->set_child1(nullptr);
								father->set_child2(nullptr);
								// use the other child to replace the split_panel
								auto ff = dynamic_cast<split_panel*>(father->parent());
								if (ff) {
									if (father == ff->get_child1()) {
										ff->set_child1(other);
									} else {
										assert_true_logical(father == ff->get_child2(), "corrupted element graph");
										ff->set_child2(other);
									}
								} else {
#ifdef CP_CHECK_LOGICAL_ERRORS
									auto f = dynamic_cast<window_base*>(father->parent());
									assert_true_logical(f != nullptr, "parent of parent must be a window or a split panel");
#else
									auto f = static_cast<window_base*>(father->parent());
#endif
									f->children().remove(*father);
									f->children().add(*other);
								}
								_manager.get_scheduler().mark_for_disposal(*father);
								_changed.erase(hst);
								_changed.emplace(other);
								_delete_tab_host(*hst);
							}
						}
					}
				}
				tmp_changes.clear();
				std::swap(_changed, tmp_changes);
			}
		}
		/*
		/// Updates the tab that's currently being dragged.
		///
		/// \todo Updating so frequently seems unnecessary.
		/// \todo Needs major improvements.
		void update_drag() {
			if (_drag) {
				vec2d mouse = os::get_mouse_position().convert<double>();
				if (_drag_dest_type == drag_destination_type::combine_in_host) { // dragging tab_button in a tab list
					rectd rgn = _drag_destination->get_tab_buttons_region().get_layout();
					vec2d mpos = _drag_destination->get_window()->screen_to_client(mouse);
					if (!rgn.contains(mpos)) { // moved out of the region
						_drag_destination->remove_tab(*_drag);
						_drag_dest_type = drag_destination_type::new_window;
						_drag_destination = nullptr;

						_drag_tab_window->children().add(_drag->get_button());
						_drag_tab_window->show();
					} else { // update tab position
						_update_drag_tab_position(mpos);
					}
				}
				// these are used to find the host with the nearest center point
				// however, since no "hovering popup" mechanism is implemented yet, this is of little use right now
				host *mindp = nullptr;
				vec2d minpos;
				double minsql = 0.0;
				if (_drag_dest_type != drag_destination_type::combine_in_host) { // find best host to target
					for (window_base *wnd : _wndlist) { // iterate through all windows according to their z-order
						bool goon = true;
						vec2d mpos = wnd->screen_to_client(mouse);
						if (wnd->hit_test_full_client(mouse)) {
							_enumerate_hosts(wnd, [&](host &hst) {
								rectd rgn = hst.get_tab_buttons_region().get_layout();
								if (rgn.contains(mpos)) { // switch to combine_in_host
									_drag_dest_type = drag_destination_type::combine_in_host;

									_drag_tab_window->children().clear();
									_drag_tab_window->hide();
									_try_dispose_preview();
									_try_detach_destination_selector();

									// change destination and add _drag to it
									_drag_destination = &hst;
									_drag_destination->add_tab(*_drag);
									_drag_destination->activate_tab(*_drag);
									// update position
									_update_drag_tab_position(mpos);
									wnd->activate();

									// should no longer go on
									goon = false;
									return false;
								}
								if (hst.get_layout().contains(mpos)) { // see if this host is closer
									vec2d cdiff = mpos - hst.get_layout().center();
									double dsql = cdiff.length_sqr();
									if (!mindp || minsql > dsql) { // yes it is
										minpos = mpos;
										mindp = &hst;
										minsql = dsql;
									}
								}
								return true;
								});
							if (!goon) {
								break;
							}
							break; // remove this line to take into account all overlapping windows
						}
					}
				}
				if (_drag_dest_type != drag_destination_type::combine_in_host) { // check nearest host
					if (mindp) { // mouse is over a host
						if (_drag_destination != mindp) {
							_try_dispose_preview();
							// move selector to new host
							if (_drag_destination) {
								_drag_destination->_set_drag_dest_selector(nullptr);
							}
							mindp->_set_drag_dest_selector(_drag_dest_selector);
						}
						drag_destination_type newdtype = _drag_dest_selector->get_drag_destination(minpos);
						assert_true_logical(
							newdtype != drag_destination_type::combine_in_host, "invalid destination type"
						);
						if (newdtype != _drag_dest_type || _drag_destination != mindp) { // update preview type
							_try_dispose_preview();
							_drag_dest_type = newdtype;
							_drag_destination = mindp;
							if (_drag_dest_type != drag_destination_type::new_window) { // insert new preview
								_dragdec = new decoration(_manager);
								_dragdec->set_class(CP_STRLIT("drag_preview"));
								_dragdec->set_layout(_get_preview_layout(*_drag_destination, _drag_dest_type));
								_drag_destination->get_window()->register_decoration(*_dragdec);
							}
						}
					} else { // new_window is the only choice
						// dispose everything
						_try_dispose_preview();
						_try_detach_destination_selector();
						_drag_destination = nullptr;
						_drag_dest_type = drag_destination_type::new_window;
					}
					_drag_tab_window->set_position(mouse - _drag_offset);
				}

				if (true) { // stop & move tab to destination
					_drag_tab_window->children().clear();
					_manager.get_scheduler().mark_for_disposal(*_drag_tab_window);
					_drag_tab_window = nullptr;
					// dispose preview and detach selector
					_try_dispose_preview();
					_try_detach_destination_selector();

					switch (_drag_dest_type) {
					case drag_destination_type::new_window:
					{
						rectd r = _dragrect;
						r.ymin = -_drag_offset.y;
						_move_tab_to_new_window(
							*_drag, r.translated(os::get_mouse_position().convert<double>())
						);
					}
					break;
					case drag_destination_type::combine_in_host:
						/*_drag->_btn->invalidate_layout();*//*
						// the tab is already added to _drag_destination
						break;
					case drag_destination_type::combine:
						_drag_destination->add_tab(*_drag);
						_drag_destination->activate_tab(*_drag);
						break;
					default: // split tab
						assert_true_logical(_drag_destination != nullptr, "invalid split target");
						_split_tab(
							*_drag_destination, *_drag,
							_drag_dest_type == drag_destination_type::new_panel_top ||
							_drag_dest_type == drag_destination_type::new_panel_bottom ?
							orientation::vertical : orientation::horizontal,
							_drag_dest_type == drag_destination_type::new_panel_left ||
							_drag_dest_type == drag_destination_type::new_panel_top
						);
						break;
					}
					// the mouse button is not down anymore
					// TODO notify mouse up
					_drag = nullptr;
					end_drag.invoke();
				} else { // keep updating
					_manager.get_scheduler().schedule_update_task(_update_drag_token);
				}
			}
		}
		*/

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
		/// \param stop A callable object that returns \p true when the tab should be released.
		void start_dragging_tab(tab &t, vec2d diff, rectd layout) {
			assert_true_usage(_drag == nullptr, "a tab is currently being dragged");

			_drag_tab_window = dynamic_cast<window_base*>(_manager.create_element("window", "tabs.drag_ghost_window"));
			assert_true_logical(_drag_tab_window, "failed to create transparent window for dragging");
			_drag_tab_window->set_display_caption_bar(false);
			_drag_tab_window->set_display_border(false);
			_drag_tab_window->set_sizable(false);
			_drag_tab_window->set_show_icon(false);
			_drag_tab_window->set_topmost(true);
			_drag_tab_window->set_client_size(t.get_button().get_layout().size());

			_drag = &t;
			_drag_offset = diff;
			_dragrect = layout;

			_stop_drag_token = (
				_drag->get_button().mouse_up += [this](mouse_button_info&) {
					_stop_dragging();
				}
			);
			_capture_lost_token = (
				_drag->get_button().lost_capture += [this]() {
					_stop_dragging();
				}
			);

			host *hst = t.get_host();
			if (hst) {
				_start_dragging_in_host(*hst);
			} else {
				_start_dragging_free();
			}

			/*_manager.get_scheduler().schedule_update_task(_update_drag_token);*/
		}

		info_event<> end_drag; ///< Invoked when the user finishes dragging a \ref tab_button.
		info_event<tab_drag_update_info> drag_move_tab_button; ///< Invoked while the user is dragging a \ref tab_button.
	protected:
		std::set<host*> _changed; ///< The set of \ref host "tab_hosts" whose children have changed.
		std::list<window_base*> _wndlist; ///< The list of windows, ordered according to their z-indices.
		/// Token of the task that updates changed tab hosts.
		scheduler::update_task::token _update_hosts_token;

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
		window_base *_new_window() {
			window_base *wnd = _manager.create_element<os::window>();
			_wndlist.emplace_front(wnd);
			wnd->got_window_focus += [this, wnd]() {
				// there can't be too many windows... right?
				auto it = std::find(_wndlist.begin(), _wndlist.end(), wnd);
				assert_true_logical(it != _wndlist.end(), "window has been silently removed");
				_wndlist.erase(it);
				_wndlist.emplace_front(wnd);
			};
			wnd->close_request += [this, wnd]() { // when requested to be closed, send request to all tabs
				_enumerate_hosts(wnd, [](host &hst) {
					auto &tabs = hst.get_tabs().items();
					std::vector<element*> ts(tabs.begin(), tabs.end());
					for (element *e : ts) {
						if (tab *t = dynamic_cast<tab*>(e)) {
							t->_on_close_requested();
						}
					}
					});
				update_changed_hosts(); // to ensure that empty hosts are merged
				if (wnd->children().items().size() == 1) {
					auto *hst = dynamic_cast<host*>(*wnd->children().items().begin());
					if (hst && hst->get_tab_count() == 0) {
						_delete_tab_host(*hst); // just in case
						_delete_window(*wnd);
					}
				}
			};
			return wnd;
		}
		/// Deletes the given window managed by this \ref tab_manager. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_window(window_base &wnd) {
			for (auto it = _wndlist.begin(); it != _wndlist.end(); ++it) {
				if (*it == &wnd) {
					_wndlist.erase(it);
					break;
				}
			}
			_manager.get_scheduler().mark_for_disposal(wnd);
		}
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
		void _delete_tab_host(host &hst) {
			logger::get().log_debug(CP_HERE) << "tab host 0x" << &hst << " disposed";
			if (_drag && _drag_destination == &hst) {
				logger::get().log_debug(CP_HERE) << "resetting drag destination";
				_try_detach_destination_selector();
				_drag_destination = nullptr;
				_dragging_in_host = false;
			}
			_manager.get_scheduler().mark_for_disposal(hst);
		}
		/// Splits the given \ref host into halves, and returns the resulting \ref split_panel. The original
		/// \ref host will be removed from its parent.
		split_panel *_replace_with_split_panel(host &hst) {
			split_panel *sp = _manager.create_element<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
			if (f) {
				if (f->get_child1() == &hst) {
					f->set_child1(sp);
				} else {
					assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
					f->set_child2(sp);
				}
			} else {
				auto *w = dynamic_cast<window_base*>(hst.parent());
				assert_true_logical(w != nullptr, "root element must be a window");
				w->children().remove(hst);
				w->children().add(*sp);
			}
			return sp;
		}

		/// Splits the given \ref host into halves, moving the given tab to one half and all others to the
		/// other half.
		///
		/// \sa split_tab
		void _split_tab(host &hst, tab &t, orientation orient, bool newfirst) {
			if (t.get_host() == &hst) {
				hst.remove_tab(t);
			}
			split_panel *sp = _replace_with_split_panel(hst);
			host *th = _new_tab_host();
			if (newfirst) {
				sp->set_child1(th);
				sp->set_child2(&hst);
			} else {
				sp->set_child1(&hst);
				sp->set_child2(th);
			}
			th->add_tab(t);
			sp->set_orientation(orient);
		}

		/// Moves the given \ref tab to a new window with the given layout, detaching it from its original parent.
		void _move_tab_to_new_window(tab &t, rectd layout) {
			host *hst = t.get_host();
			if (hst != nullptr) {
				hst->remove_tab(t);
			}
			window_base *wnd = _new_window();
			wnd->set_client_size(layout.size());
			wnd->set_position(layout.xmin_ymin());
			host *nhst = _new_tab_host();
			wnd->children().add(*nhst);
			nhst->add_tab(t);
			wnd->show_and_activate();
		}

		/// Detaches \ref _drag_dest_selector from its parent if it has one.
		void _try_detach_destination_selector() {
			if (_drag_dest_selector->parent() != nullptr) {
				assert_true_logical(_drag_dest_selector->parent() == _drag_destination, "wrong parent for position selector");
				_drag_destination->_set_drag_dest_selector(nullptr);
			}
		}

		/// Iterates through all \ref host "tab_hosts" in a given window, in a dfs-like fashion.
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


		// dragging related functions
		/// Called when starting to drag a tab in a tab button area or when the user drags a tab into the tab button
		/// area of a \ref host. This function sets \ref _drag_destination and \ref _drag_dest_type, captures the
		/// mouse, and registers event listeners for mouse movement. The tab should have already been added to the
		/// host before calling this function.
		void _start_dragging_in_host(host &h) {
			_drag_destination = &h;
			_dragging_in_host = true;
			assert_true_logical(_drag->get_button().parent(), "the tab should've already been added to the host");
			_drag->get_window()->set_mouse_capture(_drag->get_button());
			_mouse_move_token = (
				_drag->get_button().mouse_move += [this](mouse_move_info &p) {
					_update_dragging_in_host(p);
				}
			);
		}
		/// Called when dragging a tab in a tab button area and the mouse moves. This is called when the mouse moves.
		void _update_dragging_in_host(mouse_move_info &p) {
			panel &region = *_drag->get_button().parent();
			vec2d mouse = p.new_position.get(region);
			if (!rectd::from_corners(vec2d(), region.get_layout().size()).contains(mouse)) {
				_exit_dragging_in_host();
				_drag_destination->remove_tab(*_drag);
				_start_dragging_free();
				return;
			}
			_update_drag_tab_position(mouse); // update position
		}
		/// Called when the user stops dragging in a \ref host or when the tab is dragged away from one. This
		/// function unregisters event handlers and releases mouse capture.
		void _exit_dragging_in_host() {
			_drag->get_button().mouse_move -= _mouse_move_token;
			_drag->get_window()->release_mouse_capture();
		}

		/// Called when starting to drag a new tab or when the user drags a tab out of the tab buttons area. This
		/// function sets \ref _drag_dest_type and starts the global update task.
		void _start_dragging_free() {
			_drag_destination = nullptr;
			_dragging_in_host = false;
			_drag_tab_window->children().add(_drag->get_button());
			_drag_tab_window->show();
			_drag_tab_window->set_mouse_capture(_drag->get_button());
			/*_drag_tab_window->set_position(_drag_tab_window->client_to_screen(
				p.new_position.get(*_drag_tab_window) - _drag_offset
			));*/ // TODO obtain the correct position
			_mouse_move_token = (
				_drag->get_button().mouse_move += [this](mouse_move_info &p) {
					_update_dragging_free(p);
				}
			);
		}
		/// Updates the tab when dragging free. This is called when the mouse moves.
		void _update_dragging_free(mouse_move_info &p) {
			// find the tab host that the mouse is currently over
			host *target = nullptr;
			for (window_base *wnd : _wndlist) {
				if (wnd->hit_test(p.new_position.get(*wnd))) { // hit
					_enumerate_hosts(
						wnd, [&p, &target](host &hst) {
							if (hst.hit_test(p.new_position.get(hst))) { // here it is
								target = &hst;
								return false;
							}
							return true;
						}
					);
					break;
				}
			}
			if (_drag_destination != target) {
				_try_detach_destination_selector();
			}
			if (target) { // over a tab host
				panel &buttons = target->get_tab_buttons_region();
				if (buttons.hit_test(p.new_position.get(buttons))) { // should be dragged in the tab button area
					_exit_dragging_free();

					target->add_tab(*_drag);
					target->activate_tab(*_drag);
					_update_drag_tab_position(p.new_position.get(*_drag->get_button().parent()));

					_start_dragging_in_host(*target);
					return;
				}
				target->_set_drag_dest_selector(_drag_dest_selector);
				_drag_dest_selector->update(p);
			} else { // to new window
				_try_detach_destination_selector();
			}
			_drag_destination = target;
			_drag_tab_window->set_position(_drag_tab_window->client_to_screen(
				p.new_position.get(*_drag_tab_window) - _drag_offset
			));
		}
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
		void _stop_dragging() {
			if (_dragging_in_host) {
				_exit_dragging_in_host();
			} else {
				_exit_dragging_free();
				drag_split_type split = drag_split_type::new_window;
				if (_drag_destination) {
					split = _drag_dest_selector->get_drag_destination(); // TODO
				}
				switch (split) {
				case drag_split_type::combine:
					_drag_destination->add_tab(*_drag);
					_drag_destination->activate_tab(*_drag);
					break;
				case drag_split_type::new_window:
				{
					rectd r = _dragrect;
					r.ymin = -_drag_offset.y;
					_move_tab_to_new_window(
						*_drag, r.translated(os::get_mouse_position().convert<double>()) // TODO fixme wrong position
					);
					break;
				}
				default: // split
					assert_true_logical(_drag_destination != nullptr, "invalid split target");
					_split_tab(
						*_drag_destination, *_drag,
						split == drag_split_type::split_top ||
						split == drag_split_type::split_bottom ?
						orientation::vertical : orientation::horizontal,
						split == drag_split_type::split_left ||
						split == drag_split_type::split_top
					);
					break;
				}
			}

			// dispose of _drag_tab_window
			_drag_tab_window->children().clear();
			_manager.get_scheduler().mark_for_disposal(*_drag_tab_window);
			_drag_tab_window = nullptr;
			// unregister events
			_drag->get_button().mouse_up -= _stop_drag_token;
			_drag->get_button().lost_capture -= _capture_lost_token;

			_drag = nullptr;
			_drag_destination = nullptr;
			end_drag.invoke();

			// TODO maybe notify the tab of mouse up
		}

		/// Updates the position of \ref _drag by putting it before the right tab and setting the correct offset.
		///
		/// \param pos The position of the mouse cursor, relative to the area that contains all
		///            \ref tab_button "tab_buttons"
		/// \param maxw The width of the area that contains all \ref tab_button "tab_buttons".
		void _update_drag_tab_position(vec2d pos) {
			drag_move_tab_button.invoke_noret(pos - _drag_offset);
		}

		/// Called when a \ref tab is removed from a \ref host. Inserts the \ref host to \ref _changed to
		/// update it afterwards, and schedules \ref update_changed_hosts() to be called.
		void _on_tab_detached(host &host, tab&) {
			_changed.insert(&host);
			_manager.get_scheduler().schedule_update_task(_update_hosts_token);
		}
	};
}
