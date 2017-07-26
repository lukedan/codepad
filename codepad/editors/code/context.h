#pragma once

#include "codebox.h"

namespace codepad {
	namespace editor {
		namespace code {
			enum class line_ending : char_t {
				none,
				r,
				n,
				rn
			};
		}
	}
	template <> inline str_t to_str<editor::code::line_ending>(editor::code::line_ending le) {
		switch (le) {
		case editor::code::line_ending::r:
			return U"\r";
		case editor::code::line_ending::n:
			return U"\n";
		case editor::code::line_ending::rn:
			return U"\r\n";
		default:
			return U"";
		}
	}
	namespace editor {
		namespace code {
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
			template <typename Cb> inline void convert_to_lines(const str_t &s, const Cb &callback) {
				char_t last = U'\0';
				std::basic_ostringstream<char_t> nss;
				for (auto i = s.begin(); i != s.end(); ++i) {
					if (last == U'\r') {
						callback(nss.str(), *i == U'\n' ? line_ending::rn : line_ending::r);
						nss.str(U"");
					} else if (*i == U'\n') {
						callback(nss.str(), line_ending::n);
						nss.str(U"");
					}
					if (*i != U'\n' && *i != U'\r') {
						nss << *i;
					}
					last = *i;
				}
				if (last == U'\r') {
					callback(nss.str(), line_ending::r);
					callback(str_t(), line_ending::none);
				} else {
					callback(nss.str(), line_ending::none);
				}
			}

			// TODO syntax highlighting, drag & drop, etc.
			enum class text_theme_parameter {
				style,
				color
			};
			struct text_theme_specification {
				text_theme_specification() = default;
				text_theme_specification(ui::font_style fs, colord c) : style(fs), color(c) {
				}
				ui::font_style style = ui::font_style::normal;
				colord color;
			};
			template <typename T> class text_theme_parameter_info {
			public:
				typedef typename std::map<caret_position, T>::iterator iterator;
				typedef typename std::map<caret_position, T>::const_iterator const_iterator;

				text_theme_parameter_info() : text_theme_parameter_info(T()) {
				}
				explicit text_theme_parameter_info(T def) {
					_changes.emplace(std::make_pair(caret_position(0, 0), std::move(def)));
				}

				void clear(T def) {
					_changes.clear();
					_changes[caret_position(0, 0)] = def;
				}
				void set_range(caret_position s, caret_position pe, T c) {
					assert_true_usage(s < pe, "invalid range");
					auto beg = _at(s), end = _at(pe);
					T begv = beg->second, endv = end->second;
					_changes.erase(++beg, ++end);
					if (begv != c) {
						_changes[s] = c;
					}
					if (endv != c) {
						_changes[pe] = endv;
					}
				}
				T get_at(caret_position cp) const {
					return _at(cp)->second;
				}

				iterator begin() {
					return _changes.begin();
				}
				iterator end() {
					return _changes.end();
				}
				iterator get_iter_at(caret_position cp) {
					return _at(cp);
				}
				const_iterator begin() const {
					return _changes.begin();
				}
				const_iterator end() const {
					return _changes.end();
				}
				const_iterator get_iter_at(caret_position cp) const {
					return _at(cp);
				}
			protected:
				std::map<caret_position, T> _changes;

				iterator _at(caret_position cp) {
					return --_changes.upper_bound(cp);
				}
				const_iterator _at(caret_position cp) const {
					return --_changes.upper_bound(cp);
				}
			};
			struct text_theme_data {
				text_theme_parameter_info<ui::font_style> style;
				text_theme_parameter_info<colord> color;

				struct char_iterator {
					text_theme_specification current_theme;
					text_theme_parameter_info<ui::font_style>::const_iterator style_iterator;
					text_theme_parameter_info<colord>::const_iterator color_iterator;
				};

				void set_range(caret_position s, caret_position pe, text_theme_specification tc) {
					color.set_range(s, pe, tc.color);
					style.set_range(s, pe, tc.style);
				}
				text_theme_specification get_at(caret_position p) const {
					return text_theme_specification(style.get_at(p), color.get_at(p));
				}
				void clear(text_theme_specification def) {
					style.clear(def.style);
					color.clear(def.color);
				}

