#pragma once

#include <fstream>
#include <map>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>

#include "../ui/element.h"
#include "../ui/manager.h"
#include "../ui/textrenderer.h"
#include "../platform/window.h"

namespace codepad {
	namespace editor {
		// TODO syntax highlighting, line numbers, etc.
		enum class line_ending {
			none,
			r,
			n,
			rn
		};
		class file_context {
		public:
			file_context(const str_t &fn, size_t buffer_size = 32768) : path(fn) {
				std::list<line> ls;
				std::stringstream ss;
				{
					assert(buffer_size > 1);
					char *buffer = static_cast<char*>(std::malloc(sizeof(char) * buffer_size));
					std::ifstream fin(convert_to_utf8(path), std::ios::binary);
					while (fin) {
						fin.read(buffer, buffer_size - 1);
						buffer[fin.gcount()] = '\0';
						ss << buffer;
					}
					std::free(buffer);
				}
				str_t fullcontent = convert_from_utf8<char_t>(ss.str());
				std::basic_ostringstream<char_t> nss;
				char_t last = U'\0';
				blocks.push_back(block());
				for (auto i = fullcontent.begin(); i != fullcontent.end(); ++i) {
					if (last == U'\r') {
						bool winle = (*i == U'\n');
						_init_append_line(nss.str(), winle ? line_ending::rn : line_ending::r);
						nss = std::basic_ostringstream<char_t>();
						if (!winle) {
							nss << *i;
						}
					} else {
						nss << (*i == U'\r' ? U'\n' : *i);
						if (*i == U'\n') {
							_init_append_line(nss.str(), line_ending::n);
							nss = std::basic_ostringstream<char_t>();
						}
					}
					last = *i;
				}
				if (last == U'\r') {
					_init_append_line(nss.str(), line_ending::r);
					_init_append_line(str_t(), line_ending::none);
				} else {
					_init_append_line(nss.str(), line_ending::none);
				}
			}

			struct line {
				line() = default;
				line(const str_t &c, line_ending t) : content(c), ending_type(t) {
				}
				str_t content;
				line_ending ending_type;
			};
			struct block {
				constexpr static size_t advised_lines = 1000;
				std::list<line> lines;
			};

			struct line_iterator {
				friend class file_context;
			public:
				line_iterator() = default;
				line_iterator(const std::list<block>::iterator bk, const std::list<line>::iterator l) : _block(bk), _line(l) {
				}

				line &operator*() const {
					return *_line;
				}
				line *operator->() const {
					return &*_line;
				}

				line_iterator &operator++() {
					if ((++_line) == _block->lines.end()) {
						++_block;
						_line = _block->lines.begin();
					}
					return *this;
				}
				line_iterator operator++(int) {
					line_iterator tmp = *this;
					++*this;
					return tmp;
				}
				line_iterator &operator--() {
					if (_line == _block->lines.begin()) {
						--_block;
						_line = _block->lines.end();
					}
					--_line;
					return *this;
				}
				line_iterator operator--(int) {
					line_iterator tmp = *this;
					--*this;
					return tmp;
				}

				friend bool operator==(const line_iterator &l1, const line_iterator &l2) {
					return l1._block == l2._block && l1._line == l2._line;
				}
				friend bool operator!=(const line_iterator &l1, const line_iterator &l2) {
					return !(l1 == l2);
				}
			protected:
				std::list<block>::iterator _block;
				std::list<line>::iterator _line;
			};

			line_iterator at(size_t v) {
				for (auto i = blocks.begin(); i != blocks.end(); ++i) {
					if (i->lines.size() > v) {
						auto j = i->lines.begin();
						for (size_t k = 0; k < v; ++k, ++j) {
						}
						return line_iterator(i, j);
					} else {
						v -= i->lines.size();
					}
				}
				assert(false);
				return line_iterator(blocks.begin(), blocks.begin()->lines.begin());
			}
			line_iterator begin() {
				return line_iterator(blocks.begin(), blocks.begin()->lines.begin());
			}
			line_iterator before_end() {
				auto lastblk = blocks.end();
				--lastblk;
				auto lastln = lastblk->lines.end();
				return line_iterator(lastblk, --lastln);
			}

			// TODO splinter_block
			line_iterator insert(line_iterator it, const line &l) {
				it._line = it._block->lines.insert(it._line, l);
				return it;
			}
			line_iterator insert_after(line_iterator it, const line &l) {
				++it._line;
				it._line = it._block->lines.insert(it._line, l);
				return it;
			}
			line_iterator erase(line_iterator it) {
				if (it == before_end()) { // TODO hackish
					_do_erase(it--);
				} else {
					_do_erase(it++);
				}
				return it;
			}
			void erase_after(line_iterator it) {
				_do_erase(++it);
			}

