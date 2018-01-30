#pragma once

#include "element.h"
#include "panel.h"
#include "draw.h"
#include "font_family.h"
#include "../utilities/misc.h"
#include "../utilities/encodings.h"
#include "../os/window.h"

namespace codepad::ui {
	enum class orientation {
		horizontal,
		vertical
	};

	class content_host {
		friend struct codepad::globals;
	public:
		content_host(element &p) : _parent(p) {
		}

		void set_text(str_t s) {
			_text = std::move(s);
			_mark_text_size_change();
		}
		const str_t &get_text() const {
			return _text;
		}

		void set_font(std::shared_ptr<const os::font> fnt) {
			_fnt = fnt;
			_mark_text_size_change();
		}
		std::shared_ptr<const os::font> get_font() const {
			return _fnt;
		}

		void set_color(colord c) {
			_clr = c;
			_parent.invalidate_visual();
		}
		colord get_color() const {
			return _clr;
		}

		inline static void set_default_font(std::shared_ptr<const os::font> fnt) {
			_get_deffnt() = std::move(fnt);
			_def_fnt_ts = static_cast<unsigned char>(((_def_fnt_ts + 1) & _mask_szcachets) | _mask_hasszcache);
		}
		inline static std::shared_ptr<const os::font> get_default_font() {
			return _get_deffnt();
		}

		void set_text_offset(vec2d o) {
			_textoffset = o;
			_parent.invalidate_layout();
		}
		vec2d get_text_offset() const {
			return _textoffset;
		}

		virtual vec2d get_text_size() const {
			if (_fnt) {
				if ((_sz_cached & _mask_hasszcache) != 0) {
					return _sz_cch;
				} else {
					_sz_cached = _mask_hasszcache;
					return _sz_cch = text_renderer::measure_plain_text(_text, _fnt);
				}
			} else {
				auto &&fnt = _get_deffnt();
				if (fnt) {
					if (_sz_cached == _def_fnt_ts) {
						return _sz_cch;
					} else {
						_sz_cached = _def_fnt_ts;
						return _sz_cch = text_renderer::measure_plain_text(_text, fnt);
					}
				}
			}
			return vec2d();
		}
		vec2d get_text_position() const {
			rectd inner = _parent.get_client_region();
			vec2d spare = inner.size() - get_text_size();
			return inner.xmin_ymin() + vec2d(spare.x * _textoffset.x, spare.y * _textoffset.y);
		}

		void render() const {
			auto &&f = _fnt ? _fnt : _get_deffnt();
			if (f) {
				text_renderer::render_plain_text(_text, f, get_text_position(), _clr);
			}
		}
	protected:
		constexpr static unsigned char _mask_hasszcache = 0x80, _mask_szcachets = 0x7F;

		// size cache
		mutable unsigned char _sz_cached = 0;
		mutable vec2d _sz_cch;
		// text stuff
		str_t _text;
		std::shared_ptr<const os::font> _fnt;
		colord _clr;
		vec2d _textoffset;
		// parent
		element &_parent;

		virtual void _mark_text_size_change() {
			_sz_cached = 0;
			if (!_parent.has_width() || !_parent.has_height()) {
				_parent.invalidate_layout();
			}
			_parent.invalidate_visual();
		}

		static unsigned char _def_fnt_ts;
		struct _default_font {
			std::shared_ptr<const os::font> font;
		};
		static std::shared_ptr<const os::font> &_get_deffnt();
	};

	class label : public element {
		friend class element;
	public:
		content_host & content() {
			return _content;
		}
		const content_host &content() const {
			return _content;
		}

		vec2d get_desired_size() const override {
			return _content.get_text_size() + _padding.size();
		}

		inline static str_t get_default_class() {
			return CP_STRLIT("label");
		}
	protected:
		void _custom_render() override {
			_content.render();
		}

		void _initialize() override {
			element::_initialize();
			_can_focus = false;
		}

		content_host _content{*this};
	};

	class button_base : public element {
		friend class element;
	public:
		enum class state : unsigned char {
			normal = 0,
			mouse_over = 1,
			mouse_down = 2,
			pressed = mouse_over | mouse_down
		};
		enum class trigger_type {
			mouse_down,
			mouse_up
		};

