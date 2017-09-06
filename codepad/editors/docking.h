#pragma once

#include "../ui/element.h"
#include "../ui/panel.h"
#include "../ui/commonelements.h"
#include "../ui/manager.h"
#include "../os/misc.h"
#include "../os/renderer.h"
#include "../os/window.h"
#include "../os/current.h"

namespace codepad {
	namespace editor {
		class draggable_separator : public ui::element {
			friend class ui::element;
		public:
			constexpr static double default_thickness = 5.0;

			void set_position(double v) {
				double ov = _posv;
				_posv = clamp(v, _minv, _maxv);
				if (_orient == ui::orientation::horizontal) {
					set_margin(ui::thickness(_posv, 0.0, 1.0 - _posv, 0.0));
				} else {
					set_margin(ui::thickness(0.0, _posv, 0.0, 1.0 - _posv));
				}
				value_changed.invoke_noret(ov);
			}
			double get_position() const {
				return _posv;
			}

			double get_range_min() const {
				return _minv;
			}
			double get_range_max() const {
				return _maxv;
			}
			void set_range(double rmin, double rmax) {
				_minv = rmin;
				_maxv = rmax;
				if (_posv < _minv || _posv > _maxv) {
					set_position(_posv);
				}
			}

			void set_orientation(ui::orientation ori) {
				_orient = ori;
				_on_orient_changed();
			}
			ui::orientation get_orientation() const {
				return _orient;
			}

			ui::cursor get_default_cursor() const override {
				return _orient == ui::orientation::horizontal ? ui::cursor::arrow_east_west : ui::cursor::arrow_north_south;
			}

			vec2d get_desired_size() const override {
				return vec2d(default_thickness, default_thickness);
			}

			rectd get_region1() const {
				rectd plo = _parent->get_client_region();
				return
					_orient == ui::orientation::horizontal ?
					rectd(plo.xmin, _layout.xmin, plo.ymin, plo.ymax) :
					rectd(plo.xmin, plo.xmax, plo.ymin, _layout.ymin);
			}
			rectd get_region2() const {
				rectd plo = _parent->get_client_region();
				return
					_orient == ui::orientation::horizontal ?
					rectd(_layout.xmax, plo.xmax, plo.ymin, plo.ymax) :
					rectd(plo.xmin, plo.xmax, _layout.ymax, plo.ymax);
			}

			event<value_update_info<double>> value_changed;
			event<void> start_drag, stop_drag;

			inline static str_t get_default_class() {
				return U"draggable_separator";
			}
		protected:
			ui::orientation _orient = ui::orientation::horizontal;
			double _posv = 0.5, _minv = 0.0, _maxv = 1.0;
			bool _drag = false;

			void _on_orient_changed() {
				if (_orient == ui::orientation::horizontal) {
					set_anchor(ui::anchor::stretch_vertically);
					set_margin(ui::thickness(_posv, 0.0, 1.0 - _posv, 0.0));
				} else {
					set_anchor(ui::anchor::stretch_horizontally);
					set_margin(ui::thickness(0.0, _posv, 0.0, 1.0 - _posv));
				}
			}

			void _on_mouse_down(ui::mouse_button_info &p) override {
				if (p.button == os::input::mouse_button::left) {
					start_drag.invoke();
					_drag = true;
					get_window()->set_mouse_capture(*this);
				}
				element::_on_mouse_down(p);
			}
			void _on_mouse_move(ui::mouse_move_info &p) override {
				if (_drag) {
					set_position(
						_orient == ui::orientation::horizontal ?
						(p.new_pos.x - _parent->get_layout().xmin - 0.5 * default_thickness) /
						(_parent->get_layout().width() - default_thickness) :
						(p.new_pos.y - _parent->get_layout().ymin - 0.5 * default_thickness) /
						(_parent->get_layout().height() - default_thickness)
					);
				}
			}
			void _on_end_drag() {
				_drag = false;
				get_window()->release_mouse_capture();
			}
			void _on_capture_lost() override {
				_on_end_drag();
			}
			void _on_mouse_up(ui::mouse_button_info &p) override {
				if (_drag && p.button == os::input::mouse_button::left) {
					_on_end_drag();
				}
			}