			size_t num_lines() const { // TODO do some caching
				size_t res = 0;
				for (auto i = blocks.begin(); i != blocks.end(); ++i) {
					res += i->lines.size();
				}
				return res;
			}

			const str_t path;
			std::list<block> blocks;
			std::mutex modify_mutex;

		protected:
			void _init_append_line(const str_t &s, line_ending end) {
				if (blocks.back().lines.size() == block::advised_lines) {
					blocks.push_back(block());
				}
				blocks.back().lines.push_back(line(s, end));
			}
			void _do_erase(line_iterator it) {
				it._block->lines.erase(it._line);
				if (it._block->lines.size() == 0) {
					blocks.erase(it._block);
				}
			}
		};
		struct caret_position {
			caret_position() = default;
			caret_position(size_t l, size_t c) : line(l), column(c) {
			}

			size_t line, column;

			friend bool operator<(caret_position p1, caret_position p2) {
				return p1.line == p2.line ? p1.column < p2.column : p1.line < p2.line;
			}
			friend bool operator>(caret_position p1, caret_position p2) {
				return p2 < p1;
			}
			friend bool operator<=(caret_position p1, caret_position p2) {
				return !(p2 < p1);
			}
			friend bool operator>=(caret_position p1, caret_position p2) {
				return !(p1 < p2);
			}
			friend bool operator==(caret_position p1, caret_position p2) {
				return p1.line == p2.line && p1.column == p2.column;
			}
			friend bool operator!=(caret_position p1, caret_position p2) {
				return !(p1 == p2);
			}
		};
		class codebox : public ui::element {
		public:
			constexpr static double
				move_speed_scale = 15.0,
				dragdrop_distance = 5.0;

			file_context *context;

			void set_text_position(double v) {
				_pos = clamp(v, 0.0, static_cast<double>(context->num_lines() - 1) * _font.maximum_height());
				invalidate_visual();
			}
			double get_text_position() {
				return _pos;
			}

			void set_tab_width(double v) {
				_tab_w = v;
				invalidate_visual();
			}
			double get_tab_width() const {
				return _tab_w;
			}

			void auto_set_line_ending() {
				size_t n[3] = { 0, 0, 0 };
				for (auto i = context->begin(); ; ++i) {
					if (i->ending_type != line_ending::none) {
						++n[static_cast<int>(i->ending_type) - 1];
					}
#ifndef NDEBUG
					if (i->ending_type == line_ending::none) {
						assert(i == context->before_end());
					}
#endif
					if (i == context->before_end()) {
						break;
					}
				}
				_le = static_cast<line_ending>(n[0] > n[1] && n[0] > n[2] ? 1 : (n[1] > n[2] ? 2 : 3));
				CP_INFO("\\r %llu, \\n %llu, \\r\\n %llu, selected %d", n[0], n[1], n[2], static_cast<int>(_le));
			}
			void set_line_ending(line_ending l) {
				assert(l != line_ending::none);
				_le = l;
			}
			line_ending get_line_ending() const {
				return _le;
			}

			ui::cursor get_current_display_cursor() const override {
				if (!_cset.selecting && _is_in_selection(_mouse_cache)) {
					return ui::cursor::normal;
				}
				return ui::cursor::text_beam;
			}

			inline static void set_font(const font_family &ff) {
				_font = ff;
			}
			inline static const font_family &get_font() {
				return _font;
			}
			inline static void set_caret_pen(const ui::basic_pen *p) {
				_caretpen = p;
			}
			inline static const ui::basic_pen *get_caret_pen() {
				return _caretpen;
			}
			inline static void set_selection_brush(const ui::basic_brush *b) {
				_selbrush = b;
			}
			inline static const ui::basic_brush *get_selection_brush() {
				return _selbrush;
			}
		protected:
			struct _caret_range {
				_caret_range() = default;
				_caret_range(caret_position cp, double bl) : selection_end(cp), baseline(bl) {
				}

				caret_position selection_end;
				double baseline;
				// caches
				double pos_cache;
				std::vector<rectd> selection_cache;
			};
			struct _caret_set {
				std::multimap<caret_position, _caret_range> carets;
				std::pair<caret_position, _caret_range> current_selection;
				bool selecting = false;

				void begin_selection(caret_position cp, double baseline) {
					selecting = true;
					current_selection = std::make_pair(cp, _caret_range(cp, baseline));
				}
				std::multimap<caret_position, _caret_range>::iterator end_selection() {
					assert(selecting);
					selecting = false;
					return add_caret(carets, current_selection);
				}