				char_iterator get_iter_at(caret_position p) const {
					char_iterator rv;
					rv.style_iterator = style.get_iter_at(p);
					rv.color_iterator = color.get_iter_at(p);
					assert_true_logical(rv.style_iterator != style.end(), "empty theme parameter info encountered");
					assert_true_logical(rv.color_iterator != color.end(), "empty theme parameter info encountered");
					rv.current_theme = text_theme_specification(rv.style_iterator->second, rv.color_iterator->second);
					++rv.style_iterator;
					++rv.color_iterator;
					return rv;
				}
			protected:
				template <typename T> inline static void _incr_iter_elem(
					caret_position cp,
					typename text_theme_parameter_info<T>::const_iterator &it,
					const text_theme_parameter_info<T> &fullset,
					T &fval
				) {
					if (it != fullset.end() && it->first <= cp) {
						fval = it->second;
						++it;
					}
				}
			public:
				void incr_iter(char_iterator &cv, caret_position cp) const {
					_incr_iter_elem(cp, cv.color_iterator, color, cv.current_theme.color);
					_incr_iter_elem(cp, cv.style_iterator, style, cv.current_theme.style);
				}
			};
			class text_context {
			public:
				void clear() {
					_blocks.clear();
				}

				void load_from_file(const str_t &fn, size_t buffer_size = 32768) {
					assert_true_usage(buffer_size > 1, "buffer size must be greater than one");
					auto begt = std::chrono::high_resolution_clock::now();
					std::list<line> ls;
					std::stringstream ss;
					{
						char *buffer = static_cast<char*>(std::malloc(sizeof(char) * buffer_size));
						std::ifstream fin(convert_to_utf8(fn), std::ios::binary);
						while (fin) {
							fin.read(buffer, buffer_size - 1);
							buffer[fin.gcount()] = '\0';
							ss << buffer;
						}
						std::free(buffer);
					}
					auto midt = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "read text file cost ", std::chrono::duration<double, std::milli>(midt - begt).count(), "ms"
					);
					auto s2 = convert_from_utf8<char_t>(ss.str());
					auto mid2t = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "decoding cost ", std::chrono::duration<double, std::milli>(mid2t - midt).count(), "ms"
					);
					convert_to_lines(convert_from_utf8<char_t>(ss.str()), std::bind(
						&text_context::_append_line, this,
						std::placeholders::_1, std::placeholders::_2
					));
					auto endt = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "converting to lines cost ", std::chrono::duration<double, std::milli>(endt - mid2t).count(), "ms"
					);
				}
				void save_to_file(const str_t &fn) const {
					std::basic_ostringstream<char_t> ss;
					for (auto it = begin(); it != end(); ++it) {
#ifdef CP_DETECT_LOGICAL_ERRORS
						auto nit = it;
						assert_true_logical(((++nit) == end()) == (it->ending_type == line_ending::none), "invalid line ending encountered");
#endif
						ss << it->content;
						switch (it->ending_type) {
						case line_ending::n:
							ss << U'\n';
							break;
						case line_ending::r:
							ss << U'\r';
							break;
						case line_ending::rn:
							ss << U"\r\n";
							break;
						case line_ending::none: // nothing to do
							break;
						}
					}
					std::string utf8str = convert_to_utf8(ss.str());
					std::ofstream fout(convert_to_utf8(fn), std::ios::binary);
					fout.write(utf8str.c_str(), utf8str.length());
				}

				struct line {
					line() = default;
					line(str_t c, line_ending t) : content(std::move(c)), ending_type(t) {
					}
					str_t content;
					line_ending ending_type;
				};
				struct block {
					constexpr static size_t advised_lines = 1000;
					std::list<line> lines;
				};