			void _initialize() override {
				ui::element::_initialize();
				_can_focus = false;
				_on_orient_changed();
			}
		};
		class split_panel : public ui::panel_base {
		public:
			constexpr static double minimum_panel_size = 30.0;

			void set_child1(ui::element *elem) {
				_change_child(_c1, elem);
			}
			ui::element *get_child1() const {
				return _c1;
			}
			void set_child2(ui::element *elem) {
				_change_child(_c2, elem);
			}
			ui::element *get_child2() const {
				return _c2;
			}

			void set_orientation(ui::orientation ori) {
				_sep->set_orientation(ori);
			}
			ui::orientation get_orientation() const {
				return _sep->get_orientation();
			}

			bool override_children_layout() const override {
				return true;
			}

			inline static str_t get_default_class() {
				return U"split_panel";
			}
		protected:
			ui::element *_c1 = nullptr, *_c2 = nullptr;
			draggable_separator *_sep = nullptr;
			bool _passivepos = false;

			void _maintain_separator_position(bool minchanged, double totv, double oldv, double newv) {
				double newpos, sw;
				if (_sep->get_orientation() == ui::orientation::horizontal) {
					sw = _sep->get_actual_size().x;
				} else {
					sw = _sep->get_actual_size().y;
				}
				if (minchanged) { // (totv * (1 - oldv) - sw) * (1 - oldmv) = (totv * (1 - newv) - sw) * (1 - newmv)
					double
						ototw = totv * (1.0 - oldv) - sw, ntotw = totv * (1.0 - newv) - sw,
						oldpos = _sep->get_position(), fixw = ototw * (1.0 - oldpos), ofw = totv - sw - fixw;
					newpos = 1.0 - fixw / ntotw;
					split_panel *sp = dynamic_cast<split_panel*>(_c1);
					if (sp && sp->get_orientation() == _sep->get_orientation()) {
						sp->_maintain_separator_position(minchanged, ofw, totv * oldv / ofw, totv * newv / ofw);
					}
				} else { // (totv * oldv - sw) * oldmv = (totv * newv - sw) * newmv
					double
						ototw = totv * oldv - sw, ntotw = totv * newv - sw,
						oldpos = _sep->get_position(), fixw = ototw * oldpos, ofw = totv - sw - fixw;
					newpos = fixw / ntotw;
					split_panel *sp = dynamic_cast<split_panel*>(_c2);
					if (sp && sp->get_orientation() == _sep->get_orientation()) {
						sp->_maintain_separator_position(
							minchanged, ofw,
							ototw * (1.0 - oldpos) / ofw,
							ntotw * (1.0 - newpos) / ofw
						);
					}
				}
				_passivepos = true;
				_sep->set_position(newpos);
				_passivepos = false;
			}
			void _reset_separator_range() {
				element *c1 = _c1, *c2 = _c2;
				for (
					split_panel *next = dynamic_cast<split_panel*>(c1);
					next;
					c1 = next->get_child2(), next = dynamic_cast<split_panel*>(c1)
					) {
				}
				for (
					split_panel *next = dynamic_cast<split_panel*>(c2);
					next;
					c2 = next->get_child1(), next = dynamic_cast<split_panel*>(c2)
					) {
				}
				double minv, maxv, lmin, lw;
				if (_sep->get_orientation() == ui::orientation::horizontal) {
					minv = c1->get_layout().xmin;
					maxv = c2->get_layout().xmax;
					lmin = get_layout().xmin;
					lw = get_layout().width();
				} else {
					minv = c1->get_layout().ymin;
					maxv = c2->get_layout().ymax;
					lmin = get_layout().ymin;
					lw = get_layout().height();
				}
				minv += minimum_panel_size;
				maxv -= minimum_panel_size;
				if (minv > maxv) {
					minv = maxv = 0.5 * (minv + maxv);
				}
				_sep->set_range((minv - lmin) / lw, (maxv - lmin) / lw);
			}

			void _change_child(ui::element *&e, ui::element *newv) {
				if (e) {
					_children.remove(*e);
				}
				e = newv;
				if (e) {
					_children.add(*e);
				}
			}
			void _on_remove_child(element *e) override {
				if (e == _c1) {
					_c1 = nullptr;
				} else if (e == _c2) {
					_c2 = nullptr;
				}
			}

