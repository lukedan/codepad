#pragma once

#include <list>

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
		enum class mouse_button { left, middle, right };
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

		struct thickness {
			explicit thickness(double uni = 0.0) : left(uni), top(uni), right(uni), bottom(uni) {
			}
			thickness(double l, double t, double r, double b) : left(l), top(t), right(r), bottom(b) {
			}
			double left, top, right, bottom;

			rectd extend(rectd r) const {
				return rectd(r.xmin - left, r.xmax + right, r.ymin - top, r.ymax + bottom);
			}
			rectd shrink(rectd r) const {
				return rectd(r.xmin + left, r.xmax - right, r.ymin + top, r.ymax - bottom);
			}

			vec2d size() const {
				return vec2d(left + right, top + bottom);
			}
		};

		enum class anchor : unsigned char {
			none = 0,

			left = 1,
			top = 2,
			right = 4,
			bottom = 8,

			top_left = top | left,
			top_right = top | right,
			bottom_left = bottom | left,
			bottom_right = bottom | right,

			stretch_horizontally = left | right,
			stretch_vertically = top | bottom,

			dock_left = stretch_vertically | left,
			dock_top = stretch_horizontally | top,
			dock_right = stretch_vertically | right,
			dock_bottom = stretch_horizontally | bottom,

			all = left | top | right | bottom
		};
		class panel_base;
		class manager;
		class element {
			friend class manager;
			friend class element_collection;
		public:
			virtual ~element();

			virtual void mark_redraw();

			rectd layout() const {
				return _layout;
			}
			bool is_mouse_over() const {
				return _mouse_over;
			}

			panel_base *parent() const {
				return _parent;
			}

			virtual void set_size(vec2d s) {
				_size = s;
				_has_size = true;
				invalidate_layout();
			}
			virtual void unset_size() {
				_dsize_cache = _has_size = false;
			}
			virtual bool has_size() const {
				return _has_size;
			}
			virtual vec2d get_desired_size() {
				return _padding.size();
			}
			virtual vec2d get_target_size() {
				if (!_has_size && !_dsize_cache) {
					_dsize_cache = true;
					_size = get_desired_size();
				}
				return _size;
			}
			virtual vec2d get_actual_size() const {
				return _layout.size();
			}

			void invalidate_layout();

			template <typename T> inline T *create() {
				static_assert(std::is_base_of<element, T>::value, "cannot create non-element");
				element *elem = new T();
				elem->_initialize();
				return static_cast<T*>(elem);
			}

			event<void_info> mouse_enter, mouse_leave, got_focus, lost_focus;
			event<mouse_move_info> mouse_move;
			event<mouse_button_info> mouse_down, mouse_up;
			event<mouse_scroll_info> mouse_scroll;
			event<key_info> key_down, key_up;
			event<text_info> keyboard_text;
		protected:
			element() {
			}

			// children-parent stuff
			panel_base *_parent = nullptr;
			std::list<element*>::iterator _tok;
			// layout result
			rectd _layout;
			// layout params
			unsigned char _anchor = static_cast<unsigned char>(anchor::all);
			bool _has_size = false, _dsize_cache = false;
			vec2d _size;
			thickness _margin, _padding;
			// input
			bool _mouse_over = false;

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

			virtual void _on_update() {
			}
			virtual void _on_render() {
			}

			inline static void _calc_layout_onedir(bool amin, bool amax, double &cmin, double &cmax, double pmin, double pmax, double sz) {
				if (amin && amax) {
					cmin += pmin;
					cmax -= pmax;
				} else if (amin) {
					cmin += pmin;
					cmax = cmin + sz;
				} else if (amax) {
					cmax -= pmax;
					cmin = cmax - sz;
				} else {
					cmin += (cmax - cmin - sz) * pmin / (pmin + pmax);
					cmax = cmin + sz;
				}
			}
			virtual void _on_invalid_layout();
			virtual void _recalc_layout();
			virtual void _finish_layout() {
			}

			virtual void _initialize() {
			}
		};
	}
}
