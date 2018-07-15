#pragma once

/// \file
/// Auxiliary classes for rendering text, primitives, etc.

#include "../core/encodings.h"
#include "../os/renderer.h"
#include "font_family.h"

namespace codepad::ui {
	/// Struct for measuring and layouting text. Note that it only does so for one line of text, and all line breaks
	/// are treated as normal characters, i.e., they don't start new lines. Note that no additional processing (e.g.,
	/// rounding) is performed.
	///
	/// \todo Implement subpixel character placement (not for this struct, but for all other struct (maybe in renderers)).
	struct character_metrics_accumulator {
	public:
		/// Constructs the struct, given a \ref font_family and a tab size.
		///
		/// \param ff The \ref font_family used for all the calculations.
		/// \param tabsize Specifies how many whitespaces the width of a tab is.
		character_metrics_accumulator(ui::font_family ff, double tabsize) :
			_ff(std::move(ff)), _tabw(tabsize * _ff.normal->get_char_entry(' ').advance) {
		}

		/// Appends a character to the end of the line, with the given \ref font_style.
		void next(char32_t c, font_style fs) {
			_lastchar = _curchar;
			_laststyle = _curstyle;
			_lce = _pos + _cw;
			replace_current(c, fs);
		}
		/// Replaces the current character with a given character with a specified font style.
		void replace_current(char32_t c, font_style fs) {
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
			if (_curchar == '\t') {
				_cw = _get_target_tab_width();
			} else {
				_cw = _cet->advance;
			}
		}
		/// Creates a blank of the specified width (in pixels) before the current character.
		void create_blank_before(double width) {
			if (_lastchar == '\0') {
				_lce = _pos;
				_pos += width;
			} else {
				_pos = _lce + width;
				_lastchar = '\0';
			};
			if (_curchar == '\t') {
				_cw = _get_target_tab_width();
			}
		}

		/// Returns the position of the left boundary of the current character.
		double char_left() const {
			return _pos;
		}
		/// Returns the position of the right boundary of the current character.
		double char_right() const {
			return _pos + _cw;
		}
		/// Returns the position of the right boundary of the previous character.
		double prev_char_right() const {
			return _lce;
		}
		/// Returns the current character.
		char32_t current_char() const {
			return _curchar;
		}
		/// Returns the os::font::entry for the current character.
		const os::font::entry &current_char_entry() const {
			return *_cet;
		}

		/// Returns the current font family in use.
		const font_family &get_font_family() const {
			return _ff;
		}

		/// Sets the width of the tab, in whitespaces.
		void set_tab_width(double tw) {
			_tabw = tw * _ff.normal->get_char_entry(' ').advance;
			if (_curchar == '\t') {
				_cw = _get_target_tab_width();
			}
		}

		/// Resets the struct to the initial state.
		void reset() {
			_laststyle = _curstyle = font_style::normal;
			_lce = _cw = _pos = 0.0;
			_lastchar = _curchar = '\0';
			_cet = nullptr;
		}
	protected:
		font_style
			_laststyle = font_style::normal, ///< The font style of the last char.
			_curstyle = font_style::normal; ///< The font style of the current char.
		font_family _ff; ///< The font family used for all the calculations.
		double
			_lce = 0.0, ///< The position of the right boundary of the previous character.
			_cw = 0.0, ///< The width of the current character.
			_pos = 0.0, ///< The position of the left boundary of the current character.
			_tabw = 0.0; ///< The width of a tab character, in pixels.
		char32_t
			_lastchar = '\0', ///< The last character.
			_curchar = '\0'; ///< The current character.
		const os::font::entry *_cet = nullptr; ///< The entry for the current character.

		/// Returns the width of the tab character, in pixels, considering its position.
		/// The tab may be shorter than the width specified by \ref _tabw.
		double _get_target_tab_width() const {
			return _tabw * (std::floor(_pos / _tabw) + 1.0) - _pos;
		}
	};

