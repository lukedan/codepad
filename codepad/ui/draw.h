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
				line_character_iterator(s.begin(), s.end(), ff, tabsize) {
			}
			line_character_iterator(
				str_t::const_iterator beg, str_t::const_iterator end,
				ui::font_family ff, double tabsize
			) : _cc(beg), _end(end), _ff(ff), _tabw(tabsize * ff.maximum_width()) {
			}

			void begin(ui::font_style fs) {
				_lce = _pos;
				if (_cc != _end) {
					_calc_char_metrics(fs);
					_lstyle = static_cast<int>(fs);
				}
			}
			void next(ui::font_style fs) {
				++_cc;
				_pos += _cw;
				_lce = _pos;
				if (_cc != _end) {
					if (static_cast<int>(fs) == _lstyle) {
						_pos += _ff.get_by_style(fs)->get_kerning(_curc, *_cc).x;
					}
					_pos = std::round(_pos);
					_calc_char_metrics(fs);
					_lstyle = static_cast<int>(fs);
				}
			}
			bool ended() const {
				return _cc == _end;
			}

			void create_blank(double width) {
				_lstyle = _invalid_style;
				_pos += std::ceil(width);
			}

			double char_left() const {
				return _pos;
			}
			double char_right() const {
				return _pos + _cw;
			}
			double prev_char_right() const {
				return _lce;
			}
			char_t current_char() const {
				return _curc;
			}
			const ui::font::entry &current_char_entry() const {
				return *_cet;
			}

			void set_tab_width(double tw) {
				_tabw = tw * _ff.maximum_width();
			}

			font_family get_font_family() const {
				return _ff;
			}
		protected:
			constexpr static int _invalid_style = -1; // cancels kerning

			int _lstyle = _invalid_style;
			str_t::const_iterator _cc, _end;
			ui::font_family _ff;
			double _lce = 0.0, _cw = 0.0, _pos = 0.0, _tabw;
			char_t _curc = U'\0';
			const ui::font::entry *_cet = nullptr;

			void _calc_char_metrics(ui::font_style fs) {
				_curc = *_cc;
				_cet = &_ff.get_by_style(fs)->get_char_entry(_curc);
				if (_curc == '\t') {
					_cw = _tabw * (std::floor(_pos / _tabw) + 1.0) - _pos;
				} else {
					_cw = _cet->advance;
				}
			}
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

		struct render_batch {
		public:
			void add_triangle(vec2d v1, vec2d v2, vec2d v3, vec2d uv1, vec2d uv2, vec2d uv3, colord c1, colord c2, colord c3) {
				_vs.push_back(v1);
				_vs.push_back(v2);
				_vs.push_back(v3);
				_uvs.push_back(uv1);
				_uvs.push_back(uv2);
				_uvs.push_back(uv3);
				_cs.push_back(c1);
				_cs.push_back(c2);
				_cs.push_back(c3);
			}
			void add_quad(rectd r, vec2d uvtl, vec2d uvtr, vec2d uvbl, vec2d uvbr, colord ctl, colord ctr, colord cbl, colord cbr) {
				add_triangle(r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(), uvtl, uvtr, uvbl, ctl, ctr, cbl);
				add_triangle(r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax(), uvtr, uvbr, uvbl, ctr, cbr, cbl);
			}
			void add_quad(rectd r, rectd uv, colord ctl, colord ctr, colord cbl, colord cbr) {
				add_quad(r, uv.xmin_ymin(), uv.xmax_ymin(), uv.xmin_ymax(), uv.xmax_ymax(), ctl, ctr, cbl, cbr);
			}
			void add_quad(rectd r, rectd uv, colord c) {
				add_quad(r, uv, c, c, c, c);
			}

			void draw(os::texture_id tex) {
				os::renderer_base::get().draw_triangles(_vs.data(), _uvs.data(), _cs.data(), _vs.size(), tex);
			}

			void reserve(size_t num) {
				num *= 3;
				_vs.reserve(num);
				_uvs.reserve(num);
				_cs.reserve(num);
			}
		protected:
			std::vector<vec2d> _vs, _uvs;
			std::vector<colord> _cs;
		};
	}
}
