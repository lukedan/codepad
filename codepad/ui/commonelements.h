#pragma once

#include "element.h"
#include "panel.h"
#include "textrenderer.h"
#include "../utilities/misc.h"
#include "../utilities/textconfig.h"
#include "../utilities/font.h"

namespace codepad {
	namespace ui {
		enum class orientation {
			horizontal,
			vertical
		};

		class content_host {
		public:
			content_host(element &p) : _parent(p) {
			}

			void set_text(const str_t &s) {
				_text = s;
				_mark_text_size_change();
			}
			const str_t &get_text() const {
				return _text;
			}

			void set_font(const font *fnt) {
				_fnt = fnt;
				_mark_text_size_change();
			}
			const font *get_font() const {
				return _fnt;
			}

			void set_color(colord c) {
				_clr = c;
				_parent.invalidate_visual();
			}
			colord get_color() const {
				return _clr;
			}

			inline static void set_default_font(const font *fnt) {
				_def_fnt = fnt;
				_def_fnt_ts = ((_def_fnt_ts + 1) & _mask_szcachets) | _mask_hasszcache;
			}
			inline static const font *get_default_font() {
				return _def_fnt;
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
						return _sz_cch = text_renderer::measure_plain_text(_text, *_fnt);
					}
				} else if (_def_fnt) {
					if (_sz_cached == _def_fnt_ts) {
						return _sz_cch;
					} else {
						_sz_cached = _def_fnt_ts;
						return _sz_cch = text_renderer::measure_plain_text(_text, *_def_fnt);
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
				const font *f = _fnt ? _fnt : _def_fnt;
				if (f) {
					text_renderer::render_plain_text(_text, *f, get_text_position(), _clr);
				}
			}
		protected:
			constexpr static unsigned char _mask_hasszcache = 0x80, _mask_szcachets = 0x7F;

			// size cache
			mutable unsigned char _sz_cached = 0;
			mutable vec2d _sz_cch;
			// text stuff
			str_t _text;
			const font *_fnt = nullptr;
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
			static const font *_def_fnt;
		};

		class label : public element {
			friend class element;
		public:
			content_host &content() {
				return _content;
			}
			const content_host &content() const {
				return _content;
			}

			vec2d get_desired_size() const override {
				return _content.get_text_size() + _padding.size();
			}
		protected:
			void _render() const override {
				_content.render();
			}

			void _initialize() override {
				element::_initialize();
				_can_focus = false;
			}

			content_host _content{ *this };
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
		protected:
			unsigned char _state = static_cast<unsigned char>(state::normal);

			void _on_mouse_enter(void_info &p) override {
				set_bit(_state, state::mouse_over);
				element::_on_mouse_enter(p);
			}
			void _on_mouse_leave(void_info &p) override {
				unset_bit(_state, state::mouse_over);
				if (test_bit(_state, state::mouse_down)) {
					manager::default().schedule_update(this);
				}
				element::_on_mouse_leave(p);
			}
			void _on_mouse_down(mouse_button_info &p) override {
				if (p.button == platform::input::mouse_button::left) {
					set_bit(_state, state::mouse_down);
				}
				element::_on_mouse_down(p);
			}
			void _on_mouse_up(mouse_button_info &p) override {
				bool valid = test_bit(_state, state::mouse_down);
				if (p.button == platform::input::mouse_button::left) {
					unset_bit(_state, state::mouse_down);
				}
				if (valid) {
					_on_click();
				}
				element::_on_mouse_up(p);
			}

			void _on_update() override {
				if (!platform::input::is_mouse_button_down(platform::input::mouse_button::left)) {
					unset_bit(_state, state::mouse_down);
				} else if (!test_bit(_state, state::mouse_over)) {
					manager::default().schedule_update(this);
				}
			}

			virtual void _on_click() = 0;
		};
		class button : public button_base {
			friend class element;
		public:
			content_host &content() {
				return _content;
			}
			const content_host &content() const {
				return _content;
			}

			vec2d get_desired_size() const override {
				return _content.get_text_size() + _padding.size();
			}

			event<void_info> click;
		protected:
			void _on_click() override {
				click.invoke_noret();
			}

			void _render() const override {
				_content.render();
			}

			content_host _content{ *this };
		};

		class stack_panel : public panel_base {
			friend class element;
		public:
		protected:
		};
	}
}