				template <typename CtxC, typename BlkIt, typename LnIt> struct line_iterator_base {
					friend class text_context;
				public:
					line_iterator_base() = default;
					template <typename NCtxC, typename NBlkIt, typename NLnIt> line_iterator_base(
						const line_iterator_base<NCtxC, NBlkIt, NLnIt> &oit
					) : _ctx(oit._ctx), _block(oit._block), _line(oit._line) {
					}

					typename LnIt::reference operator*() const {
						return *_line;
					}
					typename LnIt::pointer operator->() const {
						return &*_line;
					}

					line_iterator_base &operator++() {
						if ((++_line) == _block->lines.end()) {
							if ((++_block) == _ctx->_blocks.end()) {
								_line = LnIt();
							} else {
								_line = _block->lines.begin();
							}
						}
						return *this;
					}
					line_iterator_base operator++(int) {
						line_iterator_base tmp = *this;
						++*this;
						return tmp;
					}
					line_iterator_base &operator--() {
						if (_line == LnIt() || _line == _block->lines.begin()) {
							--_block;
							_line = _block->lines.end();
						}
						--_line;
						return *this;
					}
					line_iterator_base operator--(int) {
						line_iterator_base tmp = *this;
						--*this;
						return tmp;
					}

					friend bool operator==(const line_iterator_base &l1, const line_iterator_base &l2) {
						return l1._block == l2._block && l1._line == l2._line;
					}
					friend bool operator!=(const line_iterator_base &l1, const line_iterator_base &l2) {
						return !(l1 == l2);
					}
				protected:
					line_iterator_base(CtxC &ctx, BlkIt bk, LnIt l) : _ctx(&ctx), _block(bk), _line(l) {
					}

					CtxC *_ctx = nullptr;
					BlkIt _block;
					LnIt _line;
				};
				typedef line_iterator_base<text_context, std::list<block>::iterator, std::list<line>::iterator> line_iterator;
				typedef line_iterator_base<const text_context, std::list<block>::const_iterator, std::list<line>::const_iterator> const_line_iterator;

				line_iterator at(size_t v) {
					return _at_impl<line_iterator>(*this, v);
				}
				const_line_iterator at(size_t v) const {
					return _at_impl<const_line_iterator>(*this, v);
				}
				line_iterator begin() {
					return line_iterator(*this, _blocks.begin(), _blocks.begin()->lines.begin());
				}
				const_line_iterator begin() const {
					return const_line_iterator(*this, _blocks.begin(), _blocks.begin()->lines.begin());
				}
				line_iterator end() {
					return line_iterator(*this, _blocks.end(), std::list<line>::iterator());
				}
				const_line_iterator end() const {
					return const_line_iterator(*this, _blocks.end(), std::list<line>::const_iterator());
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
					_do_erase(it++);
					return it;
				}

				size_t num_lines() const { // TODO do some caching
					size_t res = 0;
					for (auto i = _blocks.begin(); i != _blocks.end(); ++i) {
						res += i->lines.size();
					}
					return res;
				}

				str_t substr(caret_position beg, caret_position end) const {
					assert_true_usage(end >= beg, "invalid range");
					if (beg.line == end.line) {
						auto lit = at(beg.line);
						return lit->content.substr(beg.column, end.column - beg.column);
					}
					std::basic_ostringstream<char_t> ss;
					auto lit = at(beg.line);
					ss << lit->content.substr(beg.column) << to_str(lit->ending_type);
					++lit;
					for (size_t cl = beg.line + 1; cl < end.line; ++cl, ++lit) {
						ss << lit->content << to_str(lit->ending_type);
					}
					ss << lit->content.substr(0, end.column);
					return ss.str();
				}

				const text_theme_data &get_text_theme() const {
					return _theme;
				}
				void set_text_theme(text_theme_data td) {
					_theme = std::move(td);
					theme_changed.invoke();
				}

				event<void> modified, theme_changed;
			protected:
				std::list<block> _blocks;
				text_theme_data _theme;

				template <typename It, typename S> inline static It _at_impl(S &s, size_t v) {
					for (auto i = s._blocks.begin(); i != s._blocks.end(); ++i) {
						if (i->lines.size() > v) {
							auto j = i->lines.begin();
							for (size_t k = 0; k < v; ++k, ++j) {
							}
							return It(s, i, j);
						} else {
							v -= i->lines.size();
						}
					}
					return s.end();
				}

				void _append_line(str_t s, line_ending end) {
					if (_blocks.size() == 0 || _blocks.back().lines.size() == block::advised_lines) {
						_blocks.push_back(block());
					}
					_blocks.back().lines.push_back(line(std::move(s), end));
				}
				void _do_erase(line_iterator it) {
					it._block->lines.erase(it._line);
					if (it._block->lines.size() == 0) {
						_blocks.erase(it._block);
					}
				}
			};
		}
	}
}