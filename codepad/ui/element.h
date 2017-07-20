#pragma once

#include <list>

#include "../utilities/hotkey_registry.h"
#include "../utilities/textconfig.h"
#include "../utilities/event.h"
#include "../utilities/misc.h"
#include "../os/renderer.h"
#include "../os/input.h"
#include "visual.h"

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
		struct mouse_button_info {
			mouse_button_info(os::input::mouse_button mb, vec2d pos) : button(mb), position(pos) {
			}
			const os::input::mouse_button button;
			const vec2d position;
			bool focus_set() const {
				return _focus_set;
			}
			void mark_focus_set() {
				assert(!_focus_set);
				_focus_set = true;
			}
		protected:
			bool _focus_set = false;
		};
		struct key_info {
			key_info(os::input::key k) : key(k) {
			}
			const os::input::key key;
		};
		struct text_info {
			text_info(char_t c) : character(c) {
			}
			const char_t character;
		};

		struct thickness {
			constexpr explicit thickness(double uni = 0.0) : left(uni), top(uni), right(uni), bottom(uni) {
			}
			constexpr thickness(double l, double t, double r, double b) : left(l), top(t), right(r), bottom(b) {
			}
			double left, top, right, bottom;

			constexpr rectd extend(rectd r) const {
				return rectd(r.xmin - left, r.xmax + right, r.ymin - top, r.ymax + bottom);
			}
			constexpr rectd shrink(rectd r) const {
				return rectd(r.xmin + left, r.xmax - right, r.ymin + top, r.ymax - bottom);
			}

			constexpr double width() const {
				return left + right;
			}
			constexpr double height() const {
				return top + bottom;
			}
			constexpr vec2d size() const {
				return vec2d(width(), height());
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
		class element;
		typedef hotkey_group<void(element*)> element_hotkey_group;
		class manager;
#ifndef NDEBUG
		struct control_dispose_rec {
			~control_dispose_rec() {
				assert(reg_created == disposed && reg_disposed == disposed);
			}
			size_t reg_created = 0, disposed = 0, reg_disposed = 0;
		};
		extern control_dispose_rec _dispose_rec;
#endif

		class element {
			friend class manager;
			friend class element_collection;
			friend class panel_base;
			friend class os::window_base;
		public:
			virtual ~element() {
#ifndef NDEBUG
				++_dispose_rec.disposed;
#endif
			}

			rectd get_layout() const {
				return _layout;
			}
			rectd get_client_region() const {
				return _clientrgn;
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
				if (test_bit_all(static_cast<unsigned char>(v) ^ _vis, visibility::render_only)) {
					invalidate_visual();
				}
				_vis = static_cast<unsigned char>(v);
			}
			virtual visibility get_visibility() const {
				return static_cast<visibility>(_vis);
			}

			virtual bool hit_test(vec2d p) const {
				return test_bit_all(_vis, visibility::interaction_only) && _layout.contains(p);
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

			virtual void set_can_focus(bool v) {
				_can_focus = v;
			}
			virtual bool get_can_focus() const {
				return _can_focus;
			}

			virtual os::window_base *get_window();
			virtual void set_default_hotkey_group(const element_hotkey_group *hgp) {
				_hotkey_gp = hgp;
			}
			virtual std::vector<const element_hotkey_group*> get_hotkey_groups() const {
				if (_hotkey_gp) {
					return {_hotkey_gp};
				}
				return std::vector<const element_hotkey_group*>();
			}

			void invalidate_visual();
			void invalidate_layout();
			void revalidate_layout();

			template <typename T, typename ...Args> inline static T *create(Args &&...arg) {
				static_assert(std::is_base_of<element, T>::value, "cannot create non-element");
				element *elem = new T(std::forward<Args>(arg)...);
#ifndef NDEBUG
				++_dispose_rec.reg_created;
#endif
				elem->_initialize();
#ifdef CP_DETECT_USAGE_ERRORS
				assert_true_usgerr(elem->_initialized, "element::_initialize() must be called by children classes");
#endif
				return static_cast<T*>(elem);
			}

			event<void> mouse_enter, mouse_leave, got_focus, lost_focus;
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
			std::list<element*>::iterator _text_tok;
			// layout result
			rectd _layout, _clientrgn;
			// layout params
			unsigned char _anchor = static_cast<unsigned char>(anchor::all);
			bool _has_width = false, _has_height = false;
			double _width, _height;
			thickness _margin, _padding;
			unsigned char _vis = static_cast<unsigned char>(visibility::visible);
			// input
			bool _mouse_over = false, _can_focus = true;
			cursor _crsr = cursor::not_specified;
			const element_hotkey_group *_hotkey_gp = nullptr;

			virtual void _on_mouse_enter() {
				_mouse_over = true;
				mouse_enter.invoke();
			}
			virtual void _on_mouse_leave() {
				_mouse_over = false;
				mouse_leave.invoke();
			}
			virtual void _on_got_focus() {
				got_focus.invoke();
			}
			virtual void _on_lost_focus() {
				lost_focus.invoke();
			}
			virtual void _on_mouse_move(mouse_move_info &p) {
				mouse_move.invoke(p);
			}
			virtual void _on_mouse_down(mouse_button_info&);
			virtual void _on_mouse_up(mouse_button_info &p) {
				mouse_up.invoke(p);
			}
			virtual void _on_mouse_scroll(mouse_scroll_info &p) {
				mouse_scroll.invoke(p);
			}
			virtual void _on_key_down(key_info &p) {
				key_down.invoke(p);
			}
			virtual void _on_key_up(key_info &p) {
				key_up.invoke(p);
			}
			virtual void _on_keyboard_text(text_info &p) {
				keyboard_text.invoke(p);
			}

			virtual void _on_padding_changed() {
				invalidate_visual();
			}

			virtual void _on_capture_lost() {
			}

			virtual void _on_update() {
			}

			virtual void _on_prerender() const {
#ifndef NDEBUG
				texture_brush(colord(1.0, 1.0, 1.0, 0.02)).fill_rect(get_layout());
				pen p(colord(1.0, 1.0, 1.0, 1.0));
				std::vector<vec2d> lines;
				lines.push_back(get_layout().xmin_ymin());
				lines.push_back(get_layout().xmax_ymin());
				lines.push_back(get_layout().xmin_ymin());
				lines.push_back(get_layout().xmin_ymax());
				lines.push_back(get_layout().xmax_ymax());
				lines.push_back(get_layout().xmin_ymax());
				lines.push_back(get_layout().xmax_ymax());
				lines.push_back(get_layout().xmax_ymin());
				p.draw_lines(lines);
#endif
				os::renderer_base::get().push_clip(_layout.minimum_bounding_box<int>());
			}
			virtual void _render() const = 0;
			virtual void _on_postrender() const {
				os::renderer_base::get().pop_clip();
			}
			virtual void _on_render() const {
				if (test_bit_all(_vis, visibility::render_only)) {
					_on_prerender();
					_render();
					_on_postrender();
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
			virtual void _recalc_layout(rectd);
			virtual void _finish_layout() {
				invalidate_visual();
			}

			virtual void _initialize() {
#ifdef CP_DETECT_USAGE_ERRORS
				_initialized = true;
#endif
			}
			virtual void _dispose();
#ifdef CP_DETECT_USAGE_ERRORS
		private:
			bool _initialized = false;
#endif
		};
	}
}
