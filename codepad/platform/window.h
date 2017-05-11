#pragma once

#include <functional>

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
			friend class ui::element_collection;
		public:
			virtual void set_caption(const str_t&) = 0;
			virtual vec2i get_position() const = 0;
			virtual void set_position(vec2i) = 0;
			virtual vec2i get_size() const = 0;
			virtual void set_size(vec2i) = 0;

			virtual vec2i screen_to_client(vec2i) const = 0;
			virtual vec2i client_to_screen(vec2i) const = 0;

			virtual void start_drag(std::function<bool()> dst = []() {
				return input::is_mouse_button_down(input::mouse_button::left);
			}) {
				assert(!_drag);
				_dragcontinue = dst;
				_drag = true;
				_doffset = get_position() - input::get_mouse_position();
			}

			event<void_info> close_request, got_window_focus, lost_window_focus;
			event<size_changed_info> size_changed;
		protected:
			element *_focus{ this };
			bool _drag = false;
			vec2i _doffset;
			std::function<bool()> _dragcontinue;

			virtual bool _update_drag() { // TODO still buggy
				if (_drag) {
					if (_dragcontinue()) {
						set_position(_doffset + input::get_mouse_position());
						return true;
					}
					_drag = false;
				}
				return false;
			}

			void _on_prerender() const override {
				platform::renderer_base::default().begin(*this);
				panel::_on_prerender();
			}
			void _on_postrender() const override {
				panel::_on_postrender();
				platform::renderer_base::default().end();
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

			void _recalc_layout(rectd) override {
				_clientrgn = get_padding().shrink(get_layout());
			}

			virtual void _on_removing_window_element(element *e) {
				element *ef = _focus;
				for (; ef && e != ef; ef = ef->parent()) {
				}
				if (ef) {
					ui::element *cur = _focus->parent();
					for (; !cur->get_can_focus(); cur = cur->parent()) {
					}
					if (ui::manager::default().get_focused() == _focus) {
						ui::manager::default().set_focus(cur);
					} else {
						_focus = cur;
					}
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

			void _initialize() override {
				panel::_initialize();
				renderer_base::default()._new_window(*this);
			}
			void _dispose() override {
				if (ui::manager::default().get_focused() == _focus) {
					ui::manager::default().set_focus(nullptr);
				}
				renderer_base::default()._delete_window(*this);
				ui::panel::_dispose();
			}
		};
	}
	namespace ui {
		inline void manager::update_invalid_visuals() {
			if (!_dirty.empty()) {
				CP_INFO("repaint");
				std::unordered_set<element*> ss;
				for (auto i = _dirty.begin(); i != _dirty.end(); ++i) {
					platform::window_base *wnd = (*i)->get_window();
					if (wnd) {
						ss.insert(wnd);
					}
				}
				_dirty.clear();
				for (auto i = ss.begin(); i != ss.end(); ++i) {
					(*i)->_on_render();
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
			CP_INFO("focus changed to 0x%p <%s>", _focus, _focus ? typeid(*_focus).name() : "nullptr");
		}

		inline platform::window_base *element::get_window() {
			element *cur = this;
			while (cur->_parent) {
				cur = cur->_parent;
			}
			return dynamic_cast<platform::window_base*>(cur);
		}

		inline void element_collection::remove(element &elem) {
			assert(elem._parent == &_f);
			platform::window_base *wnd = _f.get_window();
			if (wnd) {
				wnd->_on_removing_window_element(&elem);
			}
			elem._parent = nullptr;
			_cs.erase(elem._tok);
			collection_change_info ci(collection_change_info::type::remove, &elem);
			_f._on_children_changed(ci);
			changed(ci);
		}
	}
}
