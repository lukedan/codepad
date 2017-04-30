#pragma once

#include <unordered_map>
#include <unordered_set>
#include <deque>

#include "element.h"

namespace codepad {
	namespace ui {
		class manager {
			friend class element;
		public:
			inline static manager &default() {
				return _sman;
			}

			void invalidate_layout(element *e) {
				if (_layouting) {
					_q.push_back(_layout_info(e, true));
				} else {
					_targets[e] = true;
				}
			}
			void revalidate_layout(element *e) {
				if (_layouting) {
					_q.push_back(_layout_info(e, false));
				} else {
					if (_targets.find(e) == _targets.end()) {
						_targets.insert(std::make_pair(e, false));
					}
				}
			}
			void update_invalid_layouts();

			void invalidate_visual(element *e) {
				_dirty.insert(e);
			}
			void update_invalid_visuals(platform::renderer_base&);

			element *get_focused() const {
				return _focus;
			}
			void set_focus(element*);
		protected:
			struct _layout_info {
				_layout_info() = default;
				_layout_info(element *el, bool v) : elem(el), need_recalc(v) {
				}
				element *elem;
				bool need_recalc;
			};

			// layout
			bool _layouting = false;
			std::unordered_map<element*, bool> _targets;
			std::deque<_layout_info> _q;
			// visual
			std::unordered_set<element*> _dirty;
			// focus
			element *_focus = nullptr;

			void _on_element_disposed(element *e) {
				auto layoutit = _targets.find(e);
				if (layoutit != _targets.end()) {
					_targets.erase(layoutit);
				}
				auto renderit = _dirty.find(e);
				if (renderit != _dirty.end()) {
					_dirty.erase(renderit);
				}
			}

			static manager _sman;
		};
		inline void element::invalidate_layout() {
			manager::default().invalidate_layout(this);
		}
		inline void element::revalidate_layout() {
			manager::default().revalidate_layout(this);
		}
		inline void element::invalidate_visual() {
			manager::default().invalidate_visual(this);
		}
		inline void element::_on_mouse_down(mouse_button_info &p) {
			mouse_down(p);
			manager::default().set_focus(this);
		}
	}
}
