#pragma once

#include "../ui/element.h"
#include "../ui/panel.h"
#include "../ui/commonelements.h"
#include "../ui/manager.h"
#include "../platform/input.h"
#include "../platform/renderer.h"
#include "../platform/window.h"
#include "../platform/current.h"

namespace codepad {
	namespace editor {
		class draggable_separator : public ui::element {
			friend class ui::element;
		public:
			constexpr static double default_thickness = 5.0;

			void set_position(double v) {
				_posv = v;
				_on_pos_changed();
			}
			double get_position() const {
				return _posv;
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

			event<void_info> value_changed;
		protected:
			ui::orientation _orient = ui::orientation::horizontal;
			double _posv = 0.5;

			void _on_pos_changed() {
				value_changed.invoke_noret();
				if (_orient == ui::orientation::horizontal) {
					set_margin(ui::thickness(_posv, 0.0, 1.0 - _posv, 0.0));
				} else {
					set_margin(ui::thickness(0.0, _posv, 0.0, 1.0 - _posv));
				}
			}
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
				if (p.button == platform::input::mouse_button::left) {
					ui::manager::default().schedule_update(this);
				}
				element::_on_mouse_down(p);
			}

			void _render() const override { // TODO change appearance
				rectd lo = get_layout();
				vec2d vs[6] = {
					lo.xmin_ymin(), lo.xmax_ymin(), lo.xmin_ymax(), lo.xmax_ymin(), lo.xmax_ymax(), lo.xmin_ymax()
				}, us[6] = {
					vec2d(0.0, 0.0), vec2d(1.0, 0.0), vec2d(0.0, 1.0), vec2d(1.0, 0.0), vec2d(1.0, 1.0), vec2d(0.0, 1.0)
				};
				colord cs[6] = {
					colord(0.4, 0.4, 0.4, 1.0), colord(0.4, 0.4, 0.4, 1.0), colord(0.4, 0.4, 0.4, 1.0),
					colord(0.4, 0.4, 0.4, 1.0), colord(0.4, 0.4, 0.4, 1.0), colord(0.4, 0.4, 0.4, 1.0)
				};
				platform::renderer_base::default().draw_triangles(vs, us, cs, 6, 0);
			}
			void _on_update() override {
				if (!platform::input::is_mouse_button_down(platform::input::mouse_button::left)) {
					return;
				}
				vec2i pos = get_window()->screen_to_client(platform::input::get_mouse_position());
				if (_orient == ui::orientation::horizontal) {
					_posv =
						(pos.x - _parent->get_layout().xmin - 0.5 * default_thickness) /
						(_parent->get_layout().width() - default_thickness);
				} else {
					_posv =
						(pos.y - _parent->get_layout().ymin - 0.5 * default_thickness) /
						(_parent->get_layout().height() - default_thickness);
				}
				_posv = clamp(_posv, 0.0, 1.0);
				_on_pos_changed();
				ui::manager::default().schedule_update(this);
			}

			void _initialize() override {
				ui::element::_initialize();
				_can_focus = false;
				_on_orient_changed();
			}
		};
		class split_panel : public ui::panel_base {
		public:
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
		protected:
			ui::element *_c1 = nullptr, *_c2 = nullptr;
			draggable_separator *_sep = nullptr;

			void _change_child(ui::element *&e, ui::element *newv) {
				if (e) {
					_children.remove(*e);
				}
				e = newv;
				if (e) {
					_children.add(*e);
				}
			}

			void _initialize() override {
				ui::panel_base::_initialize();
				_sep = ui::element::create<draggable_separator>();
				_sep->value_changed += [this](void_info&) {
					invalidate_layout();
				};
				_children.add(*_sep);
			}

			void _on_remove_child(element *e) override {
				if (e == _c1) {
					_c1 = nullptr;
				} else if (e == _c2) {
					_c2 = nullptr;
				}
			}

			void _render() const override {
				_child_on_render(_sep);
				if (_c1) {
					platform::renderer_base::default().push_clip(_sep->get_region1().minimum_bounding_box<int>());
					_child_on_render(_c1);
					platform::renderer_base::default().pop_clip();
				}
				if (_c2) {
					platform::renderer_base::default().push_clip(_sep->get_region2().minimum_bounding_box<int>());
					_child_on_render(_c2);
					platform::renderer_base::default().pop_clip();
				}
			}

			void _finish_layout() override {
				_child_recalc_layout(_sep, get_client_region());
				_sep->revalidate_layout();
				if (_c1) {
					_child_recalc_layout(_c1, _sep->get_region1());
					_c1->revalidate_layout();
				}
				if (_c2) {
					_child_recalc_layout(_c2, _sep->get_region2());
					_c2->revalidate_layout();
				}
				ui::panel_base::_finish_layout();
			}
		};

