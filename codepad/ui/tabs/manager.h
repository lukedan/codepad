#pragma once

#include "../../os/misc.h"
#include "../../os/current/window.h"
#include "../element.h"
#include "../panel.h"
#include "split_panel.h"
#include "tab.h"
#include "tab_host.h"

namespace codepad::ui {
	/// Information about the user dragging a \ref tab_button.
	struct tab_drag_update_info {
		/// Initializes all fields of this struct.
		explicit tab_drag_update_info(vec2d pos) : position(pos) {
		}
		const vec2d position; /// New position of the top-left corner of the \ref tab_button.
	};

	/// Manages all \ref tab "tabs" and \ref tab_host "tab_hosts".
	class tab_manager {
		friend tab;
		friend tab_host;
	public:
		/// Constructor. Initializes \ref _possel and update tasks.
		tab_manager(ui::manager &man) : _manager(man) {
			_update_hosts_token = _manager.get_scheduler().register_update_task([this]() {
				update_changed_hosts();
			});
			_update_drag_token = _manager.get_scheduler().register_update_task([this]() {
				update_drag();
			});

			_possel = _manager.create_element<drag_destination_selector>();
		}
		/// Disposes \ref _possel, and unregisters update tasks.
		~tab_manager() {
			_manager.get_scheduler().mark_for_disposal(*_possel);

			_manager.get_scheduler().unregister_update_task(_update_drag_token);
			_manager.get_scheduler().unregister_update_task(_update_hosts_token);
		}

		/// Creates a new \ref tab in a \ref tab_host in the last focused \ref os::window_base. If there are no
		/// windows, a new one is created.
		tab *new_tab() {
			tab_host *host = nullptr;
			if (!_wndlist.empty()) {
				_enumerate_hosts(*_wndlist.begin(), [&host](tab_host &h) {
					host = &h;
					return false;
				});
			}
			return new_tab_in(host);
		}
		/// Creates a new \ref tab in the given \ref tab_host and returns it. If the given \ref tab_host is
		/// \p nullptr, a new window containing a new \ref tab_host will be created, in which the \ref tab will be
		/// created.
		tab *new_tab_in(tab_host *host = nullptr) {
			if (!host) {
				host = _new_tab_host();
				_new_window()->children().add(*host);
			}
			tab *t = _new_detached_tab();
			host->add_tab(*t);
			return t;
		}

		/// Returns the total number of windows managed by \ref tab_manager.
		size_t window_count() const {
			return _wndlist.size();
		}

		/// Returns \p true if there are no more \ref tab instances.
		bool empty() const {
			return window_count() == 0 && _drag == nullptr;
		}

		/// Sets the current \ref drag_destination_selector used among all \ref tab_host "tab_hosts".
		void set_drag_destination_selector(drag_destination_selector *sel) {
			if (_possel) {
				_manager.get_scheduler().mark_for_disposal(*_possel);
			}
			_possel = sel;
		}
		/// Returns the current \ref drag_destination_selector used among all \ref tab_host "tab_hosts".
		drag_destination_selector *get_drag_destination_selector() const {
			return _possel;
		}

		/// Splits the \ref tab_host the given \ref tab is in into two \ref tab_host "tab_hosts" in a
		/// \ref split_panel, and moves the given tab into the other \ref tab_host.
		///
		/// \param t The \ref tab.
		/// \param vertical If \p true, the new tab will be placed above or below old ones. Otherwise they'll be
		///                 placed side by side.
		/// \param newfirst If \p true, \p t will be placed in the top/left \ref tab_host while other remaining
		///                 tabs will be put in the bottom/right \ref tab_host.
		void split_tab(tab &t, bool vertical, bool newfirst) {
			tab_host *host = t.get_host();
			assert_true_usage(host != nullptr, "cannot split tab without host");
			_split_tab(*host, t, vertical, newfirst);
		}
		/// Creates a new \ref os::window and a \ref tab_host and moves the given tab into the newly created
		/// \ref tab_host. The size of the tab will be kept unchanged.
		void move_tab_to_new_window(tab &t) {
			rectd tglayout = t.get_layout();
			tab_host *hst = t.get_host();
			ui::window_base *wnd = t.get_window();
			if (hst != nullptr && wnd != nullptr) {
				tglayout = hst->get_layout().translated(wnd->get_position().convert<double>());
			}
			_move_tab_to_new_window(t, tglayout);
		}

