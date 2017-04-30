#pragma once

#include <list>

#include "../utilities/event.h"
#include "../utilities/misc.h"
#include "../platform/renderer.h"

namespace codepad {
	namespace ui {
		struct mouse_move_info {
			mouse_move_info(vec2d p) : new_pos(p) {
			}
			const vec2d new_pos;
		};
		struct mouse_scroll_info {
			mouse_scroll_info(double d, vec2d pos) : delta(d), position(pos) {
			}
			const double delta;
			const vec2d position;
			bool handled() const {
				return _handled;
			}
			void mark_handled() {
				assert(!_handled);
				_handled = true;
			}
		protected:
			bool _handled = false;
		};
		enum class mouse_button { left, middle, right };
		struct mouse_button_info {
			mouse_button_info(mouse_button mb, vec2d pos) : button(mb), position(pos) {
			}
			const mouse_button button;
			const vec2d position;
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

		enum class cursor {
			normal,
			busy,
			crosshair,
			hand,
			help,
			text_beam,
			denied,
			arrow_all,
			arrow_northeast_southwest,
			arrow_north_south,
			arrow_northwest_southeast,
			arrow_east_west,

			invisible,
			not_specified
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
		enum class visibility : unsigned char {
			ignored = 0,
			render_only = 1,
			interaction_only = 2,
			visible = render_only | interaction_only
		};
		class panel_base;
		class manager;
		class element {
			friend class manager;
			friend class element_collection;
			friend class panel_base;
			friend class platform::window_base;
		public:
			virtual ~element();

			rectd layout() const {
				return _layout;
			}
			bool is_mouse_over() const {
				return _mouse_over;
			}

			panel_base *parent() const {
				return _parent;
			}

			virtual void set_width(double w) {
				_width = w;
				_has_width = true;
				invalidate_layout();
			}
			virtual void set_height(double h) {
				_height = h;
				_has_height = true;
				invalidate_layout();
			}
			virtual void unset_width() {
				_has_width = false;
				invalidate_layout();
			}
			virtual void unset_height() {
				_has_height = false;
				invalidate_layout();
			}
			virtual void set_size(vec2d s) {
				_width = s.x;
				_height = s.y;
				_has_width = _has_height = true;
				invalidate_layout();
			}
			virtual void unset_size() {
				_has_width = _has_height = false;
				invalidate_layout();
			}
			virtual bool has_width() const {
				return _has_width;
			}
			virtual bool has_height() const {
				return _has_height;
			}
			virtual vec2d get_desired_size() const {
				return _padding.size();
			}
			virtual vec2d get_target_size() const {
				if (_has_width && _has_height) {
					return vec2d(_width, _height);
				}
				vec2d des = get_desired_size();
				if (_has_width) {
					des.x = _width;
				} else if (_has_height) {
					des.y = _height;
				}
				return des;
			}
			virtual vec2d get_actual_size() const {
				return _layout.size();
			}

			virtual void set_margin(thickness t) {
				_margin = t;
				invalidate_layout();
			}
			virtual thickness get_margin() const {
				return _margin;
			}
			virtual void set_padding(thickness t) {
				_padding = t;
				_on_padding_changed();
			}
			virtual thickness get_padding() const {
				return _padding;
			}

			virtual void set_anchor(anchor a) {
				_anchor = static_cast<unsigned char>(a);
				invalidate_layout();
			}
			virtual anchor get_anchor() const {
				return static_cast<anchor>(_anchor);
			}

			virtual void set_visibility(visibility v) {
				if ((
					(static_cast<unsigned char>(v) ^ _vis) &
					static_cast<unsigned char>(visibility::render_only)
					) != 0) {
					invalidate_visual();
				}
			}
			virtual visibility get_visibility() const {
				return static_cast<visibility>(_vis);
			}

			virtual bool hit_test(vec2d p) const {
				return _layout.contains(p);
			}

			virtual cursor get_default_cursor() const {
				return cursor::normal;
			}
			virtual void set_overriden_cursor(cursor c) {
				_crsr = c;
			}
			virtual cursor get_overriden_cursor() const {
				return _crsr;
			}
			virtual cursor get_current_display_cursor() const {
				if (_crsr == cursor::not_specified) {
					return get_default_cursor();
				}
				return _crsr;
			}

			virtual platform::window_base *get_window();

			void invalidate_visual();
			void invalidate_layout();
			void revalidate_layout();

			template <typename T> inline static T *create() {
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
			bool _has_width = false, _has_height = false;
			double _width, _height;
			thickness _margin, _padding;
			unsigned char _vis = static_cast<unsigned char>(visibility::visible);
			// input
			bool _mouse_over = false;
			cursor _crsr = cursor::not_specified;

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
			virtual void _on_mouse_down(mouse_button_info&);
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

			virtual void _on_padding_changed() {
			}

			virtual void _on_update() {
			}

			virtual void _on_prerender(platform::renderer_base &r) const {
				r.push_clip(_layout.minimum_bounding_box<int>());
			}
			virtual void _render(platform::renderer_base&) const = 0;
			virtual void _on_postrender(platform::renderer_base &r) const {
				r.pop_clip();
			}
			virtual void _on_render(platform::renderer_base &r) const {
				if (_vis & static_cast<unsigned char>(visibility::render_only)) {
					_on_prerender(r);
					_render(r);
					_on_postrender(r);
				}
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
			virtual void _recalc_layout();
			virtual void _finish_layout() {
				invalidate_visual();
			}

			virtual void _initialize() {
			}
		};
	}
}
