#pragma once

#include "element.h"
#include "manager.h"
#include "../platform/renderer.h"

namespace codepad {
	namespace ui {
		struct collection_change_info {
			enum class type { add, remove };
			collection_change_info(type t, element *c) : change_type(t), changed(c) {
			}
			const type change_type;
			element *const changed;
		};
		class element_collection {
		public:
			element_collection(panel_base &b) : _f(b) {
			}
			~element_collection() {
				for (auto i = _cs.begin(); i != _cs.end(); ++i) {
					(*i)->_parent = nullptr;
				}
			}

			void add(element&);
			void remove(element&);

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
			size_t size() const {
				return _cs.size();
			}

			event<collection_change_info> changed;
		protected:
			panel_base &_f;
			std::list<element*> _cs;
		};
		class panel_base : public element {
			friend class element;
		public:
			virtual bool override_children_layout() const {
				return false;
			}
			virtual bool dependent_relayout() const {
				return !has_width() || !has_height() || override_children_layout();
			}

			cursor get_current_display_cursor() const override {
				if (_children_cursor != cursor::not_specified) {
					return _children_cursor;
				}
				return element::get_current_display_cursor();
			}
		protected:
			panel_base() : element(), _children(*this) {
				_children.changed += [this](collection_change_info &info) {
					if (info.change_type == collection_change_info::type::add) {
						_on_add_child(info.changed);
					} else {
						_on_remove_child(info.changed);
					}
				};
			}

			virtual void _on_add_child(element *e) {
				e->invalidate_layout();
				invalidate_visual();
			}
			virtual void _on_remove_child(element*) {
				if (dependent_relayout()) {
					invalidate_layout();
				}
				invalidate_visual();
			}

			void _on_padding_changed() override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					(*i)->invalidate_layout();
				}
			}

			void _render(platform::renderer_base &r) const override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					(*i)->_on_render(r);
				}
			}

			void _finish_layout() override {
				element::_finish_layout();
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					(*i)->invalidate_layout();
				}
			}

			void _on_mouse_down(mouse_button_info &p) override {
				mouse_down(p);
				bool hit = false;
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_down(p);
						hit = true;
						break;
					}
				}
				if (!hit) {
					manager::default().set_focus(this);
				}
			}
			void _on_mouse_leave(void_info &p) override {
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->is_mouse_over()) {
						(*i)->_on_mouse_leave(p);
					}
				}
				element::_on_mouse_leave(p);
			}
			void _on_mouse_move(mouse_move_info &p) override {
				bool hit = false;
				_children_cursor = cursor::not_specified;
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if (!hit && (*i)->hit_test(p.new_pos)) {
						if (!(*i)->is_mouse_over()) {
							void_info vi;
							(*i)->_on_mouse_enter(vi);
						}
						(*i)->_on_mouse_move(p);
						_children_cursor = (*i)->get_current_display_cursor();
						hit = true;
						continue;
					}
					if ((*i)->is_mouse_over()) {
						void_info vi;
						(*i)->_on_mouse_leave(vi);
					}
				}
				element::_on_mouse_move(p);
			}
			void _on_mouse_scroll(mouse_scroll_info &p) override {
				element::_on_mouse_scroll(p);
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_scroll(p);
						break;
					}
				}
			}
			void _on_mouse_up(mouse_button_info &p) override {
				element::_on_mouse_up(p);
				for (auto i = _children.begin(); i != _children.end(); ++i) {
					if ((*i)->hit_test(p.position)) {
						(*i)->_on_mouse_up(p);
						break;
					}
				}
			}

			element_collection _children;
			cursor _children_cursor = cursor::not_specified;
		};

		inline void element::_recalc_layout() {
			_layout = _parent->_padding.shrink(_parent->_layout);
			vec2d sz = get_target_size();
			_calc_layout_onedir(
				_anchor & static_cast<unsigned char>(anchor::left), _anchor & static_cast<unsigned char>(anchor::right),
				_layout.xmin, _layout.xmax, _margin.left, _margin.right, sz.x
			);
			_calc_layout_onedir(
				_anchor & static_cast<unsigned char>(anchor::top), _anchor & static_cast<unsigned char>(anchor::bottom),
				_layout.ymin, _layout.ymax, _margin.top, _margin.bottom, sz.y
			);
		}
		inline element::~element() {
			if (_parent) {
				_parent->_children.remove(*this);
			}
			manager::default()._on_element_disposed(this);
		}

		inline void element_collection::add(element &elem) {
			assert(elem._parent == nullptr);
			elem._parent = &_f;
			elem._tok = _cs.insert(_cs.end(), &elem);
			collection_change_info ci(collection_change_info::type::add, &elem);
			changed(ci);
		}
		inline void element_collection::remove(element &elem) {
			assert(elem._parent == &_f);
			elem._parent = nullptr;
			_cs.erase(elem._tok);
			collection_change_info ci(collection_change_info::type::remove, &elem);
			changed(ci);
		}

		inline void manager::update_invalid_layouts() {
			if (!_targets.empty()) {
				_layouting = true;
				std::unordered_map<element*, bool> ftg;
				for (auto i = _targets.begin(); i != _targets.end(); ++i) {
					element *cur = i->first;
					while (cur->_parent && cur->_parent->dependent_relayout()) {
						cur = cur->_parent;
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
						li.elem->_recalc_layout();
					}
					li.elem->_finish_layout();
				}
				_layouting = false;
			}
		}

		class panel : public panel_base {
		public:
			element_collection &children() {
				return _children;
			}
		protected:
		};
	}
}
