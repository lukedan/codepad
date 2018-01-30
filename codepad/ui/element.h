#pragma once

#include <list>

#include "../utilities/hotkey_registry.h"
#include "../utilities/encodings.h"
#include "../utilities/event.h"
#include "../utilities/misc.h"
#include "../os/renderer.h"
#include "../os/misc.h"
#include "element_classes.h"
#include "visual.h"
#include "draw.h"

namespace codepad::ui {
	class panel_base;

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
		friend class visual_state;
		friend class visual;
	public:
		constexpr static int
			max_zindex = std::numeric_limits<int>::max(),
			min_zindex = std::numeric_limits<int>::min();

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
			return _rst.test_state_bit(visual::get_predefined_states().mouse_over);
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
			_anchor = a;
			invalidate_layout();
		}
		anchor get_anchor() const {
			return static_cast<anchor>(_anchor);
		}

		void set_visibility(visibility v) {
			if (test_bit_all(static_cast<unsigned>(v) ^ static_cast<unsigned>(_vis), visibility::render_only)) {
				invalidate_visual();
			}
			_vis = v;
		}
		visibility get_visibility() const {
			return static_cast<visibility>(_vis);
		}

		virtual bool hit_test(vec2d p) const {
			return test_bit_all(_vis, visibility::interaction_only) && _layout.contains(p);
		}

		virtual os::cursor get_default_cursor() const {
			return os::cursor::normal;
		}
		virtual void set_overriden_cursor(os::cursor c) {
			_crsr = c;
		}
		virtual os::cursor get_overriden_cursor() const {
			return _crsr;
		}
		virtual os::cursor get_current_display_cursor() const {
			if (_crsr == os::cursor::not_specified) {
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

		void set_zindex(int);
		int get_zindex() const {
			return _zindex;
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

		template <typename T, typename ...Args> inline static T *create(Args &&...args) {
			static_assert(std::is_base_of_v<element, T>, "cannot create non-element");
			element *elem = new T(std::forward<Args>(args)...);
#ifdef CP_DETECT_LOGICAL_ERRORS
			++control_dispose_rec::get().reg_created;
#endif
			elem->_initialize();
#ifdef CP_DETECT_USAGE_ERRORS
			assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
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
			if (clientmin > clientmax) {
				if (!anchormin && !anchormax) {
					clientmin = clientmax =
						(clientmin * marginmax + clientmax * marginmin) / (marginmin + marginmax);
				} else {
					clientmin = clientmax = 0.5 * (clientmin + clientmax);
				}
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
		std::list<element*>::iterator _col_token;
		// layout result
		rectd _layout, _clientrgn;
		// layout params
		anchor _anchor = anchor::all;
		bool _has_width = false, _has_height = false;
		double _width = 0.0, _height = 0.0;
		thickness _margin, _padding;
		visibility _vis = visibility::visible;
		int _zindex = 0;
		// input
		bool _can_focus = true;
		os::cursor _crsr = os::cursor::not_specified;
		// visual info
		visual::render_state _rst;

		virtual void _on_mouse_enter() {
			mouse_enter.invoke();
			_set_visual_style_bit(visual::get_predefined_states().mouse_over, true);
		}
		virtual void _on_mouse_leave() {
			mouse_enter.invoke();
			_set_visual_style_bit(visual::get_predefined_states().mouse_over, false);
		}
		virtual void _on_got_focus() {
			got_focus.invoke();
			_set_visual_style_bit(visual::get_predefined_states().focused, true);
		}
		virtual void _on_lost_focus() {
			lost_focus.invoke();
			_set_visual_style_bit(visual::get_predefined_states().focused, false);
		}
		virtual void _on_mouse_move(mouse_move_info &p) {
			mouse_move.invoke(p);
		}
		virtual void _on_mouse_down(mouse_button_info&);
		virtual void _on_mouse_up(mouse_button_info &p) {
			mouse_up.invoke(p);
			_set_visual_style_bit(visual::get_predefined_states().mouse_down, false);
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

		void _set_visual_style_bit(visual_state::id_t id, bool v) {
			_rst.set_state_bit(id, v);
			_on_visual_state_changed();
		}

		virtual void _on_padding_changed() {
			invalidate_visual();
		}

		virtual void _on_capture_lost() {
		}

		virtual void _on_update() {
		}

		virtual void _on_visual_state_changed() {
			invalidate_visual();
		}

		virtual void _on_prerender() {
			os::renderer_base::get().push_clip(_layout.fit_grid_enlarge<int>());
		}
		virtual void _custom_render() {
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

	class decoration {
		friend class os::window_base;
	public:
		decoration() = default;
		decoration(decoration&&) = delete;
		decoration(const decoration&) = delete;
		decoration &operator=(decoration&&) = delete;
		decoration &operator=(const decoration&) = delete;

		void set_layout(rectd r) {
			_layout = r;
			_on_visual_changed();
		}
		rectd get_layout() const {
			return _layout;
		}

		void set_class(str_t cls) {
			_st.set_class(std::move(cls));
			_on_visual_changed();
		}
		const str_t &get_class() const {
			return _st.get_class();
		}

		void set_state(visual_state::id_t id) {
			_st.set_state(id);
			_on_visual_changed();
		}
		visual_state::id_t get_state() const {
			return _st.get_state();
		}

		os::window_base *get_window() const {
			return _wnd;
		}
	protected:
		rectd _layout;
		visual::render_state _st;
		os::window_base *_wnd = nullptr;
		std::list<decoration*>::const_iterator _tok;

		bool _update_and_render() {
			return _st.update_and_render(_layout);
		}

		void _on_visual_changed();
	};
}
