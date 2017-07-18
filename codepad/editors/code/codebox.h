#pragma once

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../../ui/element.h"
#include "../../ui/manager.h"
#include "../../ui/textrenderer.h"
#include "../../os/window.h"

namespace codepad {
	namespace editor {
		class codebox;
		class codebox_editor;

		class codebox_component : public ui::element { // TODO move focus to codebox, make elements non-focusable
			friend class codebox;
		public:
		protected:
			codebox *_get_box() const;
			codebox_editor *_get_editor() const;

			virtual void _on_added() {
			}
			virtual void _on_removing() {
			}
		};
		class codebox : public ui::panel_base {
			friend class codebox_editor;
		public:
			void set_vertical_position(double p) {
				_vscroll->set_value(p);
			}
			double get_vertical_position() const {
				return _vscroll->get_value();
			}

			void make_point_visible(vec2d v) {
				_vscroll->make_point_visible(v.y);
				// TODO horizontal view
			}

			bool override_children_layout() const override {
				return true;
			}

			codebox_editor *get_editor() const {
				return _editor;
			}

			void add_component_left(codebox_component &e) {
				_add_component_to(e, _lcs);
			}
			void remove_component_left(codebox_component &e) {
				_remove_component_from(e, _lcs);
			}

			void add_component_right(codebox_component &e) {
				_add_component_to(e, _rcs);
			}
			void remove_component_right(codebox_component &e) {
				_remove_component_from(e, _rcs);
			}
		protected:
			ui::scroll_bar *_vscroll;
			codebox_editor *_editor;
			std::vector<codebox_component*> _lcs, _rcs;

			void _add_component_to(codebox_component &e, std::vector<codebox_component*> &v) {
				_children.add(e);
				v.push_back(&e);
				e._on_added();
			}
			void _remove_component_from(codebox_component &e, std::vector<codebox_component*> &v) {
				assert(e.parent() == this);
				auto it = std::find(v.begin(), v.end(), &e);
				assert(it != v.end());
				e._on_removing();
				v.erase(it);
				_children.remove(e);
			}

			void _reset_scrollbars();
			void _on_content_modified() {
				_reset_scrollbars();
			}

			void _on_mouse_scroll(ui::mouse_scroll_info&) override;

			void _finish_layout() override;

			void _initialize() override;
			void _dispose() override {
				while (_lcs.size() > 0) {
					codebox_component &cmp = *_lcs.back();
					_remove_component_from(cmp, _lcs);
					if (_dispose_children) {
						ui::manager::get().mark_disposal(cmp);
					}
				}
				while (_rcs.size() > 0) {
					codebox_component &cmp = *_rcs.back();
					_remove_component_from(cmp, _rcs);
					if (_dispose_children) {
						ui::manager::get().mark_disposal(cmp);
					}
				}

				panel_base::_dispose();
			}
		};

		inline codebox *codebox_component::_get_box() const {
#ifndef NDEBUG
			codebox *cb = dynamic_cast<codebox*>(_parent);
			assert(cb);
			return cb;
#else
			return static_cast<codebox*>(_parent);
#endif
		}
	}
}
