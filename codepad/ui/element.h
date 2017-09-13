#pragma once

#include <list>

#include "../utilities/hotkey_registry.h"
#include "../utilities/textconfig.h"
#include "../utilities/event.h"
#include "../utilities/misc.h"
#include "../os/renderer.h"
#include "../os/misc.h"
#include "theme_providers.h"
#include "draw.h"

namespace codepad {
	namespace ui {
		struct mouse_move_info {
			explicit mouse_move_info(vec2d p) : new_pos(p) {
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
				assert_true_usage(!_handled, "event is being marked as handled twice");
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
				assert_true_usage(!_focus_set, "event is being marked as handled twice");
				_focus_set = true;
			}
		protected:
			bool _focus_set = false;
		};
		struct key_info {
			explicit key_info(os::input::key k) : key(k) {
			}
			const os::input::key key;
		};
		struct text_info {
			explicit text_info(str_t s) : content(std::move(s)) {
			}
			const str_t content;
		};

		class panel_base;
		class element;
		typedef hotkey_group<void(element*)> element_hotkey_group;
		class manager;
#ifdef CP_DETECT_LOGICAL_ERRORS
		struct control_dispose_rec {
			~control_dispose_rec() {
				assert_true_logical(
					reg_created == disposed && reg_disposed == disposed,
					"undisposed controls remaining"
				);
			}
			size_t reg_created = 0, disposed = 0, reg_disposed = 0;

			static control_dispose_rec &get();
		};
#endif

		class element {
			friend class manager;
			friend class element_collection;
			friend class panel_base;
			friend class os::window_base;
			friend class visual_provider_state;
			friend class visual_provider;
		public:
			element(const element&) = delete;
			element(element&&) = delete;
			element &operator=(const element&) = delete;
			element &operator=(element&&) = delete;
			virtual ~element() {
#ifdef CP_DETECT_LOGICAL_ERRORS
				++control_dispose_rec::get().disposed;
#endif
			}

			rectd get_layout() const {
				return _layout;
			}
			rectd get_client_region() const {
				return _clientrgn;
			}
			bool is_mouse_over() const {
				return _rst.test_state_bit(visual_manager::default_states().mouse_over);
			}

			panel_base *parent() const {
				return _parent;
			}