				inline static std::multimap<caret_position, _caret_range>::iterator add_caret(
					std::multimap<caret_position, _caret_range> &mp,
					const std::pair<caret_position, _caret_range> &c
				) {
					bool dummy;
					return add_caret(mp, c, dummy);
				}
				inline static std::multimap<caret_position, _caret_range>::iterator add_caret(
					std::multimap<caret_position, _caret_range> &mp,
					const std::pair<caret_position, _caret_range> &c,
					bool &merged
				) {
					merged = false;
					auto minmaxv = std::minmax({ c.first, c.second.selection_end });
					auto beg = mp.lower_bound(minmaxv.first);
					if (beg != mp.begin()) {
						--beg;
					}
					std::pair<caret_position, _caret_range> res = c;
					while (beg != mp.end() && std::min(beg->first, beg->second.selection_end) <= minmaxv.second) {
						if (can_merge_selection(
							res.first, res.second.selection_end,
							beg->first, beg->second.selection_end,
							res.first, res.second.selection_end
						)) {
							beg = mp.erase(beg);
							merged = true;
						} else {
							++beg;
						}
					}
					return mp.insert(res);
				}
				inline static bool can_merge_selection(
					caret_position mm, caret_position ms,
					caret_position sm, caret_position ss,
					caret_position &rm, caret_position &rs
				) {
					auto p1mmv = std::minmax(mm, ms), p2mmv = std::minmax(sm, ss);
					if (mm == ms && mm >= p2mmv.first && mm <= p2mmv.second) {
						rm = sm;
						rs = ss;
						return true;
					} else if (sm == ss && sm >= p1mmv.first && sm <= p1mmv.second) {
						rm = mm;
						rs = ms;
						return true;
					}
					if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) {
						return false;
					}
					caret_position gmin = std::min(p1mmv.first, p2mmv.first), gmax = std::max(p1mmv.second, p2mmv.second);
					assert(!((mm == gmin && sm == gmax) || (mm == gmax && sm == gmin)));
					if (mm < ms) {
						rm = gmin;
						rs = gmax;
					} else {
						rm = gmax;
						rs = gmin;
					}
					return true;
				}
			};

			double _pos = 0.0, _tab_w = 4.0, _scrolldiff = 0.0;
			line_ending _le;
			_caret_set _cset;
			vec2d _predrag_pos;
			bool _insert = true, _predrag = false;
			// cache
			caret_position _mouse_cache;
#ifndef NDEBUG
			bool _modifying = false;
#endif

			static const ui::basic_pen *_caretpen;
			static const ui::basic_brush *_selbrush;
			static font_family _font;

			struct _char_pos_iterator {
			public:
				_char_pos_iterator(const str_t &s, const font_family &ff, double tabsize) :
					_cc(s.begin()), _end(s.end()), _ff(ff), _tabw(tabsize * _ff.maximum_width()) {
				}

				bool end() const {
					return _cc == _end;
				}
				bool next() {
					if (_cc == _end) {
						return false;
					}
					_pos += _ndiff;
					_curc = *_cc;
					_cet = &_ff.normal->get_char_entry(_curc);
					if (_curc == '\t') {
						_cw = _tabw * (std::floor(_pos / _tabw) + 1.0) - _pos;
					} else {
						_cw = _cet->advance;
					}
					if (++_cc != _end) {
						_ndiff = _cw + _ff.normal->get_kerning(_curc, *_cc).x;
					} else {
						_ndiff = _cw;
					}
					_ndiff = std::round(_ndiff);
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
				const font::entry &current_char_entry() const {
					return *_cet;
				}
			protected:
				str_t::const_iterator _cc, _end;
				const font_family &_ff;
				double _ndiff = 0.0, _cw = 0.0, _pos = 0.0, _tabw;
				char_t _curc = U'\0';
				const font::entry *_cet = nullptr;
			};