			void _custom_render() const override {
				_child_on_render(_sep);
				if (_c1) {
					os::renderer_base::get().push_clip(_sep->get_region1().minimum_bounding_box<int>());
					_child_on_render(_c1);
					os::renderer_base::get().pop_clip();
				}
				if (_c2) {
					os::renderer_base::get().push_clip(_sep->get_region2().minimum_bounding_box<int>());
					_child_on_render(_c2);
					os::renderer_base::get().pop_clip();
				}
			}

			void _finish_layout() override {
				_child_recalc_layout(_sep, get_client_region());
				if (_c1) {
					_child_recalc_layout(_c1, _sep->get_region1());
				}
				if (_c2) {
					_child_recalc_layout(_c2, _sep->get_region2());
				}
				ui::panel_base::_finish_layout();
			}

			void _initialize() override {
				ui::panel_base::_initialize();
				_sep = ui::element::create<draggable_separator>();
				_sep->value_changed += [this](value_update_info<double> &p) {
					if (!_passivepos) {
						double totw =
							_sep->get_orientation() == ui::orientation::horizontal ?
							get_layout().width() - _sep->get_actual_size().x :
							get_layout().height() - _sep->get_actual_size().y;
						split_panel *sp = dynamic_cast<split_panel*>(_c1);
						if (sp && sp->get_orientation() == _sep->get_orientation()) {
							sp->_maintain_separator_position(false, totw, p.old_value, _sep->get_position());
						}
						sp = dynamic_cast<split_panel*>(_c2);
						if (sp && sp->get_orientation() == _sep->get_orientation()) {
							sp->_maintain_separator_position(true, totw, p.old_value, _sep->get_position());
						}
					}
					invalidate_layout();
				};
				_sep->start_drag += [this]() {
					_reset_separator_range();
				};
				_sep->stop_drag += [this]() {
					_sep->set_range(0.0, 1.0);
				};
				_children.add(*_sep);
			}
		};

		struct tab_drag_info {
			tab_drag_info(vec2d df) : drag_diff(df) {
			}
			const vec2d drag_diff;
		};
		class tab_button : public ui::panel_base {
			friend class ui::element;
			friend class dock_manager;
			friend class tab_host;
		public:
			constexpr static double drag_pivot = 5.0;
			constexpr static ui::thickness content_padding = ui::thickness(5.0);

			void set_text(str_t str) {
				_content.set_text(std::move(str));
			}
			const str_t &get_text() const {
				return _content.get_text();
			}

			vec2d get_desired_size() const override {
				vec2d sz = _content.get_text_size() + _padding.size();
				sz.x += sz.y;
				return sz;
			}

			inline static double get_tab_button_area_height() {
				return ui::content_host::get_default_font()->height() + tab_button::content_padding.height();
			}

			event<void> click, request_close;
			event<tab_drag_info> start_drag;

			inline static str_t get_default_class() {
				return U"tab_button";
			}
		protected:
			ui::content_host _content = *this;
			ui::button *_btn;
			vec2d _mdpos;
			double _xoffset = 0.0;

			void _on_mouse_down(ui::mouse_button_info &p) override {
				panel_base::_on_mouse_down(p);
				if (
					p.button == os::input::mouse_button::left &&
					!test_bit_all(static_cast<unsigned>(_btn->get_state()), ui::button_base::state::mouse_over)
					) {
					_mdpos = p.position;
					ui::manager::get().schedule_update(*this);
					click.invoke();
				} else if (p.button == os::input::mouse_button::middle) {
					request_close.invoke();
				}
			}

			void _on_update() override {
				if (os::input::is_mouse_button_down(os::input::mouse_button::left)) {
					vec2d diff = get_window()->screen_to_client(os::input::get_mouse_position()).convert<double>() - _mdpos;
					if (diff.length_sqr() > drag_pivot * drag_pivot) {
						start_drag.invoke_noret(get_layout().xmin_ymin() - _mdpos);
					} else {
						ui::manager::get().schedule_update(*this);
					}
				}
			}
			void _custom_render() const override {
				_content.render();
				panel_base::_custom_render();
			}

			void _finish_layout() override {
				_btn->set_width(get_layout().height() - get_padding().height());
				panel_base::_finish_layout();
			}

