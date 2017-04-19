#pragma once

#include "../utilities/event.h"
#include "../utilities/misc.h"

namespace codepad {
	namespace ui {
		struct mouse_move_info {
			mouse_move_info(vec2i p) : new_pos(p) {
			}
			const vec2i new_pos;
		};
		struct mouse_scroll_info {
			mouse_scroll_info(double d) : delta(d) {
			}
			const double delta;
		};
		enum class mouse_button {
			left,
			middle,
			right
		};
		struct mouse_button_info {
			mouse_button_info(mouse_button mb) : button(mb) {
			}
			const mouse_button button;
		};
		struct key_info {
			key_info(int k) : key(k) {
			}
			const int key;
		};
		struct text_info {
			text_info(wchar_t c) : character(c) {
			}
			const wchar_t character;
		};

		class element {
		public:
			virtual ~element() {
			}

			virtual void mark_dirty(recti rgn) {
				if (_dirty) {
					_dirty_rgn = recti::bounding_box(_dirty_rgn, rgn);
				} else {
					_dirty = true;
					_dirty_rgn = rgn;
				}
			}

			rectd layout() const {
				return _layout;
			}
			bool is_mouse_over() const {
				return _mouse_over;
			}

			element *parent() const {
				return _parent;
			}

			event<void_info> mouse_enter, mouse_leave, got_focus, lost_focus;
			event<mouse_move_info> mouse_move;
			event<mouse_button_info> mouse_down, mouse_up;
			event<mouse_scroll_info> mouse_scroll;
			event<key_info> key_down, key_up;
			event<text_info> keyboard_text;
		protected:
			element *_parent = nullptr;
			rectd _layout;
			bool _mouse_over = false;

			bool _dirty = false;
			recti _dirty_rgn;

			virtual void _on_mouse_enter(void_info &p) {
				_mouse_over = true;
				mouse_enter(p);
			}
			virtual void _on_mouse_leave(void_info &p) {
				_mouse_over = false;
				mouse_leave(p);
			}
			virtual void _on_got_focus(void_info &p) {
				got_focus(p);
			}
			virtual void _on_lost_focus(void_info &p) {
				lost_focus(p);
			}
			virtual void _on_mouse_move(mouse_move_info &p) {
				mouse_move(p);
			}
			virtual void _on_mouse_down(mouse_button_info &p) {
				mouse_down(p);
			}
			virtual void _on_mouse_up(mouse_button_info &p) {
				mouse_up(p);
			}
			virtual void _on_mouse_scroll(mouse_scroll_info &p) {
				mouse_scroll(p);
			}
			virtual void _on_key_down(key_info &p) {
				key_down(p);
			}
			virtual void _on_key_up(key_info &p) {
				key_up(p);
			}
			virtual void _on_keyboard_text(text_info &p) {
				keyboard_text(p);
			}
		};
	}
}