			size_t _hit_test_for_caret_x(const file_context::line &ln, double pos) const {
				_char_pos_iterator it(ln.content, _font, _tab_w);
				for (size_t i = 0; it.next(); ++i) {
					if (pos < (it.char_left() + it.next_char_left()) * 0.5) {
						return i;
					}
				}
				return ln.ending_type == line_ending::none ? ln.content.length() : ln.content.length() - 1;
			}
			void _render_line(const str_t &str, vec2d pos) const {
				int sx = static_cast<int>(std::ceil(pos.x)), sy = static_cast<int>(std::ceil(pos.y));
				for (_char_pos_iterator it(str, _font, _tab_w); it.next(); ) {
					if (is_graphical_char(it.current_char())) {
						platform::renderer_base::get().draw_character(
							it.current_char_entry().texture,
							vec2d(sx + it.char_left(), sy) + it.current_char_entry().placement.xmin_ymin(),
							colord(1.0, 1.0, 1.0, 1.0)
						);
					}
				}
			}
			double _get_caret_pos_x(file_context::line_iterator lit, size_t pos) const {
				_char_pos_iterator it(lit->content, _font, _tab_w);
				for (size_t i = 0; i < pos; ++i, it.next()) {
				}
				return it.next_char_left();
			}
			double _get_caret_pos_x(caret_position pos) const {
				return _get_caret_pos_x(context->at(pos.line), pos.column);
			}

			struct _modify_iterator {
			public:
				bool ended() {
					return _cur == _cb->_cset.carets.end();
				}
				void next() {
					bool merged;
					auto it = _caret_set::add_caret(_newcs, _rpos, merged);
					if (merged) {
						it->second.baseline = _cb->_get_caret_pos_x(it->first);
					}
					if (++_cur != _cb->_cset.carets.end()) {
						_rpos = *_cur;
						_fixup_pos(_rpos.first);
						_fixup_pos(_rpos.second.selection_end);
						_on_set_rpos();
					}
				}

				void insert_char(char_t c) {
					bool had_selection = _smin != _smax;
					if (had_selection) {
						_delete_selection();
					}
					if (c == U'\n') {
						++_dy;
						++_ly;
						_dx -= static_cast<int>(_smin.column);
						auto newline = _cb->context->insert_after(
							_lit, file_context::line(_lit->content.substr(_smin.column), _lit->ending_type)
						);
						_lit->content = _lit->content.substr(0, _smin.column) + U'\n';
						_lit->ending_type = _cb->_le;
						_lit = newline;
						++_smin.line;
						_smin.column = 0;
					} else {
						if (
							_cb->_insert || had_selection ||
							_smin.column == _lit->content.length() ||
							_lit->content[_smin.column] == U'\n'
							) {
							_lit->content.insert(_smin.column, 1, c);
							++_dx;
						} else {
							_lit->content[_smin.column] = c;
						}
						++_smin.column;
					}
					_smax = _smin;
					_rpos = std::make_pair(_smin, _caret_range(_smin, _cb->_get_caret_pos_x(_smin)));
				}
				void delete_char_before() {
					if (_smin != _smax) {
						_delete_selection();
					} else if (_smin != caret_position(0, 0)) {
						if (_smin.column == 0) {
							auto prevline = _lit;
							--_smin.line;
							--_dy;
							--_ly;
							--prevline;
							assert(prevline->content.back() == U'\n');
							assert(_dx == 0);
							_smin.column = prevline->content.length() - 1;
							prevline->content.pop_back();
							_dx += static_cast<int>(prevline->content.length());
							prevline->content += _lit->content;
							prevline->ending_type = _lit->ending_type;
							_lit = prevline;
							_cb->context->erase_after(_lit);
						} else {
							--_dx;
							--_smin.column;
							_lit->content.erase(_smin.column, 1);
						}
						_smax = _smin;
						_rpos = std::make_pair(_smin, _caret_range(_smin, _cb->_get_caret_pos_x(_smin)));
					}
				}
				void delete_char_after() {
					if (_smin != _smax) {
						_delete_selection();
					} else if (_smin.column <= _lit->content.length()) {
						if (_lit->content[_smin.column] == U'\n') {
							assert(_smin.line + 1 != _cb->context->num_lines());
							auto nextline = _lit;
							--_dy;
							++nextline;
							_lit->content.pop_back();
							_dx += static_cast<int>(_lit->content.length());
							_lit->content += nextline->content;
							_lit->ending_type = nextline->ending_type;
							_cb->context->erase(nextline);
						} else {
							--_dx;
							_lit->content.erase(_smin.column, 1);
						}
						_rpos.second.baseline = _cb->_get_caret_pos_x(_smin);
#ifndef NDEBUG
					} else {
						assert(_smin.line + 1 == _cb->context->num_lines());
#endif
					}
				}
				void move_to(caret_position p, double baseline) {
					_rpos = std::make_pair(p, _caret_range(p, baseline));
				}
				void move_to_with_selection(caret_position p, double baseline) {
					_rpos = std::make_pair(p, _caret_range(_cur->second.selection_end, baseline));
				}