			void _initialize() override {
				panel_base::_initialize();
				_btn = ui::element::create<ui::button>();
				_btn->set_anchor(ui::anchor::dock_right);
				_btn->set_can_focus(false);
				_btn->click += [this]() {
					request_close.invoke();
				};
				_children.add(*_btn);
				_padding = content_padding;
				_can_focus = false;
			}
		};

		class tab;
		class tab_host : public ui::panel_base {
			friend class tab;
			friend class dock_manager;
		public:
			bool override_children_layout() const override {
				return true;
			}

			void add_tab(tab&);
			void remove_tab(tab&);

			void activate_tab(tab&);

			size_t get_tab_position(tab&) const;
			tab &get_tab_at(size_t) const;
			void move_tab_before(tab&, tab*);

			rectd get_tab_button_region() const {
				return rectd(
					_layout.xmin, _layout.xmax, _layout.ymin,
					_layout.ymin + ui::content_host::get_default_font()->height() + tab_button::content_padding.height()
				);
			}

			size_t tab_count() const {
				return _tabs.size();
			}

			inline static str_t get_default_class() {
				return U"tab_host";
			}
		protected:
			std::list<tab*> _tabs;
			std::list<tab*>::iterator _active_tab = _tabs.end();
			std::list<tab_host*>::iterator _text_tok;

			void _finish_layout() override;

			void _initialize() override;
		};
		class tab : public ui::panel {
			friend class tab_host;
			friend class dock_manager;
		public:
			void set_caption(str_t s) {
				_btn->set_text(std::move(s));
			}
			const str_t &get_caption() const {
				return _btn->get_text();
			}

			void activate() {
				get_host()->activate_tab(*this);
			}

			tab_host *get_host() const {
#ifdef CP_DETECT_LOGICAL_ERRORS
				tab_host *hst = dynamic_cast<tab_host*>(parent());
				assert_true_logical(hst, "parent is not a tab host when get_host() is called");
				return hst;
#else
				return static_cast<tab_host*>(parent());
#endif
			}

			inline static str_t get_default_class() {
				return U"tab";
			}
		protected:
			tab_button *_btn;
			std::list<tab*>::iterator _text_tok;

			virtual void _on_request_close() {
				get_host()->remove_tab(*this);
				ui::manager::get().mark_disposal(*this);
			}

			void _initialize() override;
			void _dispose() override {
				ui::manager::get().mark_disposal(*_btn);
				ui::panel::_dispose();
			}
		};

		class dock_manager {
			friend class tab;
			friend class tab_host;
		public:
			tab_host *get_focused_tab_host() const {
				ui::element *focus = ui::manager::get().get_focused();
				tab_host *host = nullptr;
				if (focus) {
					for (
						host = dynamic_cast<tab_host*>(focus);
						focus && !host;
						focus = focus->parent(), host = dynamic_cast<tab_host*>(focus)
						) {
					}
				}
				return host;
			}
			tab *new_tab() {
				return new_tab_in(_lasthost);
			}
			tab *new_tab_in(tab_host *host = nullptr) {
				if (!host) {
					host = ui::element::create<tab_host>();
					_new_window()->children().add(*host);
				}
				tab *t = ui::element::create<tab>();
				host->add_tab(*t);
				return t;
			}

			size_t window_count() const {
				return _wndcnt;
			}

			bool empty() const {
				return window_count() == 0 && _drag == nullptr;
			}

