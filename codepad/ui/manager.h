#pragma once

#include <set>
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
				_target.insert(e);
			}
			void update_invalid_layouts() {
				// TODO bfs
			}
		protected:
			std::set<element*> _target;

			void _on_element_disposed(element *e) {
				auto it = _target.find(e);
				if (it != _target.end()) {
					_target.erase(it);
				}
			}

			static manager _sman;
		};
		inline void element::invalidate_layout() {
			manager::default().invalidate_layout(this);
		}
		inline void element::mark_redraw() {
			// TODO
		}
		inline element::~element() {
			manager::default()._on_element_disposed(this);
		}
	}
}