				std::pair<caret_position, _caret_range> &current_position() {
					return _rpos;
				}
				const std::pair<caret_position, _caret_range> &current_position() const {
					return _rpos;
				}
				const std::pair<caret_position, _caret_range> &current_old_position() const {
					return *_cur;
				}

				void start(codebox &cb) {
					_cb = &cb;
					_lit = _cb->context->begin();
					_cur = _cb->_cset.carets.begin();
					_rpos = *_cur;
					if (_cb->_cset.selecting) {
						_cb->_end_selection();
					}
					_on_set_rpos();
				}
				void end() {
					std::swap(_cb->_cset.carets, _newcs);
					_cb->_rebuild_selection_cache();
#ifndef NDEBUG
					_cb->_modifying = false;
#endif
				}
			protected:
				codebox *_cb = nullptr;
				std::multimap<caret_position, _caret_range> _newcs;
				file_context::line_iterator _lit;
				int _dx = 0, _dy = 0;
				size_t _ly = 0;
				std::map<caret_position, _caret_range>::iterator _cur;
				std::pair<caret_position, _caret_range> _rpos;
				caret_position _smin, _smax;

				void _fixup_pos(caret_position &pos) const {
					pos.line = static_cast<size_t>(static_cast<int>(pos.line) + _dy);
					if (pos.line == _ly) {
						pos.column = static_cast<size_t>(static_cast<int>(pos.column) + _dx);
					}
				}

				void _on_set_rpos() {
					auto minmaxv = std::minmax(_rpos.first, _rpos.second.selection_end);
					_smin = minmaxv.first;
					_smax = minmaxv.second;
					if (_ly != _smin.line) {
						_dx = 0;
						_ly = _smin.line;
						_lit = _cb->context->at(_ly);
					}
				}

				void _delete_selection() {
					if (_smin.line == _smax.line) {
						_lit->content = _lit->content.substr(0, _smin.column) + _lit->content.substr(_smax.column);
					} else {
						_dy -= static_cast<int>(_smax.line - _smin.line);
						for (; _smin.line + 1 < _smax.line; --_smax.line) {
							_cb->context->erase_after(_lit);
						}
						auto nl = _lit;
						++nl;
						_lit->content = _lit->content.substr(0, _smin.column) + nl->content.substr(_smax.column);
						_lit->ending_type = nl->ending_type;
						_cb->context->erase_after(_lit);
					}
					_dx -= static_cast<int>(_smax.column) - static_cast<int>(_smin.column);
					_smax = _smin;
					_rpos = std::make_pair(_smin, _caret_range(_smin, _cb->_get_caret_pos_x(_smin)));
				}
			};

			_modify_iterator _start_modification() {
#ifndef NDEBUG
				assert(!_modifying);
				_modifying = true;
#endif
				_modify_iterator it;
				it.start(*this);
				return it;
			}

			struct _modify_iterator_rsrc {
			public:
				_modify_iterator_rsrc(codebox &cb) : _rsrc(cb._start_modification()) {
				}
				~_modify_iterator_rsrc() {
					_rsrc.end();
				}

				_modify_iterator *operator->() {
					return &_rsrc;
				}
				_modify_iterator &get() {
					return _rsrc;
				}
				_modify_iterator &operator*() {
					return get();
				}
			protected:
				_modify_iterator _rsrc;
			};

			void _on_content_modified() {
				set_text_position(_pos);
			}
			void _end_selection() {
				assert(_cset.selecting);
				auto it = _cset.end_selection();
				it->second.selection_cache.clear();
				_make_selection_cache_of(it, _font.maximum_height());
			}

			caret_position _hit_test_for_caret(vec2d pos) {
				caret_position cp;
				cp.line = static_cast<size_t>(std::max(0.0, (pos.y + _pos) / _font.maximum_height()));
				if (cp.line >= context->num_lines()) {
					cp.line = context->num_lines() - 1;
				}
				cp.column = _hit_test_for_caret_x(*context->at(cp.line), pos.x);
				return cp;
			}
			bool _is_in_selection(caret_position cp) const {
				auto cur = _cset.carets.lower_bound(cp);
				if (cur != _cset.carets.begin()) {
					--cur;
				}
				while (cur != _cset.carets.end() && std::min(cur->first, cur->second.selection_end) <= cp) {
					if (cur->first != cur->second.selection_end) {
						auto minmaxv = std::minmax(cur->first, cur->second.selection_end);
						if (cp >= minmaxv.first && cp <= minmaxv.second) {
							return true;
						}
					}
					++cur;
				}
				return false;
			}

