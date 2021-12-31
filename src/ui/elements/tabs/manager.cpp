// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/tabs/manager.h"

/// \file
/// Implementation of the tab manager.

namespace codepad::ui::tabs {
	tab *tab_manager::new_tab() {
		host *hst = nullptr;
		if (!_wndlist.empty()) {
			_enumerate_hosts(*_wndlist.begin(), [&hst](host &h) {
				hst = &h;
				return false;
				});
		}
		return new_tab_in(hst);
	}

	tab *tab_manager::new_tab_in(host *host) {
		if (!host) {
			host = _new_tab_host();
			window *w = _new_window();
			w->children().add(*host);
			w->show_and_activate();
		}
		tab *t = _new_detached_tab();
		host->add_tab(*t);
		return t;
	}

	void tab_manager::move_tabs_to_new_window(std::span<tab *const> tabs) {
		rectd tglayout = tabs[0]->get_layout();
		host *hst = tabs[0]->get_host();
		window *wnd = tabs[0]->get_window();
		if (hst != nullptr && wnd != nullptr) {
			vec2d windowpos = wnd->client_to_screen(hst->get_layout().xmin_ymin());
			tglayout = rectd::from_corner_and_size(windowpos, tglayout.size());
		}
		_move_tabs_to_new_window(tabs, tglayout);
	}