	/// Constains functions for rendering text.
	namespace text_renderer {
		/// Renders the given text, using the specified font, position, and color.
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
					if (last != '\0') {
						cur.x += static_cast<int>(std::round(lastw + fnt->get_kerning(last, *i).x));
					}
					os::font::entry &et = fnt->draw_character(*i, cur.convert<double>(), color);
					last = *i;
					lastw = et.advance;
				}
			}
		}
		/// \overload
		template <typename Cont> inline void render_plain_text(
			Cont &&c, const std::shared_ptr<const os::font> &fnt, vec2d topleft, colord color
		) {
			render_plain_text(c.begin(), c.end(), fnt, topleft, color);
		}

		/// Measures the size of the bounding box of the text, using the given font.
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
		/// \overload
		template <typename Cont> inline vec2d measure_plain_text(
			Cont &&cont, const std::shared_ptr<const os::font> &fnt
		) {
			return measure_plain_text(cont.begin(), cont.end(), fnt);
		}
	}

	/// Stores a batch of vertices for rendering to reduce the number of draw calls.
	struct render_batch {
	public:
		/// Adds a triangle to the render batch, given the coordinates, uvs, and colors of its vertices.
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
		/// Adds a rectangle to the render batch, given its coordinates, UV's, and color.
		void add_quad(rectd r, rectd uv, colord c) {
			add_quad(r, uv, c, c, c, c);
		}
		/// \overload
		/// \param r The rectangle.
		/// \param uv Rectangle of the texture.
		/// \param ctl Color of the top left corner of the rectangle.
		/// \param ctr Color of the top right corner of the rectangle.
		/// \param cbl Color of the bottom left corner of the rectangle.
		/// \param cbr Color of the bottom right corner of the rectangle.
		void add_quad(rectd r, rectd uv, colord ctl, colord ctr, colord cbl, colord cbr) {
			add_quad(r, uv.xmin_ymin(), uv.xmax_ymin(), uv.xmin_ymax(), uv.xmax_ymax(), ctl, ctr, cbl, cbr);
		}
		/// \overload
		/// \param r The rectangle.
		/// \param uvtl The UV of the top left corner of the rectangle.
		/// \param uvtr The UV of the top right corner of the rectangle.
		/// \param uvbl The UV of the bottom left corner of the rectangle.
		/// \param uvbr The UV of the bottom right corner of the rectangle.
		/// \param ctl Color of the top left corner of the rectangle.
		/// \param ctr Color of the top right corner of the rectangle.
		/// \param cbl Color of the bottom left corner of the rectangle.
		/// \param cbr Color of the bottom right corner of the rectangle.
		/// \remark This version allows the caller to set all four UV's of the rectangle,
		///         but since the rectangle is rendered as two traingles, settings the UV's in a
		///         non-rectangular fashion can cause improper stretching of the texture.
		void add_quad(rectd r, vec2d uvtl, vec2d uvtr, vec2d uvbl, vec2d uvbr, colord ctl, colord ctr, colord cbl, colord cbr) {
			add_triangle(r.xmin_ymin(), r.xmax_ymin(), r.xmin_ymax(), uvtl, uvtr, uvbl, ctl, ctr, cbl);
			add_triangle(r.xmax_ymin(), r.xmax_ymax(), r.xmin_ymax(), uvtr, uvbr, uvbl, ctr, cbr, cbl);
		}

		/// Renders all stored triangles using the given texture.
		void draw(const os::texture &tex) {
			os::renderer_base::get().draw_triangles(tex, _vs.data(), _uvs.data(), _cs.data(), _vs.size());
		}

		/// Enlarges the underlying data structures to reserve for a given number of triangles.
		void reserve(size_t numtris) {
			numtris *= 3;
			_vs.reserve(numtris);
			_uvs.reserve(numtris);
			_cs.reserve(numtris);
		}
	protected:
		std::vector<vec2d>
			_vs, ///< Vertices.
			_uvs; ///< UV's.
		std::vector<colord> _cs; ///< Vertex colors.
	};
}