		/// Updates all \ref tab_host "tab_hosts" whose tabs have been closed or moved. This is mainly intended to
		/// automatically merge empty tab hosts when they are emptied.
		void update_changed_hosts() {
			std::set<tab_host*> tmp_changes;
			std::swap(_changed, tmp_changes);
			while (!tmp_changes.empty()) {
				for (tab_host *host : tmp_changes) {
					if (host->get_tab_count() == 0) {
						if (auto father = dynamic_cast<split_panel*>(host->parent()); father) {
							// only merge when two empty hosts are side by side
							auto *other = dynamic_cast<tab_host*>(
								host == father->get_child1() ?
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
									auto f = dynamic_cast<ui::window_base*>(father->parent());
									assert_true_logical(f != nullptr, "parent of parent must be a window or a split panel");
#else
									auto f = static_cast<os::window_base*>(father->parent());
#endif
									f->children().remove(*father);
									f->children().add(*other);
								}
								_manager.get_scheduler().mark_for_disposal(*father);
								_changed.erase(host);
								_changed.emplace(other);
								_delete_tab_host(*host);
							}
						}
					}
				}
				tmp_changes.clear();
				std::swap(_changed, tmp_changes);
			}
		}
		/// Updates the tab that's currently being dragged.
		///
		/// \todo Updating so frequently seems unnecessary.
		/// \todo Needs major improvements.
		void update_drag() {
			if (_drag) {
				vec2i mouse = os::get_mouse_position();
				if (_dtype == drag_destination_type::combine_in_tab) { // dragging tab_button in a tab list
					rectd rgn = _dest->get_tab_buttons_region();
					vec2d mpos = _dest->get_window()->screen_to_client(mouse).convert<double>();
					if (!rgn.contains(mpos)) { // moved out of the region
						_dest->remove_tab(*_drag);
						_dtype = drag_destination_type::new_window;
						_dest = nullptr;
					} else { // update tab position
						_update_drag_tab_position(mpos);
					}
				}
				// these are used to find the tab_host with the nearest center point
				// however, since no "hovering popup" mechanism is implemented yet, this is of little use right now
				tab_host *mindp = nullptr;
				vec2d minpos;
				double minsql = 0.0;
				if (_dtype != drag_destination_type::combine_in_tab) { // find best tab_host to target
					for (window_base *wnd : _wndlist) { // iterate through all windows according to their z-order
						bool goon = true;
						vec2d mpos = wnd->screen_to_client(mouse).convert<double>();
						if (wnd->hit_test_full_client(mouse)) {
							_enumerate_hosts(wnd, [&](tab_host &hst) {
								rectd rgn = hst.get_tab_buttons_region();
								if (rgn.contains(mpos)) { // switch to combine_in_tab
									_dtype = drag_destination_type::combine_in_tab;
									_try_dispose_preview();
									_try_detach_possel();
									// change destination and add _drag to it
									_dest = &hst;
									_dest->add_tab(*_drag);
									_dest->activate_tab(*_drag);
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
				if (_dtype != drag_destination_type::combine_in_tab) { // check nearest host
					if (mindp) { // mouse is over a host
						if (_dest != mindp) {
							_try_dispose_preview();
							// move selector to new host
							if (_dest) {
								_dest->_set_drag_dest_selector(nullptr);
							}
							mindp->_set_drag_dest_selector(_possel);
						}
						drag_destination_type newdtype = _possel->get_drag_destination(minpos);
						assert_true_logical(
							newdtype != drag_destination_type::combine_in_tab, "invalid destination type"
						);
						if (newdtype != _dtype || _dest != mindp) { // update preview type
							_try_dispose_preview();
							_dtype = newdtype;
							_dest = mindp;
							if (_dtype != drag_destination_type::new_window) { // insert new preview
								_dragdec = new decoration(_manager);
								_dragdec->set_class(CP_STRLIT("drag_preview"));
								_dragdec->set_layout(_get_preview_layout(*_dest, _dtype));
								_dest->get_window()->register_decoration(*_dragdec);
							}
						}
					} else { // new_window is the only choice
						// dispose everything
						_try_dispose_preview();
						_try_detach_possel();
						_dest = nullptr;
						_dtype = drag_destination_type::new_window;
					}
				}

				if (_stopdrag()) { // stop & move tab to destination
					switch (_dtype) {
					case drag_destination_type::new_window:
					{
						rectd r = _dragrect;
						r.ymin = _dragdiff.y;
						_move_tab_to_new_window(
							*_drag, r.translated(os::get_mouse_position().convert<double>())
						);
					}
					break;
					case drag_destination_type::combine_in_tab:
						/*_drag->_btn->invalidate_layout();*/
						// the tab is already added to _dest
						break;
					case drag_destination_type::combine:
						_dest->add_tab(*_drag);
						_dest->activate_tab(*_drag);
						break;
					default: // split tab
						assert_true_logical(_dest != nullptr, "invalid split target");
						_split_tab(
							*_dest, *_drag,
							_dtype == drag_destination_type::new_panel_top ||
							_dtype == drag_destination_type::new_panel_bottom,
							_dtype == drag_destination_type::new_panel_left ||
							_dtype == drag_destination_type::new_panel_top
						);
						break;
					}
					// dispose preview and detach selector
					_try_dispose_preview();
					_try_detach_possel();
					// the mouse button is not down anymore
					_drag->_btn->set_state_bits(_manager.get_predefined_states().mouse_down, false);
					_drag = nullptr;
					end_drag_move.invoke();
				} else { // keep updating
					_manager.get_scheduler().schedule_update_task(_update_drag_token);
				}
			}
		}

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
		void start_drag_tab(tab &t, vec2d diff, rectd layout, std::function<bool()> stop = []() {
			return !os::is_mouse_button_down(ui::mouse_button::primary);
		}) {
			assert_true_usage(_drag == nullptr, "a tab is currently being dragged");
			tab_host *hst = t.get_host();
			if (hst) {
				_dest = hst;
				_dtype = drag_destination_type::combine_in_tab;
			} else {
				_dest = nullptr;
				_dtype = drag_destination_type::new_window;
			}
			_drag = &t;
			_dragdiff = diff;
			_dragrect = layout;
			_stopdrag = std::move(stop);
			_manager.get_scheduler().schedule_update_task(_update_drag_token);
		}

		info_event<> end_drag_move; ///< Invoked when the user finishes dragging a \ref tab_button.
		info_event<tab_drag_update_info> drag_move_tab_button; ///< Invoked while the user is dragging a \ref tab_button.
	protected:
		std::set<tab_host*> _changed; ///< The set of \ref tab_host "tab_hosts" whose children have changed.
		std::list<ui::window_base*> _wndlist; ///< The list of windows, ordered according to their z-indices.

		tab *_drag = nullptr; ///< The \ref tab that's currently being dragged.
		tab_host *_dest = nullptr; ///< The destination \ref tab_host of the \ref tab that's currently being dragged.
		drag_destination_type _dtype = drag_destination_type::new_window; ///< The type of the drag destination.
		vec2d _dragdiff; ///< The offset from the top left corner of the \ref tab_button to the mouse cursor.
		rectd _dragrect; ///< The boundaries of the main panel of \ref _drag, relative to the mouse cursor.
		std::function<bool()> _stopdrag; ///< The function used to determine when to stop dragging.
		ui::scheduler::update_task::token
			_update_hosts_token, ///< Token of the task that updates changed tab hosts.
			_update_drag_token; ///< Token of the task that updates the tab that's being dragged.
		/// The decoration for indicating where the tab will be if the user releases the primary mouse button.
		ui::decoration *_dragdec = nullptr;
		drag_destination_selector *_possel = nullptr; ///< The \ref drag_destination_selector.
		ui::manager &_manager; ///< The \ref ui::manager that manages all tabs.

		/// Creates a new window and registers necessary event handlers.
		ui::window_base *_new_window() {
			ui::window_base *wnd = _manager.create_element<os::window>();
			_wndlist.emplace_back(wnd);
			wnd->got_window_focus += [this, wnd]() {
				// there can't be too many windows... right?
				auto it = std::find(_wndlist.begin(), _wndlist.end(), wnd);
				assert_true_logical(it != _wndlist.end(), "window has been silently removed");
				_wndlist.erase(it);
				_wndlist.insert(_wndlist.begin(), wnd);
			};
			wnd->close_request += [this, wnd]() { // when requested to be closed, send request to all tabs
				_enumerate_hosts(wnd, [](tab_host &hst) {
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
					auto *host = dynamic_cast<tab_host*>(*wnd->children().items().begin());
					if (host && host->get_tab_count() == 0) {
						_delete_tab_host(*host); // just in case
						_delete_window(*wnd);
					}
				}
			};
			return wnd;
		}
		/// Deletes the given window managed by this \ref tab_manager. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_window(ui::window_base &wnd) {
			for (auto it = _wndlist.begin(); it != _wndlist.end(); ++it) {
				if (*it == &wnd) {
					_wndlist.erase(it);
					break;
				}
			}
			_manager.get_scheduler().mark_for_disposal(wnd);
		}
		/// Creates a new \ref tab instance not attached to any \ref tab_host. Use this instead of
		/// \ref ui::manager::create_element<tab>() so that the \ref tab is correctly registered to this manager.
		tab *_new_detached_tab() {
			tab *t = _manager.create_element<tab>();
			t->_tab_manager = this;
			return t;
		}
		/// Creates a new \ref tab_host instance. Use this instead of \ref ui::manager::create_element<tab_host>()
		/// so that the \ref tab_host is correctly registered to this manager.
		tab_host *_new_tab_host() {
			tab_host *h = _manager.create_element<tab_host>();
			h->_tab_manager = this;
			return h;
		}
		/// Prepares and marks a \ref tab_host for disposal. Use this instead of directly calling
		/// \ref scheduler::mark_for_disposal().
		void _delete_tab_host(tab_host &hst) {
			logger::get().log_info(CP_HERE, "tab host 0x", &hst, " disposed");
			if (_drag && _dest == &hst) {
				logger::get().log_info(CP_HERE, "resetting drag destination");
				_try_dispose_preview();
				_try_detach_possel();
				_dest = nullptr;
				_dtype = drag_destination_type::new_window;
			}
			_manager.get_scheduler().mark_for_disposal(hst);
		}
		/// Splits the given \ref tab_host into halves, and returns the resulting \ref split_panel. The original
		/// \ref tab_host will be removed from its parent.
		split_panel *_replace_with_split_panel(tab_host &hst) {
			split_panel *sp = _manager.create_element<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
			sp->set_can_focus(false);
			if (f) {
				if (f->get_child1() == &hst) {
					f->set_child1(sp);
				} else {
					assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
					f->set_child2(sp);
				}
			} else {
				auto *w = dynamic_cast<ui::window_base*>(hst.parent());
				assert_true_logical(w != nullptr, "root element must be a window");
				w->children().remove(hst);
				w->children().add(*sp);
			}
			return sp;
		}

		/// Splits the given \ref tab_host into halves, moving the given tab to one half and all others to the
		/// other half.
		///
		/// \sa split_tab
		void _split_tab(tab_host &host, tab &t, bool vertical, bool newfirst) {
			if (t.get_host() == &host) {
				host.remove_tab(t);
			}
			split_panel *sp = _replace_with_split_panel(host);
			tab_host *th = _new_tab_host();
			if (newfirst) {
				sp->set_child1(th);
				sp->set_child2(&host);
			} else {
				sp->set_child1(&host);
				sp->set_child2(th);
			}
			th->add_tab(t);
			sp->set_is_vertical(vertical);
		}

		/// Moves the given \ref tab to a new window with the given layout, detaching it from its original parent.
		void _move_tab_to_new_window(tab &t, rectd layout) {
			tab_host *host = t.get_host();
			if (host != nullptr) {
				host->remove_tab(t);
			}
			ui::window_base *wnd = _new_window();
			tab_host *nhst = _new_tab_host();
			wnd->children().add(*nhst);
			nhst->add_tab(t);
			wnd->set_client_size(layout.size().convert<int>());
			wnd->set_position(layout.xmin_ymin().convert<int>());
		}

		/// Disposes \ref _dragdec if it isn't \p nullptr.
		void _try_dispose_preview() {
			if (_dragdec) {
				_dragdec->set_state(_manager.get_predefined_states().corpse);
				_dragdec = nullptr;
			}
		}
		/// Detaches \ref _possel from its parent if it has one.
		void _try_detach_possel() {
			if (_possel->parent() != nullptr) {
				assert_true_logical(_possel->parent() == _dest, "wrong parent for position selector");
				_dest->_set_drag_dest_selector(nullptr);
			}
		}

		/// Returns the layout of \ref _dragdec for the given \ref tab_host and \ref drag_destination_type.
		inline static rectd _get_preview_layout(const tab_host &th, drag_destination_type dtype) {
			rectd r = th.get_layout();
			switch (dtype) {
			case drag_destination_type::new_panel_left:
				r.xmax = r.centerx();
				break;
			case drag_destination_type::new_panel_top:
				r.ymax = r.centery();
				break;
			case drag_destination_type::new_panel_right:
				r.xmin = r.centerx();
				break;
			case drag_destination_type::new_panel_bottom:
				r.ymin = r.centery();
				break;
			default:
				break;
			}
			return r;
		}

		/// Iterates through all \ref tab_host "tab_hosts" in a given window, in a dfs-like fashion.
		///
		/// \param base The window.
		/// \param cb A callable object. It can either return \p void, or a \p bool indicating whether to continue.
		template <typename T> inline static void _enumerate_hosts(ui::window_base *base, T &&cb) {
			assert_true_logical(base->children().size() == 1, "window must have only one child");
			std::vector<ui::element*> hsts;
			hsts.push_back(*base->children().items().begin());
			while (!hsts.empty()) {
				ui::element *ce = hsts.back();
				hsts.pop_back();
				auto *hst = dynamic_cast<tab_host*>(ce);
				if (hst) {
					if constexpr (std::is_same_v<decltype(cb(*static_cast<tab_host*>(nullptr))), bool>) {
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

		/// Updates the position of \ref _drag by putting it before the right tab and setting the correct offset.
		///
		/// \param pos The position of the mouse cursor, relative to the area that contains all
		///            \ref tab_button "tab_buttons"
		/// \param maxw The width of the area that contains all \ref tab_button "tab_buttons".
		void _update_drag_tab_position(vec2d pos) {
			drag_move_tab_button.invoke_noret(pos + _dragdiff);
		}

		/// Called when a \ref tab is removed from a \ref tab_host. Inserts the \ref tab_host to \ref _changed to
		/// update it afterwards, and schedules \ref update_changed_hosts() to be called.
		void _on_tab_detached(tab_host &host, tab&) {
			_changed.insert(&host);
			_manager.get_scheduler().schedule_update_task(_update_hosts_token);
		}
	};
}