			std::pair<caret_position, double> _get_left_position(caret_position cp) const {
				file_context::line_iterator lit = context->at(cp.line);
				if (cp.column == 0) {
					if (cp.line > 0) {
						--lit;
						--cp.line;
						assert(lit->content.back() == U'\n');
						cp.column = lit->content.length() - 1;
					}
				} else {
					--cp.column;
				}
				return std::make_pair(cp, _get_caret_pos_x(lit, cp.column));
			}
			std::pair<caret_position, double> _get_right_position(caret_position cp) const {
				file_context::line_iterator lit = context->at(cp.line);
				if (cp.line + 1 == context->num_lines()) {
					assert(lit->content.length() == 0 || lit->content.back() != U'\n');
					cp.column = std::min(cp.column + 1, lit->content.length());
				} else {
					assert(lit->content.back() == U'\n');
					if (cp.column + 1 == lit->content.length()) {
						++lit;
						++cp.line;
						cp.column = 0;
					} else {
						++cp.column;
					}
				}
				return std::make_pair(cp, _get_caret_pos_x(lit, cp.column));
			}
			caret_position _get_up_position(caret_position cp, double bl) const {
				if (cp.line == 0) {
					return cp;
				}
				cp.column = _hit_test_for_caret_x(*context->at(--cp.line), bl);
				return cp;
			}
			caret_position _get_down_position(caret_position cp, double bl) const {
				if (cp.line + 1 == context->num_lines()) {
					return cp;
				}
				cp.column = _hit_test_for_caret_x(*context->at(++cp.line), bl);
				return cp;
			}