			void update_changed_hosts() {
				for (auto i = _changed.begin(); i != _changed.end(); ++i) {
					if ((*i)->tab_count() == 0) {
						_on_tab_host_disposed(**i);
						split_panel *father = dynamic_cast<split_panel*>((*i)->parent());
						if (father) {
							ui::element *other = (*i) == father->get_child1() ? father->get_child2() : father->get_child1();
							father->set_child1(nullptr);
							father->set_child2(nullptr);
							split_panel *ff = dynamic_cast<split_panel*>(father->parent());
							if (ff) {
								if (father == ff->get_child1()) {
									ff->set_child1(other);
								} else {
									assert_true_logical(father == ff->get_child2(), "corrupted element graph");
									ff->set_child2(other);
								}
							} else {
#ifdef CP_DETECT_LOGICAL_ERRORS
								os::window_base *f = dynamic_cast<os::window_base*>(father->parent());
								assert_true_logical(f, "parent of parent must be a window or a split panel");
#else
								os::window_base *f = static_cast<os::window_base*>(father->parent());
#endif
								f->children().remove(*father);
								f->children().add(*other);
							}
							ui::manager::get().mark_disposal(*father);
						} else {
#ifdef CP_DETECT_LOGICAL_ERRORS
							os::window_base *f = dynamic_cast<os::window_base*>((*i)->parent());
							assert_true_logical(f, "parent must be a window or a split panel");
#else
							os::window_base *f = static_cast<os::window_base*>((*i)->parent());
#endif
							ui::manager::get().mark_disposal(*f);
							--_wndcnt;
						}
						ui::manager::get().mark_disposal(**i);
					}
				}
				_changed.clear();
			}
			void update_drag() {
				if (_drag) {
					if (_stopdrag()) {
						switch (_dtype) {
						case _drag_dest_type::new_wnd:
							{
								os::window_base *wnd = _new_window();
								tab_host *nhst = ui::element::create<tab_host>();
								wnd->children().add(*nhst);
								nhst->add_tab(*_drag);
								wnd->set_client_size(vec2d(_dragrect.width(), _dragrect.ymax - _dragdiff.y).convert<int>());
								wnd->set_position(os::input::get_mouse_position() + _dragdiff.convert<int>());
							}
							break;
						case _drag_dest_type::combine_intab:
							_drag->_btn->_xoffset = 0.0;
							_drag->_btn->invalidate_layout();
							break;
						case _drag_dest_type::combine:
							_dest->add_tab(*_drag);
							_dest->activate_tab(*_drag);
							break;
						default:
							{
								assert_true_logical(_dest, "invalid split target");
								split_panel *sp = _replace_with_split_panel(*_dest);
								tab_host *th = ui::element::create<tab_host>();
								_hostlist.erase(th->_text_tok);
								th->_text_tok = _hostlist.insert(_dest->_text_tok, th);
								if (_dtype == _drag_dest_type::new_pnl_l || _dtype == _drag_dest_type::new_pnl_u) {
									sp->set_child1(th);
									sp->set_child2(_dest);
								} else {
									sp->set_child1(_dest);
									sp->set_child2(th);
								}
								th->add_tab(*_drag);
								sp->set_orientation(
									_dtype == _drag_dest_type::new_pnl_l || _dtype == _drag_dest_type::new_pnl_r ?
									ui::orientation::horizontal :
									ui::orientation::vertical
								);
							}
							break;
						}
						_drag = nullptr;
						return;
					}
					vec2i mouse = os::input::get_mouse_position();
					if (_dtype == _drag_dest_type::combine_intab) {
						rectd rgn = _dest->get_tab_button_region();
						vec2d mpos = _dest->get_window()->screen_to_client(mouse).convert<double>();
						if (!rgn.contains(mpos)) {
							_drag->_btn->_xoffset = 0.0f;
							_dest->remove_tab(*_drag);
							_dtype = _drag_dest_type::new_wnd;
							_dest = nullptr;
						} else {
							_dest->move_tab_before(
								*_drag, _get_drag_tab_before(mpos.x + _dragdiff.x - rgn.xmin, rgn.width(), _drag->_btn->_xoffset)
							);
						}
					}
					vec2d ddiff;
					tab_host *mindp = nullptr;
					double minsql = 0.0;
					os::window_base *moverwnd = nullptr;
					if (_dtype != _drag_dest_type::combine_intab) {
						for (auto i = _hostlist.begin(); i != _hostlist.end(); ++i) {
							os::window_base *curw = (*i)->get_window();
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
									_dtype = _drag_dest_type::combine_intab;
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
									ddiff = cdiff;
									mindp = *i;
									minsql = dsql;
								}
							}
						}
					}
					if (_dtype != _drag_dest_type::combine_intab) {
						if (mindp) {
							_dest = mindp;
							if (std::abs(ddiff.x) > std::abs(ddiff.y)) {
								_dtype = ddiff.x > 0.0 ? _drag_dest_type::new_pnl_r : _drag_dest_type::new_pnl_l;
							} else {
								_dtype = ddiff.y > 0.0 ? _drag_dest_type::new_pnl_d : _drag_dest_type::new_pnl_u;
							}
						} else {
							_dtype = _drag_dest_type::new_wnd;
						}
					}
					// TODO preview
				}
			}
			void update() {
				update_changed_hosts();
				update_drag();
			}