		struct tab_drag_info {
			tab_drag_info(vec2d df) : drag_diff(df) {
			}
			const vec2d drag_diff;
		};
		class tab_button : public ui::panel_base {
			friend class ui::element;
		public:
			constexpr static double drag_pivot = 5.0;

			void set_text(const str_t &str) {
				_content.set_text(str);
			}
			const str_t &get_text() const {
				return _content.get_text();
			}

			vec2d get_desired_size() const override {
				vec2d sz = _content.get_text_size() + _padding.size();
				sz.x += sz.y;
				return sz;
			}

			event<void_info> click, request_close;
			event<tab_drag_info> start_drag;
		protected:
			ui::content_host _content{ *this };
			ui::button *_btn;
			vec2d mdpos;

			void _on_mouse_down(ui::mouse_button_info &p) override {
				panel_base::_on_mouse_down(p);
				if (p.button == platform::input::mouse_button::left && !_btn->hit_test(p.position)) {
					p.mark_focus_set();
					mdpos = p.position;
					ui::manager::default().schedule_update(this);
					click.invoke_noret();
				}
			}

			void _on_update() override {
				if (platform::input::is_mouse_button_down(platform::input::mouse_button::left)) {
					vec2d diff = get_window()->screen_to_client(platform::input::get_mouse_position()).convert<double>() - mdpos;
					if (diff.length_sqr() > drag_pivot * drag_pivot) {
						start_drag.invoke_noret(get_layout().xmin_ymin() - mdpos);
					} else {
						ui::manager::default().schedule_update(this);
					}
				}
			}
			void _render() const override {
				_content.render();
				panel_base::_render();
			}

			void _finish_layout() override {
				_btn->set_width(get_layout().height() - get_padding().height());
				panel_base::_finish_layout();
			}

			void _initialize() override {
				panel_base::_initialize();
				_btn = ui::element::create<ui::button>();
				_btn->set_anchor(ui::anchor::dock_right);
				_btn->click += [this](void_info &vp) {
					request_close(vp);
				};
				_children.add(*_btn);
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

			size_t tab_count() const {
				return _tabs.size();
			}
		protected:
			std::list<tab*> _tabs;
			std::list<tab*>::iterator _active_tab{ _tabs.end() };
			std::list<tab_host*>::iterator _tok;

			void _finish_layout() override;

			void _initialize() override;
			void _dispose() override;
		};
		class tab : public ui::panel {
			friend class tab_host;
			friend class dock_manager;
		public:
			void set_caption(const str_t &s) {
				_btn->set_text(s);
			}
			const str_t &get_caption() const {
				return _btn->get_text();
			}
		protected:
			tab_button *_btn;
			std::list<tab*>::iterator _tok;

			tab_host *_get_host() const {
				tab_host *hst = dynamic_cast<tab_host*>(parent());
				assert(hst);
				return hst;
			}

			virtual void _on_request_close() {
				_get_host()->remove_tab(*this);
				ui::manager::default().mark_disposal(this);
			}

			void _initialize() override;
			void _dispose() override {
				ui::manager::default().mark_disposal(_btn);
				ui::panel::_dispose();
			}
		};