			template <typename GetPos, typename GetTg> void _on_key_down_lr(GetPos gp, GetTg gt) {
				if (platform::input::is_key_down(platform::input::key::shift)) {
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						auto newp = (this->*gp)(it->current_position().first);
						it->move_to_with_selection(newp.first, newp.second);
					}
				} else {
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						const auto &curp = it->current_position();
						if (curp.first == curp.second.selection_end) {
							auto newp = (this->*gp)(curp.first);
							it->move_to(newp.first, newp.second);
						} else {
							caret_position newp = gt(curp.first, curp.second.selection_end);
							it->move_to(newp, _get_caret_pos_x(newp));
						}
					}
				}
			}
			template <typename Comp, typename GetPos> void _on_key_down_ud(GetPos gp) {
				if (platform::input::is_key_down(platform::input::key::shift)) {
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						const auto &curp = it->current_position();
						it->move_to_with_selection(
							(this->*gp)(curp.first, curp.second.baseline), curp.second.baseline
						);
					}
				} else {
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						const auto &curp = it->current_position();
						double bl = curp.second.baseline;
						caret_position newop = curp.first;
						if (Comp()(curp.first, curp.second.selection_end)) {
							newop = curp.second.selection_end;
							bl = _get_caret_pos_x(newop);
						}
						it->move_to((this->*gp)(newop, bl), bl);
					}
				}
			}

			void _on_selecting_mouse_move(vec2d pos) {
				vec2d relpos = pos - get_client_region().xmin_ymin(), clampedpos = relpos;
				if (relpos.y < 0.0) {
					clampedpos.y = 0.0;
					_scrolldiff = relpos.y;
					ui::manager::get().schedule_update(this);
				} else {
					double h = get_client_region().height();
					if (relpos.y > h) {
						clampedpos.y = h;
						_scrolldiff = relpos.y - get_client_region().height();
						ui::manager::get().schedule_update(this);
					}
				}
				_mouse_cache = _hit_test_for_caret(clampedpos);
				if (_cset.selecting) {
					if (_mouse_cache != _cset.current_selection.first) {
						_cset.current_selection.first = _mouse_cache;
						_cset.current_selection.second.baseline = _get_caret_pos_x(_cset.current_selection.first);
						_cset.current_selection.second.selection_cache.clear();
						_make_selection_cache_of(&_cset.current_selection, _font.maximum_height());
						invalidate_visual();
					}
				}
			}

			void _on_mouse_move(ui::mouse_move_info &info) override {
				_on_selecting_mouse_move(info.new_pos);
				if (_predrag) {
					if ((info.new_pos - _predrag_pos).length_sqr() > dragdrop_distance * dragdrop_distance) {
						_predrag = false;
						CP_INFO("starting drag & drop of text");
						// TODO start drag drop
					}
				}
				element::_on_mouse_move(info);
			}
			void _on_mouse_scroll(ui::mouse_scroll_info &info) override {
				double oldp = _pos;
				set_text_position(_pos - info.delta * 3.0 * _font.maximum_height()); // TODO magic values
				if (std::abs(oldp - _pos) > 1.0) {
					info.mark_handled();
				}
			}
			void _on_mouse_down(ui::mouse_button_info &info) override {
				element::_on_mouse_down(info);
				if (info.button == platform::input::mouse_button::left) {
					_mouse_cache = _hit_test_for_caret(info.position - get_client_region().xmin_ymin());
					if (!_is_in_selection(_mouse_cache)) {
						if (!platform::input::is_key_down(platform::input::key::control)) {
							_cset.carets.clear();
						}
						_cset.begin_selection(_mouse_cache, _get_caret_pos_x(_mouse_cache));
						_make_selection_cache_of(&_cset.current_selection, _font.maximum_height());
						invalidate_visual();
					} else {
						_predrag_pos = info.position;
						_predrag = true;
					}
					get_window()->set_mouse_capture(*this);
				} else if (info.button == platform::input::mouse_button::middle) {
					// TODO block selection
				}
			}
			void _on_mouse_up(ui::mouse_button_info &info) {
				if (info.button == platform::input::mouse_button::left) {
					if (_cset.selecting) {
						_end_selection();
						invalidate_visual();
					} else if (_predrag) {
						_predrag = false;
						caret_position hitp = _hit_test_for_caret(_predrag_pos - get_client_region().xmin_ymin());
						_cset.carets.clear();
						_cset.carets.insert(std::make_pair(hitp, _caret_range(hitp, _get_caret_pos_x(hitp))));
						_rebuild_selection_cache();
					}
					get_window()->release_mouse_capture();
				}
			}
			void _on_key_down(ui::key_info &info) override {
				switch (info.key) {
					// deleting stuff
				case platform::input::key::backspace:
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						it->delete_char_before();
					}
					_on_content_modified();
					break;
				case platform::input::key::del:
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						it->delete_char_after();
					}
					_on_content_modified();
					break;

					// movement control
				case platform::input::key::left:
					_on_key_down_lr(&codebox::_get_left_position, static_cast<
						const caret_position&(*)(const caret_position&, const caret_position&)
					>(std::min));
					break;
				case platform::input::key::right:
					_on_key_down_lr(&codebox::_get_right_position, static_cast<
						const caret_position&(*)(const caret_position&, const caret_position&)
					>(std::max));
					break;
				case platform::input::key::up:
					_on_key_down_ud<std::greater<caret_position>>(&codebox::_get_up_position);
					break;
				case platform::input::key::down:
					_on_key_down_ud<std::less<caret_position>>(&codebox::_get_down_position);
					break;
				case platform::input::key::home:
					{
						auto fptr =
							platform::input::is_key_down(platform::input::key::shift) ?
							&_modify_iterator::move_to_with_selection :
							&_modify_iterator::move_to;
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							caret_position cp = it->current_position().first;
							auto lit = context->at(cp.line);
							size_t i = 0;
							while (i < lit->content.length() && (lit->content[i] == U' ' || lit->content[i] == U'\t')) {
								++i;
							}
							if (cp.column == i) {
								cp.column = 0;
								((*it).*fptr)(cp, 0.0);
							} else {
								cp.column = i;
								((*it).*fptr)(cp, _get_caret_pos_x(lit, cp.column));
							}
						}
					}
					break;
				case platform::input::key::end:
					{
						auto fptr =
							platform::input::is_key_down(platform::input::key::shift) ?
							&_modify_iterator::move_to_with_selection :
							&_modify_iterator::move_to;
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							caret_position cp = it->current_position().first;
							auto lit = context->at(cp.line);
							cp.column = lit->content.length();
							if (lit->ending_type != line_ending::none) {
								assert(lit->content.back() == U'\n');
								--cp.column;
							}
							((*it).*fptr)(cp, std::numeric_limits<double>::infinity());
						}
					}
					break;
				case platform::input::key::page_up:
					// TODO page_up
					break;
				case platform::input::key::page_down:
					// TODO page_down
					break;
				case platform::input::key::escape:
					{
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							auto curpos = it->current_position();
							it->move_to(curpos.first, curpos.second.baseline);
						}
					}
					break;

					// edit mode toggle
				case platform::input::key::insert:
					_insert = !_insert;
					invalidate_visual();
					break;
				}
				element::_on_key_down(info);
			}
			void _on_keyboard_text(ui::text_info &info) override {
				_modify_iterator it = _start_modification();
				for (; !it.ended(); it.next()) {
					it.insert_char(info.character);
				}
				it.end();
				_on_content_modified();
			}
			void _on_update() override {
				if (_cset.selecting) {
					set_text_position(_pos + move_speed_scale * _scrolldiff * ui::manager::get().delta_time().count());
					_on_selecting_mouse_move(get_window()->screen_to_client(platform::input::get_mouse_position()).convert<double>());
				}
			}

			template <typename T> void _make_selection_cache_of(T it, double h) const {
				assert(it->second.selection_cache.size() == 0);
				it->second.pos_cache = _get_caret_pos_x(it->first);
				if (it->first != it->second.selection_end) {
					double begp = it->second.pos_cache, endp = _get_caret_pos_x(it->second.selection_end);
					caret_position begcp = it->first, endcp = it->second.selection_end;
					if (begcp > endcp) {
						std::swap(begp, endp);
						std::swap(begcp, endcp);
					}
					double y = begcp.line * h;
					if (begcp.line == endcp.line) {
						it->second.selection_cache.push_back(rectd(begp, endp, y, y + h));
					} else {
						auto lit = context->at(begcp.line);
						double end = _get_caret_pos_x(caret_position(begcp.line, lit->content.length()));
						it->second.selection_cache.push_back(rectd(begp, end, y, y + h));
						size_t i = begcp.line + 1;
						for (++lit, y += h; i < endcp.line; ++i, ++lit, y += h) {
							end = _get_caret_pos_x(caret_position(i, lit->content.length()));
							it->second.selection_cache.push_back(rectd(0.0, end, y, y + h));
						}
						it->second.selection_cache.push_back(rectd(0.0, endp, y, y + h));
					}
				}
			}
			void _rebuild_selection_cache() {
				double h = _font.maximum_height();
				for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
					_make_selection_cache_of(i, h);
				}
				invalidate_visual();
			}
			void _draw_caret_and_selection(const std::pair<caret_position, _caret_range> &sp, std::vector<vec2d> &ls, double h) const {
				double
					x = get_client_region().xmin + sp.second.pos_cache,
					y = get_client_region().ymin - _pos + static_cast<double>(sp.first.line) * h;
				if (_insert) {
					ls.push_back(vec2d(x, y));
					ls.push_back(vec2d(x, y + h));
				} else {
					auto lit = context->at(sp.first.line);
					double cw =
						sp.first.column < lit->content.length() ?
						_get_caret_pos_x(lit, sp.first.column + 1) :
						sp.second.pos_cache + _font.normal->get_char_entry(U'\n').advance;
					double yv = y + h;
					ls.push_back(vec2d(x, yv));
					ls.push_back(vec2d(get_client_region().xmin + cw, yv));
				}
				if (sp.first != sp.second.selection_end) {
					vec2d pdiff(get_client_region().xmin, get_client_region().ymin - _pos);
					for (auto i = sp.second.selection_cache.begin(); i != sp.second.selection_cache.end(); ++i) {
						_selbrush->fill_rect(i->translated(pdiff));
					}
				}
			}
			void _render() const override {
#ifndef NDEBUG
				assert(!_modifying);
#endif
				if (get_client_region().height() < 0.0) {
					return;
				}
				double lh = _font.maximum_height();
				size_t
					line_beg = static_cast<size_t>(_pos / lh),
					line_end = static_cast<size_t>((_pos + get_client_region().height()) / lh) + 1;
				// text rendering stuff
				auto lit = context->at(line_beg);
				double cury = get_client_region().ymin - _pos + static_cast<double>(line_beg) * lh;
				for (size_t i = line_beg; i <= line_end; ++i, ++lit, cury += lh) {
					_render_line(lit->content, vec2d(get_client_region().xmin, cury));
					if (lit == context->before_end()) {
						break;
					}
				}
				// render carets
				std::vector<vec2d> cls;
				std::multimap<caret_position, _caret_range> val;
				const std::multimap<caret_position, _caret_range> *csetptr = &_cset.carets;
				if (_cset.selecting) {
					val = _cset.carets;
					auto it = _caret_set::add_caret(val, _cset.current_selection);
					it->second.selection_cache.clear();
					_make_selection_cache_of(it, lh);
					csetptr = &val;
				}
				auto
					caret_beg = csetptr->lower_bound(caret_position(line_beg, 0)),
					caret_end = csetptr->lower_bound(caret_position(line_end + 1, 0));
				if (caret_beg != csetptr->begin()) {
					auto prev = caret_beg;
					--prev;
					if (prev->second.selection_end.line >= line_beg) {
						caret_beg = prev;
					}
				}
				if (caret_end != csetptr->end() && caret_end->second.selection_end.line <= line_end) {
					++caret_end;
				}
				for (auto i = caret_beg; i != caret_end; ++i) {
					_draw_caret_and_selection(*i, cls, lh);
				}
				_caretpen->draw_lines(cls);
			}
		};
	}
}