			void start_drag_tab(tab &t, vec2d diff, rectd layout, std::function<bool()> stop = []() {
				return !os::input::is_mouse_button_down(os::input::mouse_button::left);
			}) {
				assert_true_usage(_drag == nullptr, "a tab is currently being dragged");
				tab_host *hst = t.get_host();
				if (hst) {
					_dest = hst;
					_dtype = _drag_dest_type::combine_intab;
				} else {
					_dest = nullptr;
					_dtype = _drag_dest_type::new_wnd;
				}
				_drag = &t;
				_dragdiff = diff;
				_dragrect = layout;
				_stopdrag = stop;
			}

			static dock_manager &get();
		protected:
			enum class _drag_dest_type {
				new_wnd,
				combine_intab,
				combine,
				new_pnl_l,
				new_pnl_u,
				new_pnl_r,
				new_pnl_d
			};
			size_t _wndcnt = 0;
			std::set<tab_host*> _changed;
			std::list<tab_host*> _hostlist;
			tab_host *_lasthost = nullptr;
			// drag related stuff
			tab *_drag = nullptr;
			tab_host *_dest = nullptr;
			_drag_dest_type _dtype = _drag_dest_type::new_wnd;
			vec2d _dragdiff;
			rectd _dragrect;
			std::function<bool()> _stopdrag;

			os::window_base *_new_window() {
				os::window_base *wnd = ui::element::create<os::window>();
				wnd->got_window_focus += [this, wnd]() {
					_lasthost = get_focused_tab_host();
					_enumerate_hosts(wnd, [this](tab_host &hst) {
						if (!_lasthost) {
							_lasthost = &hst;
						}
						_hostlist.erase(hst._text_tok);
						hst._text_tok = _hostlist.insert(_hostlist.begin(), &hst);
					});
				};
				wnd->close_request += [wnd]() {
					_enumerate_hosts(wnd, [](tab_host &hst) {
						std::vector<tab*> ts;
						for (auto i = hst._tabs.begin(); i != hst._tabs.end(); ++i) {
							ts.push_back(*i);
						}
						for (auto i = ts.begin(); i != ts.end(); ++i) {
							(*i)->_on_request_close();
						}
					});
				};
				++_wndcnt;
				return wnd;
			}

			split_panel *_replace_with_split_panel(tab_host &hst) {
				split_panel *sp = ui::element::create<split_panel>(), *f = dynamic_cast<split_panel*>(hst.parent());
				if (f) {
					if (f->get_child1() == &hst) {
						f->set_child1(sp);
					} else {
						assert_true_logical(f->get_child2() == &hst, "corrupted element tree");
						f->set_child2(sp);
					}
				} else {
					os::window_base *w = dynamic_cast<os::window_base*>(hst.parent());
					assert_true_logical(w, "root element must be a window");
					w->children().remove(hst);
					w->children().add(*sp);
				}
				return sp;
			}

			void _on_tab_detached(tab_host &host, tab&) {
				_changed.insert(&host);
			}

			template <typename T> inline static void _enumerate_hosts(os::window_base *base, const T &cb) {
				assert_true_logical(base->children().size() == 1, "window must have only one child");
				std::vector<ui::element*> hsts;
				hsts.push_back(*base->children().begin());
				while (!hsts.empty()) {
					ui::element *ce = hsts.back();
					hsts.pop_back();
					tab_host *hst = dynamic_cast<tab_host*>(ce);
					if (hst) {
						cb(*hst);
					} else {
						split_panel *sp = dynamic_cast<split_panel*>(ce);
						assert_true_logical(sp, "corrupted element tree");
						hsts.push_back(sp->get_child1());
						hsts.push_back(sp->get_child2());
					}
				}
			}

