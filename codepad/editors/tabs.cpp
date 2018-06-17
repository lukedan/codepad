#include "tabs.h"

/// \file
/// Implementation of certain tab-related functions.

using namespace codepad::ui;
using namespace codepad::os;

namespace codepad::editor {
	void tab::_initialize() {
		panel::_initialize();
		_can_focus = true;
		_btn = element::create<tab_button>();
		_btn->click += [this](tab_button_click_info &info) {
			get_host()->activate_tab(*this);
			info.click_info.mark_focus_set();
		};
		_btn->request_close += [this]() {
			_on_close_requested();
		};
		_btn->start_drag += [this](tab_drag_info &p) {
			vec2d diff = p.drag_diff - vec2d(get_layout().xmin, _btn->get_layout().ymin);
			tab_manager::get().start_drag_tab(*this, p.drag_diff, get_layout().translated(diff));
		};
	}


	void tab_host::add_tab(tab &t) {
		t._text_tok = _tabs.insert(_tabs.end(), &t);
		_children.add(t);
		_children.add(*t._btn);
		t.set_visibility(visibility::ignored);
		if (_tabs.size() == 1) {
			switch_tab(t);
		}
		invalidate_layout();
	}

	void tab_host::remove_tab(tab &t) {
		_children.remove(t);
	}

	void tab_host::_on_child_removing(element *elem) {
		tab *t = dynamic_cast<tab*>(elem);
		if (t != nullptr) {
			if (t->_text_tok == _active_tab) { // change active tab
				if (_tabs.size() == 1) {
					_active_tab = _tabs.end();
				} else {
					auto toact = _active_tab;
					if (++toact == _tabs.end()) {
						toact = _active_tab;
						--toact;
					}
					bool is_focused = false;
					os::window_base *wnd = get_window();
					for (element *e = wnd->get_window_focused_element(); e; e = e->parent()) {
						if (e == this) {
							is_focused = true;
						}
					}
					switch_tab(**toact);
					if (is_focused) {
						wnd->set_window_focused_element(**toact);
					}
				}
			}
			_children.remove(*t->_btn);
			_tabs.erase(t->_text_tok);
			tab_manager::get()._on_tab_detached(*this, *t);
			invalidate_layout();
		}
	}

	void tab_host::switch_tab(tab &t) {
		assert_true_logical(t._parent == this, "corrupted element tree");
		if (_active_tab != _tabs.end()) {
			(*_active_tab)->set_visibility(visibility::ignored);
			(*_active_tab)->_btn->set_zindex(0);
		}
		_active_tab = t._text_tok;
		t.set_visibility(visibility::visible);
		t._btn->set_zindex(1);
		invalidate_layout();
	}
	void tab_host::activate_tab(tab &t) {
		switch_tab(t);
		manager::get().set_focused_element(t);
	}

	size_t tab_host::get_tab_position(tab &tb) const {
		assert_true_logical(tb.parent() == this, "corrupted element tree");
		size_t d = 0;
		for (auto i = _tabs.begin(); i != _tabs.end(); ++i, ++d) {
			if (*i == &tb) {
				return d;
			}
		}
		assert_true_logical(false, "corrupted element tree");
		return 0;
	}

	tab &tab_host::get_tab_at(size_t pos) const {
		auto v = _tabs.begin();
		for (size_t i = 0; i < pos; ++i, ++v) {
		}
		return **v;
	}

	void tab_host::move_tab_before(tab &target, tab *before) {
		bool setactive = &target == *_active_tab;
		if (setactive) {
			_active_tab = _tabs.end();
		}
		_tabs.erase(target._text_tok);
		target._text_tok = _tabs.insert(before ? before->_text_tok : _tabs.end(), &target);
		if (setactive) {
			_active_tab = target._text_tok;
		}
		invalidate_layout();
	}

	/// \todo Use stack panel-like layout for tab buttons.
	void tab_host::_finish_layout() {
		rectd client = get_client_region();
		double x = client.xmin, y = tab_button::get_tab_button_area_height();
		for (auto i = _tabs.begin(); i != _tabs.end(); ++i) {
			double w = (*i)->_btn->get_desired_width().first;
			_child_set_layout((*i)->_btn, rectd::from_xywh(
				x + (*i)->_btn->_xoffset, client.ymin, w, y
			));
			x += w;
		}
		if (_active_tab != _tabs.end()) { // make active tab visible
			_child_set_layout(*_active_tab, rectd(client.xmin, client.xmax, client.ymin + y, client.ymax));
		}
		if (_dsel) {
			_child_set_layout(_dsel, get_layout());
		}
		panel_base::_finish_layout();
	}


	/// \todo Add proper literal for drag destination previews.
	void tab_manager::update_drag() {
		if (_drag != nullptr) {
			vec2i mouse = input::get_mouse_position();
			if (_dtype == drag_destination_type::combine_in_tab) { // dragging tab_button in a tab list
				rectd rgn = _dest->get_tab_button_region();
				vec2d mpos = _dest->get_window()->screen_to_client(mouse).convert<double>();
				if (!rgn.contains(mpos)) { // moved out of the region
					_drag->_btn->_xoffset = 0.0f;
					_dest->remove_tab(*_drag);
					_dtype = drag_destination_type::new_window;
					_dest = nullptr;
				} else { // update tab position
					_update_drag_tab_position(mpos.x - rgn.xmin, rgn.width());
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
							rectd rgn = hst.get_tab_button_region();
							if (rgn.contains(mpos)) { // switch to combine_in_tab
								_dtype = drag_destination_type::combine_in_tab;
								_try_dispose_preview();
								_try_detach_possel();
								// change destination and add _drag to it
								_dest = &hst;
								_dest->add_tab(*_drag);
								_dest->activate_tab(*_drag);
								// update position
								_update_drag_tab_position(mpos.x - rgn.xmin, rgn.width());
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
							_dragdec = new decoration();
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
				bool mouseover = false; // whether mouse_over visual bit should be set
				switch (_dtype) {
				case drag_destination_type::new_window:
					{
						rectd r = _dragrect;
						r.ymin = _dragdiff.y;
						_move_tab_to_new_window(
							*_drag, r.translated(input::get_mouse_position().convert<double>())
						);
					}
					break;
				case drag_destination_type::combine_in_tab:
					_drag->_btn->_xoffset = 0.0; // reset offset
					_drag->_btn->invalidate_layout();
					mouseover = true;
					// the tab is already added to _dest
					break;
				case drag_destination_type::combine:
					_dest->add_tab(*_drag);
					_dest->activate_tab(*_drag);
					break;
				default: // split tab
					assert_true_logical(_dest != nullptr, "invalid split target");
					_split_tab(
						*_dest, *_drag, (
							_dtype == drag_destination_type::new_panel_left ||
							_dtype == drag_destination_type::new_panel_right
							) ? ui::orientation::horizontal : ui::orientation::vertical,
						_dtype == drag_destination_type::new_panel_left ||
						_dtype == drag_destination_type::new_panel_top
					);
					break;
				}
				// dispose preview and detach selector
				_try_dispose_preview();
				_try_detach_possel();
				// the mouse button is not down anymore
				_drag->_btn->_set_visual_style_bit(visual::get_predefined_states().mouse_down, false);
				// set mouse over bit accordingly, although it works (almost) fine without this
				_drag->_btn->_set_visual_style_bit(visual::get_predefined_states().mouse_over, mouseover);
				_drag = nullptr;
			}
		}
	}
}