			void set_width(double w) {
				_width = w;
				_has_width = true;
				invalidate_layout();
			}
			void set_height(double h) {
				_height = h;
				_has_height = true;
				invalidate_layout();
			}
			void unset_width() {
				_has_width = false;
				invalidate_layout();
			}
			void unset_height() {
				_has_height = false;
				invalidate_layout();
			}
			void set_size(vec2d s) {
				_width = s.x;
				_height = s.y;
				_has_width = _has_height = true;
				invalidate_layout();
			}
			void unset_size() {
				_has_width = _has_height = false;
				invalidate_layout();
			}
			bool has_width() const {
				return _has_width;
			}
			bool has_height() const {
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

			void set_margin(thickness t) {
				_margin = t;
				invalidate_layout();
			}
			thickness get_margin() const {
				return _margin;
			}
			void set_padding(thickness t) {
				_padding = t;
				_on_padding_changed();
			}
			thickness get_padding() const {
				return _padding;
			}

			void set_anchor(anchor a) {
				_anchor = static_cast<unsigned char>(a);
				invalidate_layout();
			}
			anchor get_anchor() const {
				return static_cast<anchor>(_anchor);
			}

			void set_visibility(visibility v) {
				if (test_bit_all(static_cast<unsigned char>(v) ^ _vis, visibility::render_only)) {
					invalidate_visual();
				}
				_vis = static_cast<unsigned char>(v);
			}
			visibility get_visibility() const {
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

			void set_can_focus(bool v) {
				_can_focus = v;
			}
			bool get_can_focus() const {
				return _can_focus;
			}
			bool has_focus() const;

			os::window_base *get_window();
			void set_default_hotkey_group(const element_hotkey_group *hgp) {
				_hotkey_gp = hgp;
			}
			std::vector<const element_hotkey_group*> get_hotkey_groups() const {
				if (_hotkey_gp != nullptr) {
					return {_hotkey_gp};
				}
				return std::vector<const element_hotkey_group*>();
			}

			void set_class(str_t s) {
				_rst.set_class(std::move(s));
				invalidate_visual();
			}
			const str_t &get_class() const {
				return _rst.get_class();
			}

			void invalidate_visual();
			void invalidate_layout();
			void revalidate_layout();

			template <typename T> inline static T *create() {
				static_assert(std::is_base_of<element, T>::value, "cannot create non-element");
				element *elem = new T();
#ifdef CP_DETECT_LOGICAL_ERRORS
				++control_dispose_rec::get().reg_created;
#endif
				elem->_initialize();
#ifdef CP_DETECT_USAGE_ERRORS
				assert_true_usage(elem->_initialized, "element::_initialize() must be called by children classes");
#endif
				elem->set_class(T::get_default_class());
				return static_cast<T*>(elem);
			}

			inline static void layout_on_direction(
				bool anchormin, bool anchormax, double &clientmin, double &clientmax,
				double marginmin, double marginmax, double size
			) {
				if (anchormin && anchormax) {
					clientmin += marginmin;
					clientmax -= marginmax;
				} else if (anchormin) {
					clientmin += marginmin;
					clientmax = clientmin + size;
				} else if (anchormax) {
					clientmax -= marginmax;
					clientmin = clientmax - size;
				} else {
					clientmin += (clientmax - clientmin - size) * marginmin / (marginmin + marginmax);
					clientmax = clientmin + size;
				}
			}

			event<void> mouse_enter, mouse_leave, got_focus, lost_focus;
			event<mouse_move_info> mouse_move;
			event<mouse_button_info> mouse_down, mouse_up;
			event<mouse_scroll_info> mouse_scroll;
			event<key_info> key_down, key_up;
			event<text_info> keyboard_text;
		protected:
			element() = default;

			// children-parent stuff
			panel_base *_parent = nullptr;
			std::list<element*>::iterator _text_tok;
			// layout result
			rectd _layout, _clientrgn;
			// layout params
			unsigned char _anchor = static_cast<unsigned char>(anchor::all);
			bool _has_width = false, _has_height = false;
			double _width = 0.0, _height = 0.0;
			thickness _margin, _padding;
			unsigned char _vis = static_cast<unsigned char>(visibility::visible);
			// input
			bool _can_focus = true;
			cursor _crsr = cursor::not_specified;
			const element_hotkey_group *_hotkey_gp = nullptr;
			// visual info
			visual_provider::render_state _rst;

			virtual void _on_mouse_enter() {
				_set_state(visual_manager::default_states().mouse_over, true);
				mouse_enter.invoke();
			}
			virtual void _on_mouse_leave() {
				_set_state(visual_manager::default_states().mouse_over, false);
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

			virtual void _set_state(visual_state_id sid, bool set) {
				_rst.set_state_bit(sid, set);
				invalidate_visual();
			}

			virtual void _on_prerender() {
				texture_brush(has_focus() ? colord(0.0, 1.0, 0.0, 0.02) : colord(1.0, 1.0, 1.0, 0.02)).fill_rect(get_layout());
				pen p(has_focus() ? colord(0.0, 1.0, 0.0, 1.0) : colord(1.0, 1.0, 1.0, 0.3));
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
				os::renderer_base::get().push_clip(_layout.minimum_bounding_box<int>());
			}
			virtual void _custom_render() const {
			}
			virtual void _on_postrender() {
				os::renderer_base::get().pop_clip();
			}
			virtual void _on_render();

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

		inline rectd visual_layer::get_center_rect(const state &s, rectd client) const {
			element::layout_on_direction(
				test_bit_all(static_cast<int>(rect_anchor), anchor::left),
				test_bit_all(static_cast<int>(rect_anchor), anchor::right),
				client.xmin, client.xmax,
				s.current_margin.current_value.left, s.current_margin.current_value.right,
				s.current_size.current_value.x
			);
			element::layout_on_direction(
				test_bit_all(static_cast<int>(rect_anchor), anchor::top),
				test_bit_all(static_cast<int>(rect_anchor), anchor::bottom),
				client.ymin, client.ymax,
				s.current_margin.current_value.top, s.current_margin.current_value.bottom,
				s.current_size.current_value.y
			);
			return client;
		}
	}
}