			tab *_get_drag_tab_before(double pos, double maxw, double &offset) const {
				double halfw = 0.5 * _drag->_btn->get_desired_size().x, posx = pos + halfw, cx = halfw;
				tab *res = nullptr;
				for (auto i = _dest->_tabs.begin(); i != _dest->_tabs.end(); ++i) {
					if (*i != _drag) {
						double thisw = (*i)->_btn->get_desired_size().x;
						if (posx < cx + 0.5 * thisw) {
							res = *i;
							break;
						}
						cx += thisw;
					}
				}
				offset = clamp(posx, halfw, maxw - halfw) - cx;
				return res;
			}

			void _on_tab_host_created(tab_host &hst) {
				logger::get().log_info(CP_HERE, "tab host 0x", &hst, " created");
				hst._text_tok = _hostlist.insert(_hostlist.begin(), &hst);
				_lasthost = &hst;
			}
			void _on_tab_host_disposed(tab_host &hst) {
				logger::get().log_info(CP_HERE, "tab host 0x", &hst, " disposed");
				if (_drag && _dest == &hst) {
					logger::get().log_info(CP_HERE, "resetting drag destination");
					_dest = nullptr;
					_dtype = _drag_dest_type::new_wnd;
				}
				_hostlist.erase(hst._text_tok);
			}
		};

		inline void tab::_initialize() {
			ui::panel::_initialize();
			_btn = ui::element::create<tab_button>();
			_btn->click += [this]() {
				get_host()->activate_tab(*this);
				ui::manager::get().set_focus(this);
			};
			_btn->request_close += [this]() {
				_on_request_close();
			};
			_btn->start_drag += [this](tab_drag_info &p) {
				vec2d diff = p.drag_diff - vec2d(get_layout().xmin, _btn->get_layout().ymin);
				dock_manager::get().start_drag_tab(*this, p.drag_diff, get_layout().translated(diff));
			};
		}

		inline void tab_host::add_tab(tab &t) {
			t._text_tok = _tabs.insert(_tabs.end(), &t);
			_children.add(t);
			_children.add(*t._btn);
			t.set_visibility(ui::visibility::ignored);
			if (_tabs.size() == 1) {
				activate_tab(t);
			}
			invalidate_layout();
		}
		inline void tab_host::remove_tab(tab &t) {
			if (t._text_tok == _active_tab) {
				if (_tabs.size() == 1) {
					_active_tab = _tabs.end();
				} else {
					auto next = _active_tab;
					if (++next == _tabs.end()) {
						auto prev = _active_tab;
						--prev;
						activate_tab(**prev);
					} else {
						activate_tab(**next);
					}
				}
			}
			_children.remove(t);
			_children.remove(*t._btn);
			_tabs.erase(t._text_tok);
			invalidate_layout();
			dock_manager::get()._on_tab_detached(*this, t);
		}
		inline void tab_host::activate_tab(tab &t) {
			assert_true_logical(t._parent == this, "corrupted element tree");
			if (_active_tab != _tabs.end()) {
				(*_active_tab)->set_visibility(ui::visibility::ignored);
			}
			_active_tab = t._text_tok;
			t.set_visibility(ui::visibility::visible);
			invalidate_layout();
		}
		inline size_t tab_host::get_tab_position(tab &tb) const {
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
		inline tab &tab_host::get_tab_at(size_t pos) const {
			auto v = _tabs.begin();
			for (size_t i = 0; i < pos; ++i, ++v) {
			}
			return **v;
		}
		inline void tab_host::move_tab_before(tab &target, tab *before) {
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
		inline void tab_host::_finish_layout() {
			rectd client = get_client_region();
			double x = client.xmin, y = tab_button::get_tab_button_area_height();
			for (auto i = _tabs.begin(); i != _tabs.end(); ++i) {
				double w = (*i)->_btn->get_desired_size().x;
				_child_set_layout((*i)->_btn, rectd(
					x + (*i)->_btn->_xoffset, x + w + (*i)->_btn->_xoffset, client.ymin, client.ymin + y
				));
				x += w;
			}
			if (_active_tab != _tabs.end()) {
				_child_set_layout(*_active_tab, rectd(client.xmin, client.xmax, client.ymin + y, client.ymax));
			}
			ui::panel_base::_finish_layout();
		}
		inline void tab_host::_initialize() {
			panel_base::_initialize();
			dock_manager::get()._on_tab_host_created(*this);
		}
	}
}
