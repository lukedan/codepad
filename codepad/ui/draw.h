#pragma once

#include "../utilities/encodings.h"
#include "../os/renderer.h"
#include "font_family.h"

namespace codepad {
	namespace ui {
		struct character_metrics_accumulator {
		public:
			character_metrics_accumulator(ui::font_family ff, double tabsize) :
				_ff(std::move(ff)), _tabw(tabsize * _ff.normal->get_char_entry(' ').advance) {
			}

			void next(char32_t c, font_style fs) {
				_lastchar = _curchar;
				_laststyle = _curstyle;
				_lce = _pos + _cw;
				_update_raw(c, fs);
			}
			void replace_current(char32_t c, font_style fs) {
				_update_raw(c, fs);
			}
			void create_blank_before(double width) {
				if (_lastchar == '\0') {
					_lce = _pos;
					_pos += std::ceil(width);
				} else {
					_pos = _lce + std::ceil(width);
					_lastchar = '\0';
				};
				if (_curchar == '\t') {
					_cw = _get_target_tab_width();
				}
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
			char32_t current_char() const {
				return _curchar;
			}
			const os::font::entry &current_char_entry() const {
				return *_cet;
			}

			void set_tab_width(double tw) {
				_tabw = tw * _ff.maximum_width();
				if (_curchar == '\t') {
					_cw = _get_target_tab_width();
				}
			}

			void reset() {
				_laststyle = _curstyle = font_style::normal;
				_lce = _cw = _pos = 0.0;
				_lastchar = _curchar = '\0';
				_cet = nullptr;
			}

			const font_family &get_font_family() const {
				return _ff;
			}
		protected:
			font_style _laststyle = font_style::normal, _curstyle = font_style::normal;
			font_family _ff;
			double _lce = 0.0, _cw = 0.0, _pos = 0.0, _tabw;
			char32_t _lastchar = '\0', _curchar = '\0';
			const os::font::entry *_cet = nullptr;

			double _get_target_tab_width() const {
				return _tabw * (std::floor(_pos / _tabw) + 1.0) - _pos;
			}
			void _update_raw(char32_t c, font_style fs) {
				_curchar = c;
				_curstyle = fs;
				const auto &stylefnt = _ff.get_by_style(_curstyle);
				if (_lastchar != '\0') {
					_pos = _lce;
					if (_curstyle == _laststyle) {
						_pos += stylefnt->get_kerning(_lastchar, _curchar).x;
					}
				}
				_cet = &stylefnt->get_char_entry(_curchar);
				_pos = static_cast<int>(_pos + 0.5);
				if (_curchar == '\t') {
					_cw = _get_target_tab_width();
				} else {
					_cw = _cet->advance;
				}
			}
		};

		namespace text_renderer {
			template <typename Cont> inline void render_plain_text(
				Cont &&c, const std::shared_ptr<const os::font> &fnt, vec2d topleft, colord color
			) {
				render_plain_text(c.begin(), c.end(), fnt, topleft, color);
			}
			template <typename It> inline void render_plain_text(
				It &&beg, It &&end, const std::shared_ptr<const os::font> &fnt, vec2d topleft, colord color
			) {
				int sx = static_cast<int>(std::round(topleft.x)), dy = static_cast<int>(std::ceil(fnt->height()));
				vec2i cur = vec2i(sx, static_cast<int>(std::round(topleft.y)));
				char32_t last = '\0';
				double lastw = 0.0;
				for (codepoint_iterator_base<std::decay_t<It>> i(beg, end); !i.at_end(); i.next()) {
					if (is_newline(*i)) {
						cur.x = sx;
						cur.y += dy;
						last = '\0';
					} else {
						const os::font::entry &et = fnt->get_char_entry(*i);
						if (last != '\0') {
							cur.x += static_cast<int>(std::round(lastw + fnt->get_kerning(last, *i).x));
						}
						os::renderer_base::get().draw_character(
							et.texture, cur.convert<double>() + et.placement.xmin_ymin(), color
						);
						last = *i;
						lastw = et.advance;
					}
				}
			}

			template <typename Cont> inline vec2d measure_plain_text(
				Cont &&cont, const std::shared_ptr<const os::font> &fnt
			) {
				return measure_plain_text(cont.begin(), cont.end(), fnt);
			}
			template <typename It> inline vec2d measure_plain_text(
				It &&beg, It &&end, const std::shared_ptr<const os::font> &fnt
			) {
				char32_t last = '\0';
				double lastw = 0.0, curline = 0.0, maxw = 0.0;
				size_t linen = 1;
				for (codepoint_iterator_base<std::decay_t<It>> i(beg, end); !i.at_end(); i.next()) {
					const os::font::entry &et = fnt->get_char_entry(*i);
					if (last != '\0') {
						curline += static_cast<int>(std::round(lastw + fnt->get_kerning(last, *i).x));
					}
					if (is_newline(*i)) {
						++linen;
						last = '\0';
						maxw = std::max(maxw, curline + et.advance);
						lastw = curline = 0.0;
					} else {
						last = *i;
						lastw = et.advance;
					}
				}
				return vec2d(std::max(maxw, curline + lastw), static_cast<double>(linen) * std::ceil(fnt->height()));
			}
		}

		struct render_batch {
		public:
			void add_triangle(
				vec2d v1, vec2d v2, vec2d v3, vec2d uv1, vec2d uv2, vec2d uv3,
				colord c1, colord c2, colord c3
			) {
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

			void draw(const os::texture &tex) {
				os::renderer_base::get().draw_triangles(tex, _vs.data(), _uvs.data(), _cs.data(), _vs.size());
			}

			void reserve(size_t numtris) {
				numtris *= 3;
				_vs.reserve(numtris);
				_uvs.reserve(numtris);
				_cs.reserve(numtris);
			}
		protected:
			std::vector<vec2d> _vs, _uvs;
			std::vector<colord> _cs;
		};

		class basic_brush {
		public:
			virtual ~basic_brush() {
			}

			virtual void fill_rect(rectd) const = 0;
		};
		class texture_brush : public basic_brush {
		public:
			texture_brush() = default;
			texture_brush(colord cv) : color(cv) {
			}
			texture_brush(colord cv, os::texture &&tex) : color(cv), texture(std::move(tex)) {
			}

			void fill_rect(rectd r) const override {
				os::renderer_base::get().draw_quad(texture, r, rectd(0.0, 1.0, 0.0, 1.0), color);
			}

			colord color;
			os::texture texture;
		};

		class basic_pen {
		public:
			virtual ~basic_pen() {
			}

			virtual void draw_lines(const std::vector<vec2d>&) const = 0;
		};
		class pen : public basic_pen {
		public:
			pen() = default;
			pen(colord c) : color(c) {
			}

			void draw_lines(const std::vector<vec2d> &poss) const override {
				std::vector<colord> cs(poss.size(), color);
				os::renderer_base::get().draw_lines(poss.data(), cs.data(), poss.size());
			}

			colord color;
		};
	}
}
