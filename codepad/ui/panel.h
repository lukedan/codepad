#pragma once

#include <chrono>
#include <unordered_map>

#include "element.h"
#include "manager.h"
#include "../os/renderer.h"

namespace codepad {
	namespace ui {
		struct element_collection_change_info {
			enum class type {
				add,
				remove,
				set_zindex
			};
			element_collection_change_info(type t, element *c) : change_type(t), changed(c) {
			}
			const type change_type;
			element *const changed;
		};
		class element_collection { // TODO implement z-order
		public:
			explicit element_collection(panel_base &b) : _f(b) {
			}
			~element_collection() {
				for (auto i = _cs.begin(); i != _cs.end(); ++i) {
					(*i)->_parent = nullptr;
				}
			}

			void add(element&);
			void remove(element&);
			void set_zindex(element&, int);

			std::list<element*>::iterator begin() {
				return _cs.begin();
			}
			std::list<element*>::const_iterator begin() const {
				return _cs.begin();
			}
			std::list<element*>::iterator end() {
				return _cs.end();
			}
			std::list<element*>::const_iterator end() const {
				return _cs.end();
			}

			std::list<element*>::reverse_iterator rbegin() {
				return _cs.rbegin();
			}
			std::list<element*>::const_reverse_iterator rbegin() const {
				return _cs.rbegin();
			}
			std::list<element*>::reverse_iterator rend() {
				return _cs.rend();
			}
			std::list<element*>::const_reverse_iterator rend() const {
				return _cs.rend();
			}

			size_t size() const {
				return _cs.size();
			}

			event<element_collection_change_info> changed;
		protected:
			panel_base &_f;
			std::list<element*> _cs;
		};
		class panel_base : public element {
			friend class element;
			friend class element_collection;
		public:
			virtual bool override_children_layout() const {
				return false;
			}
			virtual bool is_dependent_relayout() const {
				return override_children_layout() || !has_width() || !has_height();
			}

			cursor get_current_display_cursor() const override {
				if (_children_cursor != cursor::not_specified) {
					return _children_cursor;
				}
				return element::get_current_display_cursor();
			}

			virtual void set_dispose_children(bool v) {
				_dispose_children = v;
			}
			virtual bool get_dispose_children() const {
				return _dispose_children;
			}
		protected:
			virtual void _on_children_changed(element_collection_change_info &p) {
				switch (p.change_type) {
				case element_collection_change_info::type::add:
					_on_child_added(p.changed);
					break;
				case element_collection_change_info::type::remove:
					_on_child_removed(p.changed);
					break;
				case element_collection_change_info::type::set_zindex:
					_on_child_zindex_changed(p.changed);
					break;
				}
			}
			virtual void _on_child_added(element *e) {
				if (is_dependent_relayout()) {
					invalidate_layout();
				} else {
					e->invalidate_layout();
				}
				invalidate_visual();
			}
			virtual void _on_child_removed(element*) {
				if (is_dependent_relayout()) {
					invalidate_layout();
				}
				invalidate_visual();
			}
			virtual void _on_child_zindex_changed(element*) {
				invalidate_visual();
			}

			void _on_padding_changed() override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					(*i)->invalidate_layout();
				}
			}

			void _custom_render() override {
				for (auto i = _children.rbegin(); i != _children.rend(); ++i) {
					(*i)->_on_render();
				}
			}

			void _finish_layout() override {
				if (!override_children_layout()) {
					for (auto i = _children.begin(); i != _children.end(); ++i) {
						(*i)->invalidate_layout();
					}
				}
				element::_finish_layout();
			}