		class dock_manager {
			friend class tab;
			friend class tab_host;
		public:
			tab_host *get_focused_tab_host() const {
				ui::element *focus = ui::manager::default().get_focused();
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
			tab *new_tab(tab_host *host = nullptr) {
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
				return _wndcnt == 0 && _drag == nullptr;
			}

			void update_changed_hosts() {
				std::deque<ui::element*> elems;
				for (auto i = _changed.begin(); i != _changed.end(); ++i) {
					if ((*i)->tab_count() == 0) {
						elems.push_back(*i);
					}
				}
				_changed.clear();
				while (!elems.empty()) {
					ui::element *cur = elems.front();
					elems.pop_front();
					split_panel *father = dynamic_cast<split_panel*>(cur->parent());
					if (father) {
						ui::element *other = cur == father->get_child1() ? father->get_child2() : father->get_child1();
						father->set_child1(nullptr);
						father->set_child2(nullptr);
						split_panel *ff = dynamic_cast<split_panel*>(father->parent());
						if (ff) {
							if (father == ff->get_child1()) {
								ff->set_child1(other);
							} else {
								ff->set_child2(other);
							}
						} else {
							platform::window *f = dynamic_cast<platform::window*>(father->parent());
							assert(f);
							f->children().remove(*father);
							f->children().add(*other);
						}
					} else {
						platform::window *f = dynamic_cast<platform::window*>(cur->parent());
						assert(f);
						ui::manager::default().mark_disposal(f);
						--_wndcnt;
					}
					ui::manager::default().mark_disposal(cur);
				}
			}
			void update_drag() {
				if (_drag) {
					if (_stopdrag()) {
						// TODO combine with other hosts
						platform::window *wnd = _new_window();
						tab_host *nhst = ui::element::create<tab_host>();
						nhst->add_tab(*_drag);
						wnd->children().add(*nhst);
						wnd->set_size(vec2d(_dragrect.width(), _dragrect.ymax - _dragdiff.y).convert<int>());
						wnd->set_position(platform::input::get_mouse_position() + _dragdiff.convert<int>());
						_drag = nullptr;
						return;
					}
					// TODO preview
				}
			}
			void update() {
				update_changed_hosts();
				update_drag();
			}

			void start_drag_tab(tab &t, vec2d diff, rectd layout, std::function<bool()> stop = []() {
				return !platform::input::is_mouse_button_down(platform::input::mouse_button::left);
			}) {
				assert(_drag == nullptr);
				tab_host *hst = t._get_host();
				if (hst) {
					hst->remove_tab(t);
				}
				_drag = &t;
				_dragdiff = diff;
				_dragrect = layout;
				_stopdrag = stop;
			}

			inline static dock_manager &default() {
				return _dman;
			}
		protected:
			size_t _wndcnt = 0;
			std::unordered_set<tab_host*> _changed;
			std::list<tab_host*> _hostlist;
			// drag related stuff
			tab *_drag = nullptr;
			vec2d _dragdiff;
			rectd _dragrect;
			std::function<bool()> _stopdrag;

			platform::window *_new_window() {
				platform::window *wnd = ui::element::create<platform::window>();
				++_wndcnt;
				return wnd;
			}

			void _on_tab_detached(tab_host &host, tab&) {
				_changed.insert(&host);
			}

			void _on_tab_host_created(tab_host &hst) {
				CP_INFO("tab host 0x%p created", &hst);
				hst._tok = _hostlist.insert(_hostlist.end(), &hst);
			}
			void _on_tab_host_disposed(tab_host &hst) {
				CP_INFO("tab host 0x%p disposed", &hst);
				_hostlist.erase(hst._tok);
			}
			void _on_tab_host_relayout(tab_host &hst) {
				CP_INFO("tab host 0x%p relayout", &hst);
			}

			static dock_manager _dman;
		};

		inline void tab::_initialize() {
			ui::panel::_initialize();
			_btn = ui::element::create<tab_button>();
			_btn->click += [this](void_info&) {
				_get_host()->activate_tab(*this);
			};
			_btn->request_close += [this](void_info&) {
				_on_request_close();
			};
			_btn->start_drag += [this](tab_drag_info &p) {
				vec2d diff = p.drag_diff - vec2d(get_layout().xmin, _btn->get_layout().ymin);
				dock_manager::default().start_drag_tab(*this, p.drag_diff, get_layout().translated(diff));
			};
		}

		inline void tab_host::add_tab(tab &t) {
			t._tok = _tabs.insert(_tabs.end(), &t);
			_children.add(t);
			_children.add(*t._btn);
			if (_tabs.size() == 1) {
				activate_tab(t);
			}
		}
		inline void tab_host::activate_tab(tab &t) {
			assert(t._parent == this);
			if (_active_tab != _tabs.end()) {
				(*_active_tab)->set_visibility(ui::visibility::ignored);
			}
			_active_tab = t._tok;
			t.set_visibility(ui::visibility::visible);
			ui::manager::default().set_focus(&t);
			invalidate_layout();
		}
		inline void tab_host::_finish_layout() {
			rectd client = get_client_region();
			double x = 0.0, maxy = 0.0;
			for (auto i = _tabs.begin(); i != _tabs.end(); ++i) {
				vec2d ds = (*i)->_btn->get_desired_size();
				maxy = std::max(maxy, ds.y);
				_child_set_layout((*i)->_btn, rectd(client.xmin + x, client.xmin + x + ds.x, client.ymin, client.ymin + maxy));
				(*i)->_btn->revalidate_layout();
				x += ds.x;
			}
			if (_active_tab != _tabs.end()) {
				_child_set_layout(*_active_tab, rectd(client.xmin, client.xmax, client.ymin + maxy, client.ymax));
				(*_active_tab)->revalidate_layout();
			}
			ui::panel_base::_finish_layout();
		}
		inline void tab_host::remove_tab(tab &t) {
			if (t._tok == _active_tab) {
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
			_tabs.erase(t._tok);
			dock_manager::default()._on_tab_detached(*this, t);
		}
		inline void tab_host::_initialize() {
			panel_base::_initialize();
			dock_manager::default()._on_tab_host_created(*this);
		}
		inline void tab_host::_dispose() {
			dock_manager::default()._on_tab_host_disposed(*this);
			panel_base::_dispose();
		}
	}
}