	void tab_manager::update_changed_hosts() {
		std::set<host*> tmp_changes;
		std::swap(_changed, tmp_changes);
		while (!tmp_changes.empty()) {
			for (host *hst : tmp_changes) {
				if (hst->get_tab_count() == 0) {
					if (auto father = dynamic_cast<split_panel*>(hst->parent()); father) {
						// only merge when two empty hosts are side by side
						auto *other = dynamic_cast<host*>(
							hst == father->get_child1() ? father->get_child2() : father->get_child1()
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
								auto f = dynamic_cast<window*>(father->parent());
								assert_true_logical(f != nullptr, "parent of parent must be a window or a split panel");
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

	void tab_manager::start_dragging_selected_tabs(host &h, vec2d offset, rectd client_region) {
		assert_true_usage(!is_dragging_any_tab(), "a tab is currently being dragged");

		// collect all tabs to be dragged
		for (element *e : h.get_tabs().items()) {
			if (auto *t = dynamic_cast<tab*>(e)) {
				if (t->is_selected()) {
					_dragging_tabs.emplace_back(t);
				}
			}
		}
		// collect drag window area
		_drag_button_rect = rectd(
			std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
			std::numeric_limits<double>::max(), -std::numeric_limits<double>::max()
		);
		vec2d absolute_mouse_pos = offset + h.get_tab_buttons_region().get_layout().xmin_ymin();
		for (std::size_t i = 0; i < _dragging_tabs.size(); ++i) {
			tab *t = _dragging_tabs[i];
			rectd layout = t->get_button().get_layout();
			_drag_button_rect = rectd::bounding_box(_drag_button_rect, layout);
			// fill in correct mouse offsets for all tab buttons
			t->get_button()._drag_pos = absolute_mouse_pos - t->get_button().get_layout().xmin_ymin();

			// switch around the tabs so that the active tab is the first one
			if (h.get_active_tab() == t) {
				std::swap(_dragging_tabs[0], _dragging_tabs[i]);
			}
		}
		_drag_button_rect = _drag_button_rect.translated(-h.get_layout().xmin_ymin() - offset);
		_drag_client_rect = client_region;

		_drag_tab_window = dynamic_cast<window*>(_manager.create_element(
			u8"window", u8"tabs.drag_ghost_window"
		));
		assert_true_logical(_drag_tab_window, "failed to create transparent window for dragging");
		_drag_tab_window->set_display_caption_bar(false);
		_drag_tab_window->set_display_border(false);
		_drag_tab_window->set_sizable(false);
		_drag_tab_window->set_show_icon(false);
		_drag_tab_window->set_topmost(true);
		_drag_tab_window->set_client_size(_drag_button_rect.size());

		assert_true_logical(_drag_dest_selector == nullptr, "overlapping drag operations");
		_drag_dest_selector = dynamic_cast<drag_destination_selector*>(_manager.create_element(
			u8"drag_destination_selector", u8"drag_destination_selector"
		));

		_stop_drag_token = (
			_dragging_tabs[0]->get_button().mouse_up += [this](mouse_button_info&) {
				_stop_dragging();
			}
		);
		_capture_lost_token = (
			_dragging_tabs[0]->get_button().lost_capture += [this]() {
				_stop_dragging();
			}
		);

		_start_dragging_in_host(h);
	}

	window *tab_manager::_new_window() {
		window *wnd = _manager.create_element<window>();
		_wndlist.emplace_front(wnd);
		wnd->got_window_focus += [this, wnd]() {
			// there can't be too many windows... right?
			auto it = std::find(_wndlist.begin(), _wndlist.end(), wnd);
			assert_true_logical(it != _wndlist.end(), "window has been silently removed");
			_wndlist.erase(it);
			_wndlist.emplace_front(wnd);
		};
		wnd->close_request += [this, wnd]() { // when requested to be closed, send request to all tabs
			bool all_closed = true;
			_enumerate_hosts(
				wnd, [&](host &hst) {
					auto &tabs = hst.get_tabs().items();
					std::vector<element*> ts(tabs.begin(), tabs.end());
					for (element *e : ts) {
						if (tab *t = dynamic_cast<tab*>(e)) {
							if (!t->request_close()) {
								all_closed = false;
							}
						}
					}
				}
			);
			if (all_closed) {
				_delete_window(*wnd);
			}
		};
		return wnd;
	}

	void tab_manager::_delete_window(window &wnd) {
		for (auto it = _wndlist.begin(); it != _wndlist.end(); ++it) {
			if (*it == &wnd) {
				_wndlist.erase(it);
				break;
			}
		}
		_manager.get_scheduler().mark_for_disposal(wnd);
	}

	void tab_manager::_delete_tab_host(host &hst) {
		logger::get().log_debug() << "tab host 0x" << &hst << " disposed";
		if (is_dragging_any_tab() && _drag_destination == &hst) {
			logger::get().log_debug() << "resetting drag destination";
			_try_detach_destination_selector();
			// TODO should we switch to dragging free properly instead of simply setting these fields?
			_drag_destination = nullptr;
			_dragging_in_host = false;
		}
		_manager.get_scheduler().mark_for_disposal(hst);
	}

	split_panel *tab_manager::_replace_with_split_panel(host &hst) {
		split_panel *sp = _manager.create_element<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
		if (f) {
			if (f->get_child1() == &hst) {
				f->set_child1(sp);
			} else {
				assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
				f->set_child2(sp);
			}
		} else {
			auto *w = dynamic_cast<window*>(hst.parent());
			assert_true_logical(w != nullptr, "root element must be a window");
			w->children().remove(hst);
			w->children().add(*sp);
		}
		return sp;
	}

	void tab_manager::_split_tabs(host &hst, std::span<tab *const> ts, orientation orient, bool newfirst) {
		for (tab *t : ts) {
			if (t->get_host() == &hst) {
				hst.remove_tab(*t);
			}
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
		for (tab *t : ts) {
			th->add_tab(*t);
		}
		sp->set_orientation(orient);
	}

	void tab_manager::_move_tabs_to_new_window(std::span<tab *const> ts, rectd layout) {
		for (tab *t : ts) {
			if (host *hst = t->get_host()) {
				hst->remove_tab(*t);
			}
		}
		window *wnd = _new_window();
		wnd->set_client_size(layout.size());
		wnd->set_position(layout.xmin_ymin());
		host *nhst = _new_tab_host();
		wnd->children().add(*nhst);
		for (tab *t : ts) {
			nhst->add_tab(*t);
		}
		wnd->show_and_activate();
	}

	void tab_manager::_start_dragging_in_host(host &h) {
		_drag_destination = &h;
		_dragging_in_host = true;
		assert_true_logical(
			_dragging_tabs[0]->get_button().parent(), "the tab should've already been added to the host"
		);
		_dragging_tabs[0]->get_window()->set_mouse_capture(_dragging_tabs[0]->get_button());
		_mouse_move_token = (
			_dragging_tabs[0]->get_button().mouse_move += [this](mouse_move_info &p) {
				_update_dragging_in_host(p);
			}
		);
	}

	void tab_manager::_update_dragging_in_host(mouse_move_info &p) {
		panel &region = _dragging_tabs[0]->get_host()->get_tab_buttons_region();
		vec2d mouse = p.new_position.get(region);
		// TODO this way of testing if the mouse is still inside can be improved
		if (!rectd::from_corners(vec2d(), region.get_layout().size()).contains(mouse)) {
			window *wnd = _dragging_tabs[0]->get_window();
			vec2d button_topleft_screen = wnd->client_to_screen(
				p.new_position.get(*wnd) + _drag_button_rect.xmin_ymin()
			);
			_exit_dragging_in_host();
			for (tab *t : _dragging_tabs) {
				_drag_destination->remove_tab(*t);
			}
			_start_dragging_free(button_topleft_screen);
			return;
		}
		_update_drag_tab_position(mouse); // update position
	}

	void tab_manager::_start_dragging_free(vec2d topleft) {
		_drag_destination = nullptr;
		_dragging_in_host = false;
		for (tab *t : _dragging_tabs) {
			_drag_tab_window->children().add(t->get_button());
		}
		_drag_tab_window->show();
		_drag_tab_window->set_mouse_capture(_dragging_tabs[0]->get_button());
		_drag_tab_window->set_position(topleft);
		_mouse_move_token = (
			_dragging_tabs[0]->get_button().mouse_move += [this](mouse_move_info &p) {
				_update_dragging_free(p);
			}
		);
	}

	void tab_manager::_update_dragging_free(mouse_move_info &p) {
		// find the tab host that the mouse is currently over
		host *target = nullptr;
		for (window *wnd : _wndlist) {
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

				for (tab *t : _dragging_tabs) {
					target->add_tab(*t);
				}
				target->activate_tab_keep_selection_and_focus(*_dragging_tabs[0]);
				_update_drag_tab_position(p.new_position.get(
					_dragging_tabs[0]->get_host()->get_tab_buttons_region()
				));

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
			p.new_position.get(*_drag_tab_window) + _drag_button_rect.xmin_ymin()
		));
	}

	void tab_manager::_stop_dragging() {
		if (_dragging_in_host) {
			_exit_dragging_in_host();
		} else {
			// these are calculated if the target is a new window
			// make the new window contain both the tab button and tab contents
			// FIXME technically this is not accurate
			rectd translated_host = rectd::bounding_box(_drag_button_rect, _drag_client_rect);
			// correct offset
			vec2d window_pos = _drag_tab_window->client_to_screen(
				translated_host.xmin_ymin() - _drag_button_rect.xmin_ymin()
			);

			_exit_dragging_free();
			drag_split_type split = drag_split_type::new_window;
			if (_drag_destination) {
				split = _drag_dest_selector->get_drag_destination();
			}
			switch (split) {
			case drag_split_type::combine:
				for (tab *t : _dragging_tabs) {
					_drag_destination->add_tab(*t);
				}
				_drag_destination->activate_tab_keep_selection_and_focus(*_dragging_tabs[0]);
				break;
			case drag_split_type::new_window:
				{
					_move_tabs_to_new_window(
						_dragging_tabs, rectd::from_corner_and_size(window_pos, translated_host.size())
					);
					break;
				}
			default: // split
				assert_true_logical(_drag_destination != nullptr, "invalid split target");
				_split_tabs(
					*_drag_destination, _dragging_tabs,
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
		_dragging_tabs[0]->get_button().mouse_up -= _stop_drag_token;
		_dragging_tabs[0]->get_button().lost_capture -= _capture_lost_token;

		std::vector<tab*> dragged_tabs = std::exchange(_dragging_tabs, std::vector<tab*>());
		_drag_destination = nullptr;
		_manager.get_scheduler().mark_for_disposal(*_drag_dest_selector);
		_drag_dest_selector = nullptr;
		drag_ended.invoke_noret(std::move(dragged_tabs));

		// TODO maybe notify the tab of mouse up
	}
}
