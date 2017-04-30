#pragma once

#include "renderer.h"
#include "../ui/panel.h"
#include "../utilities/event.h"
#include "../utilities/textconfig.h"

namespace codepad {
	namespace platform {
		struct size_changed_info {
			size_changed_info(vec2i v) : new_size(v) {
			}
			const vec2i new_size;
		};
		class window_base : public ui::panel {
			friend class ui::manager;
		public:
			~window_base() {
				if (ui::manager::default().get_focused() == _focus) {
					ui::manager::default().set_focus(nullptr);
				}
			}

			virtual void set_caption(const str_t&) = 0;

			virtual bool idle() = 0;

			virtual vec2i screen_to_client(vec2i) const = 0;
			virtual vec2i client_to_screen(vec2i) const = 0;

			event<void_info> close_request, got_window_focus, lost_window_focus;
			event<size_changed_info> size_changed;
		protected:
			element *_focus{ this };

			void _on_prerender(platform::renderer_base &r) const override {
				r.begin(*this);
				panel::_on_prerender(r);
			}
			void _on_postrender(platform::renderer_base &r) const override {
				panel::_on_postrender(r);
				r.end();
			}

			virtual void _on_close_request(void_info &p) {
				close_request(p);
			}
			virtual void _on_size_changed(size_changed_info &p) {
				invalidate_layout();
				size_changed(p);
			}

			void _on_key_down(ui::key_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_key_down(p);
				} else {
					element::_on_key_down(p);
				}
			}
			void _on_key_up(ui::key_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_key_up(p);
				} else {
					element::_on_key_up(p);
				}
			}
			void _on_keyboard_text(ui::text_info &p) override {
				if (_focus && _focus != this) {
					_focus->_on_keyboard_text(p);
				} else {
					element::_on_keyboard_text(p);
				}
			}

			virtual void _on_got_window_focus(void_info &p) {
				ui::manager::default().set_focus(_focus);
				got_window_focus(p);
			}
			virtual void _on_lost_window_focus(void_info &p) {
				if (ui::manager::default().get_focused() == _focus) { // in case the focus has already shifted
					ui::manager::default().set_focus(nullptr);
				}
				lost_window_focus(p);
			}
		};
	}
	namespace ui {
		inline void manager::update_invalid_visuals(platform::renderer_base &r) {
			if (!_dirty.empty()) {
				std::unordered_set<element*> ss;
				for (auto i = _dirty.begin(); i != _dirty.end(); ++i) {
					platform::window_base *wnd = (*i)->get_window();
					if (wnd) {
						ss.insert(wnd);
					}
				}
				_dirty.clear();
				for (auto i = ss.begin(); i != ss.end(); ++i) {
					(*i)->_on_render(r);
				}
			}
		}
		inline void manager::set_focus(element *elem) {
			if (elem == _focus) {
				return;
			}
			platform::window_base *neww = elem ? elem->get_window() : nullptr;
			void_info vp;
			element *oldf = _focus;
			_focus = elem;
			if (neww) {
				neww->_focus = elem;
			}
			if (oldf) {
				oldf->_on_lost_focus(vp);
			}
			if (_focus) {
				_focus->_on_got_focus(vp);
			}
			// TODO debug
			printf("focus changed to 0x%p", _focus);
			if (_focus) {
				printf(": %s\n", typeid(*_focus).name());
			} else {
				printf("(nullptr)\n");
			}
		}

		inline platform::window_base *element::get_window() {
			element *cur = this;
			while (cur->_parent) {
				cur = cur->_parent;
			}
			return dynamic_cast<platform::window_base*>(cur);
		}
	}
}