		inline static str_t get_default_class() {
			return CP_STRLIT("button_base");
		}
	protected:
		unsigned char _state = static_cast<unsigned char>(state::normal);
		trigger_type _trigtype = trigger_type::mouse_up;
		os::input::mouse_button _trigbtn = os::input::mouse_button::left;

		virtual void _on_state_changed() {
		}

		void _set_state(unsigned char s) {
			if (_state != s) {
				_state = s;
				_on_state_changed();
			}
		}

		void _on_mouse_enter() override {
			_set_state(with_bit_set(_state, state::mouse_over));
			element::_on_mouse_enter();
		}
		void _on_mouse_leave() override {
			_set_state(with_bit_unset(_state, state::mouse_over));
			element::_on_mouse_leave();
		}
		void _on_mouse_down(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position);
			if (p.button == _trigbtn) {
				_set_state(with_bit_set(_state, state::mouse_down));
				get_window()->set_mouse_capture(*this);
				if (_trigtype == trigger_type::mouse_down) {
					_on_click();
				}
			}
			element::_on_mouse_down(p);
		}
		void _on_mouse_lbutton_up() {
			if (test_bit_all(_state, state::mouse_down)) {
				get_window()->release_mouse_capture();
			}
			_set_state(with_bit_unset(_state, state::mouse_down));
		}
		void _on_capture_lost() override {
			_on_mouse_lbutton_up();
		}
		void _on_mouse_up(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position);
			bool valid = test_bit_all(_state, state::pressed);
			if (p.button == _trigbtn) {
				_on_mouse_lbutton_up();
			}
			if (valid && _trigtype == trigger_type::mouse_up) {
				_on_click();
			}
			element::_on_mouse_up(p);
		}
		void _on_update_mouse_pos(vec2d pos) {
			if (hit_test(pos)) {
				_set_state(with_bit_set(_state, state::mouse_over));
			} else {
				_set_state(with_bit_unset(_state, state::mouse_over));
			}
		}
		void _on_mouse_move(mouse_move_info &p) override {
			_on_update_mouse_pos(p.new_pos);
			element::_on_mouse_move(p);
		}

		virtual void _on_click() = 0;
	};
	class button : public button_base {
		friend class element;
	public:
		state get_state() const {
			return static_cast<state>(_state);
		}

		void set_trigger_button(os::input::mouse_button btn) {
			_trigbtn = btn;
		}
		os::input::mouse_button get_trigger_button() const {
			return _trigbtn;
		}

		void set_trigger_type(trigger_type t) {
			_trigtype = t;
		}
		trigger_type get_trigger_type() const {
			return _trigtype;
		}

		event<void> click;

		inline static str_t get_default_class() {
			return CP_STRLIT("button");
		}
	protected:
		void _on_click() override {
			click.invoke();
		}
	};

	class scroll_bar;
	class scroll_bar_drag_button : public button_base {
		friend class scroll_bar;
	public:
		inline static str_t get_default_class() {
			return CP_STRLIT("scrollbar_drag_button");
		}
	protected:
		scroll_bar * _get_bar() const;
		double _doffset = 0.0;

		void _initialize() override {
			button_base::_initialize();
			_trigtype = trigger_type::mouse_down;
			set_can_focus(false);
		}

		void _on_click() override {
		}

		void _on_mouse_down(mouse_button_info&) override;
		void _on_mouse_move(mouse_move_info&) override;
	};
	class scroll_bar : public panel_base {
		friend class element;
		friend class scroll_bar_drag_button;
	public:
		void set_orientation(orientation o) {
			if (_ori != o) {
				_ori = o;
				invalidate_layout();
			}
		}
		orientation get_orientation() const {
			return _ori;
		}

		vec2d get_desired_size() const override {
			return vec2d(10.0, 10.0) + get_padding().size();
		}

		void set_value(double v) {
			double ov = _curv;
			_curv = clamp(v, 0.0, _totrng - _range);
			invalidate_layout();
			value_changed.invoke_noret(ov);
		}
		double get_value() const {
			return _curv;
		}
		void set_params(double tot, double vis) {
			assert_true_usage(vis <= tot, "scrollbar visible range too large");
			_totrng = tot;
			_range = vis;
			set_value(_curv);
		}
		double get_total_range() const {
			return _totrng;
		}
		double get_visible_range() const {
			return _range;
		}

		void make_point_visible(double v) {
			if (_curv > v) {
				set_value(v);
			} else {
				v -= _range;
				if (_curv < v) {
					set_value(v);
				}
			}
		}

		bool override_children_layout() const override {
			return true;
		}

		event<value_update_info<double>> value_changed;

		inline static str_t get_default_class() {
			return CP_STRLIT("scrollbar");
		}
		inline static str_t get_page_up_button_class() {
			return CP_STRLIT("scrollbar_page_up_button");
		}
		inline static str_t get_page_down_button_class() {
			return CP_STRLIT("scrollbar_page_down_button");
		}
	protected:
		orientation _ori = orientation::vertical;
		double _totrng = 1.0, _curv = 0.0, _range = 0.1;
		scroll_bar_drag_button *_drag = nullptr;
		button *_pgup = nullptr, *_pgdn = nullptr;

		void _finish_layout() override {
			rectd cln = get_client_region();
			if (_ori == orientation::vertical) {
				double
					tszratio = cln.height() / _totrng,
					mid1 = cln.ymin + tszratio * _curv,
					mid2 = mid1 + tszratio * _range;
				_child_set_layout(_drag, rectd(cln.xmin, cln.xmax, mid1, mid2));
				_child_set_layout(_pgup, rectd(cln.xmin, cln.xmax, cln.ymin, mid1));
				_child_set_layout(_pgdn, rectd(cln.xmin, cln.xmax, mid2, cln.ymax));
			} else {
				double
					tszratio = cln.width() / _totrng,
					mid1 = cln.xmin + tszratio * _curv,
					mid2 = mid1 + tszratio * _range;
				_child_set_layout(_drag, rectd(mid1, mid2, cln.ymin, cln.ymax));
				_child_set_layout(_pgup, rectd(cln.xmin, mid1, cln.ymin, cln.ymax));
				_child_set_layout(_pgdn, rectd(mid2, cln.xmax, cln.ymin, cln.ymax));
			}
			element::_finish_layout();
		}

		void _initialize() override {
			panel_base::_initialize();
			_drag = element::create<scroll_bar_drag_button>();
			_children.add(*_drag);
			_pgup = element::create<button>();
			_pgup->set_trigger_type(button_base::trigger_type::mouse_down);
			_pgup->set_can_focus(false);
			_pgup->set_class(get_page_up_button_class());
			_pgup->click += [this]() {
				set_value(get_value() - get_visible_range());
			};
			_children.add(*_pgup);
			_pgdn = element::create<button>();
			_pgdn->set_trigger_type(button_base::trigger_type::mouse_down);
			_pgdn->set_can_focus(false);
			_pgdn->set_class(get_page_down_button_class());
			_pgdn->click += [this]() {
				set_value(get_value() + get_visible_range());
			};
			_children.add(*_pgdn);
			_can_focus = false;
		}
	};
	inline scroll_bar *scroll_bar_drag_button::_get_bar() const {
#ifdef CP_DETECT_LOGICAL_ERRORS
		auto res = dynamic_cast<scroll_bar*>(_parent);
		assert_true_logical(res != nullptr, "the button is not a child of a scroll bar");
		return res;
#else
		return static_cast<scroll_bar*>(_parent);
#endif
	}
	inline void scroll_bar_drag_button::_on_mouse_down(mouse_button_info &p) {
		if (p.button == _trigbtn) {
			scroll_bar *b = _get_bar();
			if (b->get_orientation() == orientation::horizontal) {
				_doffset = p.position.x - get_layout().xmin;
			} else {
				_doffset = p.position.y - get_layout().ymin;
			}
		}
		button_base::_on_mouse_down(p);
	}
	inline void scroll_bar_drag_button::_on_mouse_move(mouse_move_info &p) {
		if (test_bit_all(_state, state::mouse_down)) {
			scroll_bar *b = _get_bar();
			double totsz, diff;
			if (b->get_orientation() == orientation::horizontal) {
				diff = p.new_pos.x - b->get_client_region().xmin - _doffset;
				totsz = b->get_client_region().width();
			} else {
				diff = p.new_pos.y - b->get_client_region().ymin - _doffset;
				totsz = b->get_client_region().height();
			}
			b->set_value(diff * b->get_total_range() / totsz);
		}
	}
}
