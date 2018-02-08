#include "docking.h"

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
			dock_manager::get().start_drag_tab(*this, p.drag_diff, get_layout().translated(diff));
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
		if (t._text_tok == _active_tab) {
			if (_tabs.size() == 1) {
				_active_tab = _tabs.end();
			} else {
				auto toact = _active_tab;
				if (++toact == _tabs.end()) {
					toact = _active_tab;
					--toact;
				}
				bool is_focused = false;
				for (element *e = manager::get().get_focused(); e; e = e->parent()) {
					if (e == this) {
						is_focused = true;
					}
				}
				if (is_focused) {
					activate_tab(**toact);
				} else {
					switch_tab(**toact);
				}
			}
		}
		_children.remove(t);
		_children.remove(*t._btn);
		_tabs.erase(t._text_tok);
		invalidate_layout();
		dock_manager::get()._on_tab_detached(*this, t);
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
		manager::get().set_focus(&t);
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

	void tab_host::_finish_layout() {
		rectd client = get_client_region();
		double x = client.xmin, y = tab_button::get_tab_button_area_height();
		for (auto i = _tabs.begin(); i != _tabs.end(); ++i) {
			double w = (*i)->_btn->get_desired_size().x;
			_child_set_layout((*i)->_btn, rectd::from_xywh(
				x + (*i)->_btn->_xoffset, client.ymin, w, y
			));
			x += w;
		}
		if (_active_tab != _tabs.end()) {
			_child_set_layout(*_active_tab, rectd(client.xmin, client.xmax, client.ymin + y, client.ymax));
		}
		if (_dsel) {
			_child_set_layout(_dsel, get_layout());
		}
		panel_base::_finish_layout();
	}

	void tab_host::_initialize() {
		panel_base::_initialize();
		dock_manager::get()._on_tab_host_created(*this);
	}


	void dock_manager::update_drag() {
		if (_drag) {
			if (_stopdrag()) {
				switch (_dtype) {
				case dock_destination_type::new_window:
					{
						rectd r = _dragrect;
						r.ymin = _dragdiff.y;
						_move_tab_to_new_window(
							*_drag, r.translated(input::get_mouse_position().convert<double>())
						);
					}
					break;
				case dock_destination_type::combine_in_tab:
					_drag->_btn->_xoffset = 0.0;
					_drag->_btn->invalidate_layout();
					break;
				case dock_destination_type::combine:
					_dest->add_tab(*_drag);
					_dest->activate_tab(*_drag);
					break;
				default:
					assert_true_logical(_dest, "invalid split target");
					_split_tab(
						*_dest, *_drag, (
							_dtype == dock_destination_type::new_panel_left ||
							_dtype == dock_destination_type::new_panel_right
							) ? ui::orientation::horizontal : ui::orientation::vertical,
						_dtype == dock_destination_type::new_panel_left ||
						_dtype == dock_destination_type::new_panel_top
					);
					break;
				}
				_try_dispose_preview();
				_try_detach_possel();
				_drag = nullptr;
				return;
			}
			vec2i mouse = input::get_mouse_position();
			if (_dtype == dock_destination_type::combine_in_tab) {
				rectd rgn = _dest->get_tab_button_region();
				vec2d mpos = _dest->get_window()->screen_to_client(mouse).convert<double>();
				if (!rgn.contains(mpos)) {
					_drag->_btn->_xoffset = 0.0f;
					_dest->remove_tab(*_drag);
					_dtype = dock_destination_type::new_window;
					_dest = nullptr;
				} else {
					_dest->move_tab_before(
						*_drag, _get_drag_tab_before(mpos.x + _dragdiff.x - rgn.xmin, rgn.width(), _drag->_btn->_xoffset)
					);
				}
			}
			vec2d minpos;
			tab_host *mindp = nullptr;
			double minsql = 0.0;
			window_base *moverwnd = nullptr;
			if (_dtype != dock_destination_type::combine_in_tab) {
				for (auto i = _hostlist.begin(); i != _hostlist.end(); ++i) {
					window_base *curw = (*i)->get_window();
					if (moverwnd && curw != moverwnd) {
						continue;
					}
					vec2d mpos = curw->screen_to_client(mouse).convert<double>();
					if (!moverwnd && curw->hit_test_full_client(mouse)) {
						moverwnd = curw;
					}
					if (moverwnd) {
						rectd rgn = (*i)->get_tab_button_region();
						if (rgn.contains(mpos)) {
							_dtype = dock_destination_type::combine_in_tab;
							_try_detach_possel();
							_dest = *i;
							_dest->add_tab(*_drag);
							_dest->activate_tab(*_drag);
							_dest->move_tab_before(
								*_drag, _get_drag_tab_before(mpos.x + _dragdiff.x - rgn.xmin, rgn.width(), _drag->_btn->_xoffset)
							);
							moverwnd->activate();
							break;
						}
					}
					if ((*i)->get_layout().contains(mpos)) {
						vec2d cdiff = mpos - (*i)->get_layout().center();
						double dsql = cdiff.length_sqr();
						if (!mindp || minsql > dsql) {
							minpos = mpos;
							mindp = *i;
							minsql = dsql;
						}
					}
				}
			}
			if (_dtype != dock_destination_type::combine_in_tab) {
				if (mindp) {
					if (_dest != mindp) {
						if (_dest) {
							_dest->_set_dock_pos_selector(nullptr);
						}
						mindp->_set_dock_pos_selector(_possel);
					}
					dock_destination_type newdtype = _possel->get_dock_destination(minpos);
					if (newdtype != _dtype || _dest != mindp) {
						_try_dispose_preview();
						_dtype = newdtype;
						_dest = mindp;
						if (_dtype != dock_destination_type::new_window) {
							_dragdec = _dest->get_window()->create_decoration();
							_initialize_preview();
						}
					}
				} else {
					_try_dispose_preview();
					_try_detach_possel();
					_dest = nullptr;
					_dtype = dock_destination_type::new_window;
				}
			} else {
				_try_dispose_preview();
				_try_detach_possel();
			}
		}
	}
}
