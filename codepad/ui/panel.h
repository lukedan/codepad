#pragma once

#include "element.h"
#include "manager.h"

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

			void add(element&);
			void remove(element&);

			event<collection_change_info> changed;
		protected:
			panel_base &_f;
			std::list<element*> _cs;
		};
		class panel_base : public element {
		public:
			panel_base() : element(), _children(*this) {
				_children.changed += [this](collection_change_info &info) {
					if (info.change_type == collection_change_info::type::add) {
						_on_add_child(info.changed);
					} else {
						_on_remove_child(info.changed);
					}
				};
			}

			virtual bool override_children_layout() const {
				return false;
			}
			virtual bool dependent_relayout() const {
				return !has_size() || override_children_layout();
			}
		protected:
			virtual void _on_add_child(element *e) {
				e->invalidate_layout();
			}
			virtual void _on_remove_child(element*) {
			}

			void _finish_layout() override {

			}

			element_collection _children;
		};

		inline void element::_on_invalid_layout() {
			if (_parent->override_children_layout()) {
				_parent->invalidate_layout();
			} else {
				_recalc_layout();
				_finish_layout();
			}
		}
		inline void element::_recalc_layout() {
			_layout = _padding.shrink(_parent->_layout);
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

		inline void element_collection::add(element &elem) {
			assert(elem._parent == nullptr);
			elem._parent = &_f;
			_cs.push_front(&elem);
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

		class panel : public panel_base {
		public:
			element_collection &children() {
				return _children;
			}
		protected:
		};
	}
}