			void _on_mouse_down(mouse_button_info &p) override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_down(p);
						break;
					}
				}
				mouse_down.invoke(p);
				if (_can_focus && !p.focus_set()) {
					p.mark_focus_set();
					manager::get().set_focus(this);
				}
				_set_visual_style_bit(visual_manager::default_states().mouse_down, true);
			}
			void _on_mouse_leave() override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->is_mouse_over()) {
						(*i)->_on_mouse_leave();
					}
				}
				element::_on_mouse_leave();
			}
			void _on_mouse_move(mouse_move_info &p) override {
				bool hit = false;
				_children_cursor = cursor::not_specified;
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if (!hit && (*i)->hit_test(p.new_pos)) {
						if (!(*i)->is_mouse_over()) {
							(*i)->_on_mouse_enter();
						}
						(*i)->_on_mouse_move(p);
						_children_cursor = (*i)->get_current_display_cursor();
						hit = true;
						continue;
					}
					if ((*i)->is_mouse_over()) {
						(*i)->_on_mouse_leave();
					}
				}
				element::_on_mouse_move(p);
			}
			void _on_mouse_scroll(mouse_scroll_info &p) override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_scroll(p);
						break;
					}
				}
				element::_on_mouse_scroll(p);
			}
			void _on_mouse_up(mouse_button_info &p) override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_up(p);
						break;
					}
				}
				element::_on_mouse_up(p);
			}

			void _initialize() override {
				element::_initialize();
			}
			void _dispose() override {
				if (_dispose_children) {
					for (auto i = _children.begin(); i != _children.end(); ++i) {
						manager::get().mark_disposal(**i);
					}
				}
				element::_dispose();
			}

			void _child_recalc_layout_noreval(element *e, rectd r) const {
				assert_true_usage(e->_parent == this, "can only invoke _recalc_layout() on children");
				e->_recalc_layout(r);
			}
			void _child_set_layout_noreval(element *e, rectd r) const {
				assert_true_usage(e->_parent == this, "can only set layout of children");
				e->_layout = r;
				e->_clientrgn = e->get_padding().shrink(e->get_layout());
			}
			void _child_recalc_layout(element *e, rectd r) const {
				_child_recalc_layout_noreval(e, r);
				e->revalidate_layout();
			}
			void _child_set_layout(element *e, rectd r) const {
				_child_set_layout_noreval(e, r);
				e->revalidate_layout();
			}

			void _child_on_render(element *e) const {
				assert_true_usage(e->_parent == this, "can only invoke _on_render() on children");
				e->_on_render();
			}

			element_collection _children{*this};
			cursor _children_cursor = cursor::not_specified;
			bool _dispose_children = true; // if true, marks all children for disposal upon disposal
		};

		inline void element::_recalc_layout(rectd prgn) {
			_layout = prgn;
			vec2d sz = get_target_size();
			layout_on_direction(
				test_bit_all(_anchor, anchor::left), test_bit_all(_anchor, anchor::right),
				_layout.xmin, _layout.xmax, _margin.left, _margin.right, sz.x
			);
			layout_on_direction(
				test_bit_all(_anchor, anchor::top), test_bit_all(_anchor, anchor::bottom),
				_layout.ymin, _layout.ymax, _margin.top, _margin.bottom, sz.y
			);
			_clientrgn = get_padding().shrink(get_layout());
		}
		inline void element::_dispose() {
			if (_parent) {
				_parent->_children.remove(*this);
			}
#ifdef CP_DETECT_USAGE_ERRORS
			_initialized = false;
#endif
		}
		inline void element::set_zindex(int v) {
			if (_parent) {
				_parent->_children.set_zindex(*this, v);
			} else {
				_zindex = v;
			}
		}

		inline void element_collection::add(element &elem) {
			assert_true_usage(elem._parent == nullptr, "the element is already a child of another panel");
			elem._parent = &_f;
			// use naive approach to find the item whose z-index is less or equal
			auto ins_before = _cs.begin();
			for (; ins_before != _cs.end(); ++ins_before) {
				if ((*ins_before)->get_zindex() <= elem.get_zindex()) {
					break;
				}
			}
			elem._col_token = _cs.insert(ins_before, &elem);
			element_collection_change_info ci(element_collection_change_info::type::add, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}
		inline void element_collection::set_zindex(element &elem, int newz) {
			if (elem._zindex != newz) {
				auto ins_before = _cs.erase(elem._col_token);
				if (elem._zindex < newz) {
					while (ins_before != _cs.begin()) {
						--ins_before;
						if ((*ins_before)->get_zindex() > newz) {
							++ins_before;
							break;
						}
					}
				} else {
					for (; ins_before != _cs.end(); ++ins_before) {
						if ((*ins_before)->get_zindex() <= newz) {
							break;
						}
					}
				}
				elem._col_token = _cs.insert(ins_before, &elem);
				elem._zindex = newz;
			}
			element_collection_change_info ci(element_collection_change_info::type::set_zindex, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}

		inline void manager::update_invalid_layouts() {
			monitor_performance mon(CP_HERE);
			if (!_targets.empty()) {
				auto start = std::chrono::high_resolution_clock::now();
				_layouting = true;
				std::unordered_map<element*, bool> ftg;
				for (auto i = _targets.begin(); i != _targets.end(); ++i) {
					element *cur = i->first;
					if (i->second) {
						while (cur->_parent && cur->_parent->is_dependent_relayout()) {
							cur = cur->_parent;
						}
					}
					bool &v = ftg[cur];
					v = v || i->second;
				}
				_targets.clear();
				for (auto i = ftg.begin(); i != ftg.end(); ++i) {
					_q.push_back(_layout_info(i->first, i->second));
				}
				while (!_q.empty()) {
					_layout_info li = _q.front();
					_q.pop_front();
					if (li.need_recalc) {
						rectd prgn;
						if (li.elem->_parent) {
							prgn = li.elem->_parent->get_client_region();
						}
						li.elem->_recalc_layout(prgn);
					}
					li.elem->_finish_layout();
				}
				_layouting = false;
				double dur = std::chrono::duration<double, std::milli>(
					std::chrono::high_resolution_clock::now() - start
					).count();
				if (dur > relayout_time_redline) {
					logger::get().log_warning(CP_HERE, "relayout cost ", dur, "ms");
				}
			}
		}

		class panel : public panel_base {
		public:
			element_collection &children() {
				return _children;
			}
		protected:
			void _initialize() override {
				panel_base::_initialize();
				_can_focus = false;
			}
		};
	}
}
