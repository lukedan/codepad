#pragma once

#include "../utilities/textconfig.h"
#include "../utilities/textproc.h"
#include "../os/renderer.h"
#include "font.h"

namespace codepad {
	namespace ui {
		struct line_character_iterator {
		public:
			line_character_iterator(const str_t &s, ui::font_family ff, double tabsize) :
				_cc(s.begin()), _end(s.end()), _ff(std::move(ff)), _tabw(tabsize * _ff.maximum_width()) {
			}

			bool end() const {
				return _cc == _end;
			}
			bool next(ui::font_style fs) {
				if (_cc == _end) {
					return false;
				}
				_pos += _ndiff;
				_curc = *_cc;
				_cet = &_ff.get_by_style(fs)->get_char_entry(_curc);
				if (_curc == '\t') {
					_cw = _tabw * (std::floor(_pos / _tabw) + 1.0) - _pos;
				} else {
					_cw = _cet->advance;
				}
				_ndiff = _cw;
				if (++_cc != _end && fs == _lstyle) {
					_ndiff += _ff.get_by_style(fs)->get_kerning(_curc, *_cc).x;
				}
				_ndiff = std::round(_ndiff);
				_lstyle = fs;
				return true;
			}

			double char_left() const {
				return _pos;
			}
			double char_right() const {
				return _pos + _cw;
			}
			double next_char_left() const {
				return _pos + _ndiff;
			}
			char_t current_char() const {
				return _curc;
			}
			const ui::font::entry &current_char_entry() const {
				return *_cet;
			}
		protected:
			ui::font_style _lstyle = ui::font_style::normal;
			str_t::const_iterator _cc, _end;
			ui::font_family _ff;
			double _ndiff = 0.0, _cw = 0.0, _pos = 0.0, _tabw;
			char_t _curc = U'\0';
			const ui::font::entry *_cet = nullptr;
		};

		namespace text_renderer {
			inline void render_plain_text(const str_t &str, const font &fnt, vec2d topleft, colord color) {
				int sx = static_cast<int>(std::round(topleft.x)), dy = static_cast<int>(std::ceil(fnt.height()));
				vec2i cur = vec2i(sx, static_cast<int>(std::round(topleft.y)));
				char_t last = U'\0';
				double lastw = 0.0;
				for (auto i = str.begin(); i != str.end(); ++i) {
					if (is_newline(*i)) {
						cur.x = sx;
						cur.y += dy;
						last = U'\0';
					} else {
						const font::entry &et = fnt.get_char_entry(*i);
						if (last != U'\0') {
							cur.x += static_cast<int>(std::round(lastw + fnt.get_kerning(last, *i).x));
						}
						os::renderer_base::get().draw_character(et.texture, cur.convert<double>() + et.placement.xmin_ymin(), color);
						last = *i;
						lastw = et.advance;
					}
				}
			}
			inline vec2d measure_plain_text(const str_t &str, const font &fnt) {
				char_t last = U'\0';
				double lastw = 0.0, curline = 0.0, maxw = 0.0;
				size_t linen = 1;
				for (auto i = str.begin(); i != str.end(); ++i) {
					const font::entry &et = fnt.get_char_entry(*i);
					if (last != U'\0') {
						curline += static_cast<int>(std::round(lastw + fnt.get_kerning(last, *i).x));
					}
					if (is_newline(*i)) {
						++linen;
						last = U'\0';
						maxw = std::max(maxw, curline + et.advance);
						lastw = curline = 0.0;
					} else {
						last = *i;
						lastw = et.advance;
					}
				}
				return vec2d(std::max(maxw, curline + lastw), static_cast<double>(linen) * std::ceil(fnt.height()));
			}
		}
	}
}
