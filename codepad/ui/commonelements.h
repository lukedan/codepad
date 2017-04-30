#pragma once

#include "element.h"
#include "textrenderer.h"
#include "../utilities/misc.h"
#include "../utilities/textconfig.h"
#include "../utilities/font.h"

namespace codepad {
	namespace ui {
		class text_element : public element {
		public:
			void set_text(const str_t &s) {
				_text = s;
				_mark_text_size_change();
			}
			str_t get_text() const {
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
				invalidate_visual();
			}
			colord get_color() const {
				return _clr;
			}

			inline static void set_default_font(font *fnt) {
				_def_fnt = fnt;
				_def_fnt_ts = ((_def_fnt_ts + 1) & _mask_szcachets) | _mask_hasszcache;
			}
			inline static font *get_default_font() {
				return _def_fnt;
			}

			void set_text_offset(vec2d o) {
				_textoffset = o;
				invalidate_layout();
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
			vec2d get_desired_size() const override {
				return _padding.size() + get_text_size();
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

			void _on_padding_changed() override {
				invalidate_visual();
			}

			virtual vec2d _get_text_position() const {
				rectd inner = _padding.shrink(_layout);
				vec2d spare = inner.size() - get_text_size();
				return inner.xmin_ymin() + vec2d(spare.x * _textoffset.x, spare.y * _textoffset.y);
			}
			void _render(platform::renderer_base&) const override {
				const font *f = _fnt ? _fnt : _def_fnt;
				if (f) {
					text_renderer::render_plain_text(_text, *f, _get_text_position(), _clr);
				}
			}

			virtual void _mark_text_size_change() {
				_sz_cached = 0;
				if (!has_width() || !has_height()) {
					invalidate_layout();
				}
				invalidate_visual();
			}

			static unsigned char _def_fnt_ts;
			static font *_def_fnt;
		};
	}
}
