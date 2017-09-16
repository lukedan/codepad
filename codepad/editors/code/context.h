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
			class editor;

			typedef vec2<int> caret_position_diff;
			struct caret_position {
				constexpr caret_position() = default;
				constexpr caret_position(size_t l, size_t c) : line(l), column(c) {
				}

				size_t line = 0, column = 0;

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

				friend caret_position_diff operator-(caret_position lhs, caret_position rhs) {
					return caret_position_diff(
						unsigned_diff<int>(lhs.column, rhs.column), unsigned_diff<int>(lhs.line, rhs.line)
					);
				}
				caret_position &operator+=(caret_position_diff d) {
					line = add_unsigned_diff(line, d.y);
					column = add_unsigned_diff(column, d.x);
					return *this;
				}
				friend caret_position operator+(caret_position lhs, caret_position_diff rhs) {
					return lhs += rhs;
				}
				friend caret_position operator+(caret_position_diff lhs, caret_position rhs) {
					return rhs += lhs;
				}
				caret_position &operator-=(caret_position_diff d) {
					line = subtract_unsigned_diff(line, d.y);
					column = subtract_unsigned_diff(column, d.x);
					return *this;
				}
				friend caret_position operator-(caret_position lhs, caret_position_diff rhs) {
					return lhs -= rhs;
				}
			};
			typedef std::pair<caret_position, caret_position> caret_selection;
			struct caret_data {
				caret_data() = default;
				explicit caret_data(double align) : alignment(align) {
				}
				double alignment = 0.0;
			};
			struct caret_set {
				typedef std::map<caret_selection, caret_data> container;
				typedef std::pair<caret_selection, caret_data> entry;
				typedef container::iterator iterator;
				typedef container::const_iterator const_iterator;

				container carets;

				container::iterator add(entry p) {
					return add_caret(carets, p);
				}
				container::iterator add(entry p, bool &merged) {
					return add_caret(carets, p, merged);
				}

				void reset() {
					carets.clear();
					carets.insert(std::make_pair(std::make_pair(caret_position(), caret_position()), caret_data(0.0)));
				}

				inline static container::iterator add_caret(container &mp, entry c) {
					bool dummy;
					return add_caret(mp, c, dummy);
				}
				inline static container::iterator add_caret(container &mp, entry c, bool &merged) {
					merged = false;
					auto minmaxv = std::minmax({c.first.first, c.first.second});
					auto beg = mp.lower_bound(std::make_pair(minmaxv.first, minmaxv.first));
					if (beg != mp.begin()) {
						--beg;
					}
					while (beg != mp.end() && std::min(beg->first.first, beg->first.second) <= minmaxv.second) {
						if (can_merge_selection(
							c.first.first, c.first.second,
							beg->first.first, beg->first.second,
							c.first.first, c.first.second
						)) {
							beg = mp.erase(beg);
							merged = true;
						} else {
							++beg;
						}
					}
					return mp.insert(c).first;
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
					}
					if (sm == ss && sm >= p1mmv.first && sm <= p1mmv.second) {
						rm = mm;
						rs = ms;
						return true;
					}
					if (p1mmv.second <= p2mmv.first || p1mmv.first >= p2mmv.second) {
						return false;
					}
					caret_position gmin = std::min(p1mmv.first, p2mmv.first), gmax = std::max(p1mmv.second, p2mmv.second);
					assert_true_logical(!((mm == gmin && sm == gmax) || (mm == gmax && sm == gmin)), "caret layout shouldn't occur");
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

			struct modification {
				modification() = default;
				explicit modification(caret_selection sel) {
					selected_before = sel.first != sel.second;
					caret_front_before = sel.first < sel.second;
					if (caret_front_before) {
						position = sel.first;
						removed_range = sel.second - sel.first;
					} else {
						position = sel.second;
						removed_range = sel.first - sel.second;
					}
				}

				bool
					caret_front_before = false, selected_before = false,
					caret_front_after = false, selected_after = false;
				str_t removed_content, added_content;
				caret_position_diff removed_range, added_range;
				caret_position position; // position after previous modifications
			};
			typedef std::vector<modification> edit;

			struct modification_positions {
				modification_positions() = default;
				explicit modification_positions(const modification &mod) :
					removed_range(mod.removed_range), added_range(mod.added_range), position(mod.position) {
				}
				modification_positions(caret_position p, caret_position_diff rem, caret_position_diff add) :
					removed_range(rem), added_range(add), position(p) {
				}

				caret_position_diff removed_range, added_range;
				caret_position position;
			};
			struct caret_fixup_info { // TODO rework this part
			public:
				struct context {
					context() = default;
					explicit context(const caret_fixup_info &src) : next(src.mods.begin()) {
					}

					void append_custom_modification(modification_positions mpos) {
						if (mpos.position.line != curline || mpos.removed_range.y != 0) {
							diff.x = 0;
						}
						diff += mpos.added_range - mpos.removed_range;
						curline = mpos.position.line + static_cast<size_t>(mpos.added_range.y);
					}

					std::vector<modification_positions>::const_iterator next;
					size_t curline = 0;
					caret_position_diff diff;
				};

				caret_fixup_info() = default;
				explicit caret_fixup_info(const edit &e) {
					mods.reserve(e.size());
					for (auto i = e.begin(); i != e.end(); ++i) {
						mods.push_back(modification_positions(*i));
					}
				}

				void increment(context &ctx) const {
					if (ctx.next == mods.end()) {
						return;
					}
					ctx.append_custom_modification(*ctx.next);
					++ctx.next;
				}

				caret_position fixup_caret_min(caret_position cp, context &ctx) const {
					cp = fixup_caret_custom_context(cp, ctx);
					while (ctx.next != mods.end() && ctx.next->position < cp) {
						if (ctx.next->position + ctx.next->removed_range > cp) {
							// if a caret is in a removed area, move the caret to the front of it
							cp = ctx.next->position;
							break;
						}
						cp = fixup_caret_with_mod(cp, *ctx.next);
						increment(ctx);
					}
					return cp;
				}
				caret_position fixup_caret_max(caret_position cp, context &ctx) const {
					cp = fixup_caret_min(cp, ctx);
					if (ctx.next != mods.end() && ctx.next->position == cp) {
						cp = ctx.next->position + ctx.next->added_range;
					}
					return cp;
				}
				caret_position fixup_caret_custom_context(caret_position cp, const context &ctx) const {
					return _fix_raw(cp, ctx.diff, ctx.curline);
				}
				caret_position fixup_caret_with_mod(caret_position cp, const modification_positions &mod) const {
					return _fix_raw(
						cp, mod.added_range - mod.removed_range,
						mod.position.line + static_cast<size_t>(mod.added_range.y)
					);
				}

				std::vector<modification_positions> mods;
			protected:
				caret_position _fix_raw(caret_position cp, caret_position_diff diff, size_t line) const {
					cp.line = add_unsigned_diff(cp.line, diff.y);
					if (cp.line == line) {
						cp.column = add_unsigned_diff(cp.column, diff.x);
					}
					return cp;
				}
			};

			template <typename Cb> inline void convert_to_lines(const str_t &s, const Cb &callback) {
				char_t last = U'\0';
				size_t lasti = 0;
				for (size_t i = 0; i < s.length(); ++i) {
					char_t cur = s[i];
					if (last == U'\r') {
						if (cur == U'\n') {
							callback(s.substr(lasti, i - lasti - 1), line_ending::rn);
							lasti = i + 1;
						} else {
							callback(s.substr(lasti, i - lasti - 1), line_ending::r);
							lasti = i;
						}
					} else if (cur == U'\n') {
						callback(s.substr(lasti, i - lasti), line_ending::n);
						lasti = i + 1;
					}
					last = cur;
				}
				if (last == U'\r') {
					callback(s.substr(lasti, s.size() - lasti - 1), line_ending::r);
					callback(str_t(), line_ending::none);
				} else {
					callback(s.substr(lasti), line_ending::none);
				}
			}

			enum class text_theme_parameter {
				style,
				color
			};
			struct text_theme_specification {
				text_theme_specification() = default;
				text_theme_specification(font_style fs, colord c) : style(fs), color(c) {
				}
				font_style style = font_style::normal;
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
				text_theme_parameter_info<font_style> style;
				text_theme_parameter_info<colord> color;

				struct char_iterator {
					text_theme_specification current_theme;
					text_theme_parameter_info<font_style>::const_iterator style_iterator;
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

			struct modification_info {
				modification_info(editor *e, caret_fixup_info fi) : source(e), caret_fixup(std::move(fi)) {
				}
				editor *const source;
				const caret_fixup_info caret_fixup;
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
					u8str_t ss;
					{
						char *buffer = static_cast<char*>(std::malloc(sizeof(char) * buffer_size));
						std::ifstream fin(reinterpret_cast<const char*>(convert_to_utf8(fn).c_str()), std::ios::binary);
						while (fin) {
							fin.read(buffer, buffer_size - 1);
							buffer[fin.gcount()] = '\0';
							ss.append(reinterpret_cast<unsigned char*>(buffer));
						}
						std::free(buffer);
					}
					auto midt = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "read text file cost ",
						std::chrono::duration<double, std::milli>(midt - begt).count(), "ms"
					);
					auto s2 = utf8_to_utf32(ss);
					auto mid2t = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "decoding cost ",
						std::chrono::duration<double, std::milli>(mid2t - midt).count(), "ms"
					);
					convert_to_lines(s2, std::bind(
						&text_context::_append_line, this,
						std::placeholders::_1, std::placeholders::_2
					));
					auto endt = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "converting to lines cost ",
						std::chrono::duration<double, std::milli>(endt - mid2t).count(), "ms"
					);
				}
				void save_to_file(const str_t &fn) const {
					str_t result;
					for (auto it = begin(); it != end(); ++it) {
#ifdef CP_DETECT_LOGICAL_ERRORS
						auto nit = it;
						assert_true_logical(
							((++nit) == end()) == (it->ending_type == line_ending::none),
							"invalid line ending encountered"
						);
#endif
						result.append(it->content);
						switch (it->ending_type) {
						case line_ending::n:
							result.append(U"\n");
							break;
						case line_ending::r:
							result.append(U"\r");
							break;
						case line_ending::rn:
							result.append(U"\r\n");
							break;
						case line_ending::none: // nothing to do
							break;
						}
					}
					std::string utf8str(reinterpret_cast<const char*>(convert_to_utf8(result).c_str()));
					std::ofstream fout(reinterpret_cast<const char*>(convert_to_utf8(fn).c_str()), std::ios::binary);
					fout.write(utf8str.c_str(), utf8str.length());
				}

				void auto_set_default_line_ending() {
					size_t n[3] = {0, 0, 0};
					for (auto i = begin(); i != end(); ++i) {
						if (i->ending_type != line_ending::none) {
							++n[static_cast<int>(i->ending_type) - 1];
						}
#ifdef CP_DETECT_LOGICAL_ERRORS
						if (i->ending_type == line_ending::none) {
							auto ni = i;
							assert_true_logical((++ni) == end(), "invalid ending type encountered");
						}
#endif
					}
					_le = static_cast<line_ending>(n[0] > n[1] && n[0] > n[2] ? 1 : (n[1] > n[2] ? 2 : 3));
					logger::get().log_info(CP_HERE, "\\r ", n[0], ", \\n ", n[1], ", \\r\\n ", n[2], ", selected ", static_cast<int>(_le));
				}
				void set_default_line_ending(line_ending l) {
					assert_true_usage(l != line_ending::none, "invalid line ending");
					_le = l;
				}
				line_ending get_default_line_ending() const {
					return _le;
				}

				void set_tab_width(double v) {
					_tab_w = v;
					visual_changed.invoke();
				}
				double get_tab_width() const {
					return _tab_w;
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
						if (_block == _ctx->_blocks.end() || _line == _block->lines.begin()) {
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
				line_iterator insert(line_iterator it, line l) {
					it._line = it._block->lines.insert(it._line, std::move(l));
					return it;
				}
				line_iterator insert_after(line_iterator it, line l) {
					++it._line;
					it._line = it._block->lines.insert(it._line, std::move(l));
					return it;
				}
				line_iterator erase(line_iterator it) {
					_do_erase(it++);
					return it;
				}

				size_t num_lines() const {
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
					str_t res;
					auto lit = at(beg.line);
					res.append(lit->content.substr(beg.column)).append(to_str(lit->ending_type));
					++lit;
					for (size_t cl = beg.line + 1; cl < end.line; ++cl, ++lit) {
						res.append(lit->content).append(to_str(lit->ending_type));
					}
					return res.append(lit->content.substr(0, end.column));
				}

				// modification made by these functions will neither invoke modified nor be recorded
				caret_position insert_text(caret_position cp, const str_t &s) {
					return cp + insert_text(at(cp.line), cp.column, s).first;
				}
				std::pair<caret_position_diff, line_iterator> insert_text(line_iterator it, size_t col, const str_t &s) {
					bool first = true;
					str_t secondpart = it->content.substr(col);
					line_ending ole = it->ending_type;
					it->content = it->content.substr(0, col);
					caret_position_diff rdiff;
					convert_to_lines(s, [&](str_t cline, line_ending le) {
						if (first) {
							it->content += cline;
							it->ending_type = le;
							first = false;
						} else {
							it = insert_after(it, line(std::move(cline), le));
							++rdiff.y;
						}
					});
					rdiff.x = unsigned_diff<int>(it->content.length(), col);
					it->content += secondpart;
					it->ending_type = ole;
					return std::make_pair(rdiff, it);
				}
				void insert_char_nonnewline(caret_position cp, char_t c) {
					insert_char_nonnewline(at(cp.line), cp.column, c);
				}
				void insert_char_nonnewline(line_iterator it, size_t col, char_t c) {
					assert_true_usage(!is_newline(c), "to insert new lines, call insert_newline");
					it->content.insert(col, 1, c);
				}
				void insert_newline(caret_position cp, line_ending le = line_ending::none) {
					insert_newline(at(cp.line), cp.column, le);
				}
				void insert_newline(line_iterator it, size_t col, line_ending le = line_ending::none) {
					if (le == line_ending::none) {
						le = _le;
					}
					insert_after(it, line(it->content.substr(col), it->ending_type));
					it->content = it->content.substr(0, col);
					it->ending_type = le;
				}
				void insert_char(caret_position cp, char_t c) {
					if (is_newline(c)) {
						insert_newline(cp);
					} else {
						insert_char_nonnewline(cp, c);
					}
				}
				void insert_char(line_iterator it, size_t col, char_t c) {
					if (is_newline(c)) {
						insert_newline(it, col);
					} else {
						insert_char_nonnewline(it, col, c);
					}
				}

				size_t delete_text(caret_position p1, caret_position p2) {
					return delete_text(at(p1.line), p1.column, p2 - p1);
				}
				size_t delete_text(line_iterator beg, size_t bcol, caret_position_diff diff) {
					if (diff.y == 0) {
						beg->content = beg->content.substr(0, bcol) + beg->content.substr(bcol + diff.x);
						return static_cast<size_t>(diff.x);
					}
					size_t res = add_unsigned_diff(beg->content.length(), diff.x);
					line_iterator nit = beg;
					++nit;
					for (int i = 1; i < diff.y; ++i) {
						res += nit->content.length();
						nit = erase(nit);
					}
					beg->content = beg->content.substr(0, bcol) + nit->content.substr(add_unsigned_diff(bcol, diff.x));
					beg->ending_type = nit->ending_type;
					erase(nit);
					return res;
				}

				str_t cut_text(caret_position p1, caret_position p2) {
					return cut_text(at(p1.line), p1.column, p2 - p1);
				}
				str_t cut_text(line_iterator beg, size_t bcol, caret_position_diff diff) {
					str_t res;
					cut_text(beg, bcol, diff, [&res](str_t s, line_ending le) {
						res.append(std::move(s)).append(to_str(le));
					});
					return res;
				}
				template <typename C> void cut_text(line_iterator beg, size_t bcol, caret_position_diff diff, const C &cb) {
					if (diff.y == 0) {
						cb(beg->content.substr(bcol, static_cast<size_t>(diff.x)), line_ending::none);
						beg->content = beg->content.substr(0, bcol) + beg->content.substr(bcol + diff.x);
					} else {
						cb(beg->content.substr(bcol), beg->ending_type);
						line_iterator nit = beg;
						++nit;
						for (int i = 1; i < diff.y; ++i) {
							cb(std::move(nit->content), nit->ending_type);
							nit = erase(nit);
						}
						size_t ecol = add_unsigned_diff(bcol, diff.x);
						cb(nit->content.substr(0, ecol), line_ending::none);
						beg->content = beg->content.substr(0, bcol) + nit->content.substr(ecol);
						beg->ending_type = nit->ending_type;
						erase(nit);
					}
				}
				// till here

				const text_theme_data &get_text_theme() const {
					return _theme;
				}
				void set_text_theme(text_theme_data td) {
					_theme = std::move(td);
					visual_changed.invoke();
				}

				// these functions do NOT consider folding
				size_t hit_test_for_caret(const_line_iterator ln, size_t line_num, double pos, ui::font_family font) const {
					ui::line_character_iterator it(ln->content, font, get_tab_width());
					text_theme_data::char_iterator ci = get_text_theme().get_iter_at(caret_position(line_num, 0));
					size_t i = 0;
					for (
						it.begin(ci.current_theme.style);
						!it.ended();
						get_text_theme().incr_iter(ci, caret_position(line_num, ++i)), it.next(ci.current_theme.style)
						) {
						if (pos < (it.char_right() + it.prev_char_right()) * 0.5) {
							return i;
						}
					}
					return ln->content.length();
				}
				size_t hit_test_for_caret(size_t line, double pos, ui::font_family font) const {
					return hit_test_for_caret(at(line), line, pos, font);
				}
				double get_horizontal_caret_position(const_line_iterator ln, size_t pos, ui::font_family font) const {
					ui::line_character_iterator it(ln->content, font, get_tab_width());
					text_theme_data::char_iterator ci = get_text_theme().get_iter_at(caret_position(pos, 0));
					size_t i = 0;
					for (
						it.begin(ci.current_theme.style);
						i < pos;
						get_text_theme().incr_iter(ci, caret_position(pos, ++i)), it.next(ci.current_theme.style)
						) {
					}
					return it.prev_char_right();
				}
				double get_horizontal_caret_position(caret_position pos, ui::font_family font) const {
					return get_horizontal_caret_position(at(pos.line), pos.column, font);
				}

				bool can_undo() const {
					return _curedit > 0;
				}
				bool can_redo() const {
					return _curedit < _edithist.size();
				}
				caret_set undo(editor*);
				caret_set redo(editor*);
				void append_edit_data(edit e) {
					if (_curedit == _edithist.size()) {
						_edithist.push_back(std::move(e));
					} else {
						_edithist[_curedit] = std::move(e);
						_edithist.erase(_edithist.begin() + _curedit + 1, _edithist.end());
					}
					++_curedit;
				}
				const std::vector<edit> &get_edits() const {
					return _edithist;
				}
				size_t get_current_edit_index() const {
					return _curedit;
				}

				event<void> visual_changed;
				event<modification_info> modified; // ONLY invoked by text_context_modifier
			protected:
				std::list<block> _blocks;
				text_theme_data _theme;
				// undo & redo
				std::vector<edit> _edithist;
				size_t _curedit = 0;
				// settings
				double _tab_w = 4.0;
				line_ending _le;

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

			struct text_context_modifier {
			public:
				explicit text_context_modifier(text_context &ctx) : _ctx(&ctx) {
				}

				// fields used: added_content, removed_range, position, caret_back_after, selected_after
				// fields modified: removed_content, added_range
				// PITFALL: if you want to use (alter, save, etc.) the position of the caret or addition/deletion
				//          ranges before applying the modification, call fixup_caret_position() before using
				//          them, and then use the nofixup version of these apply_* functions
				void apply_modification_nofixup(modification mod) {
					assert_true_usage(mod.position >= _lastmax, "positions must be increasing");
					auto lit = _ctx->at(mod.position.line);
					if (mod.removed_range.x != 0 || mod.removed_range.y != 0) {
						mod.removed_content = _ctx->cut_text(lit, mod.position.column, mod.removed_range);
					}
					if (mod.added_content.length() > 0) {
						mod.added_range = _ctx->insert_text(lit, mod.position.column, mod.added_content).first;
					}
					_append_fixup_item(modification_positions(mod));
					_append_caret(get_caret_selection_after(mod));
					_edit.push_back(std::move(mod));
				}
				void apply_modification(modification mod) {
					fixup_caret_position(mod);
					apply_modification_nofixup(std::move(mod));
				}

				void redo_modification(const modification &mod) {
					assert_true_usage(mod.position >= _lastmax, "positions must be increasing");
					auto lit = _ctx->at(mod.position.line);
					if (mod.removed_content.length() > 0) {
						_ctx->delete_text(lit, mod.position.column, mod.removed_range);
					}
					if (mod.added_content.length() > 0) {
						_ctx->insert_text(lit, mod.position.column, mod.added_content);
					}
					_append_fixup_item(modification_positions(mod));
					_append_caret(get_caret_selection_after(mod));
				}
				void undo_modification(const modification &mod) {
					caret_position
						pos = fixup_caret_position(mod.position),
						addend = fixup_caret_position(mod.position + mod.added_range),
						delend = fixup_caret_position(mod.position + mod.removed_range);
					assert_true_usage(pos >= _lastmax, "positions must be increasing");
					auto lit = _ctx->at(pos.line);
					if (mod.added_content.length() > 0) {
						_ctx->delete_text(lit, pos.column, addend - pos);
					}
					if (mod.removed_content.length() > 0) {
						_ctx->insert_text(lit, pos.column, mod.removed_content);
					}
					_append_fixup_item(modification_positions(pos, addend - pos, delend - pos));
					_append_caret(get_caret_selection(pos, delend - pos, mod.selected_before, mod.caret_front_before));
				}

				inline static caret_selection get_caret_selection_after(const modification &mod) {
					return get_caret_selection(mod.position, mod.added_range, mod.selected_after, mod.caret_front_after);
				}
				inline static caret_selection get_caret_selection(
					caret_position pos, caret_position_diff diff, bool selected, bool caretfront
				) {
					caret_selection res = std::make_pair(pos, pos + diff);
					if (!caretfront) {
						std::swap(res.first, res.second);
					}
					if (!selected) {
						res.second = res.first;
					}
					return res;
				}

				caret_position fixup_caret_position(caret_position c) const {
					return _cfixup.fixup_caret_custom_context(c, _cfctx);
				}
				void fixup_caret_position(modification &m) const {
					caret_position rmend = fixup_caret_position(m.position + m.removed_range);
					m.position = fixup_caret_position(m.position);
					m.removed_range = rmend - m.position;
				}

				void on_text(caret_selection cs, str_t s) {
					modification mod(cs);
					mod.caret_front_after = false;
					mod.selected_after = false;
					mod.added_content = std::move(s);
					apply_modification(std::move(mod));
				}
				void on_text_overwrite(caret_selection cs, str_t s) {
					modification mod(cs);
					fixup_caret_position(mod);
					if (!mod.selected_before) {
						auto lit = _ctx->at(mod.position.line);
						caret_position remend = mod.position;
						for (auto i = s.begin(); i != s.end(); ++i) {
							if (!is_newline(*i) && remend.column < lit->content.length()) {
								++remend.column;
							}
						}
						mod.removed_range = remend - mod.position;
						mod.caret_front_before = true;
					}
					mod.added_content = std::move(s);
					apply_modification_nofixup(std::move(mod));
				}
				void on_backspace(caret_selection cs) {
					modification mod(cs);
					fixup_caret_position(mod);
					if (!mod.selected_before) {
						if (mod.position.column == 0) {
							if (mod.position.line > 0) {
								--mod.position.line;
								auto lit = _ctx->at(mod.position.line);
								mod.removed_range = caret_position_diff(-static_cast<int>(lit->content.length()), 1);
								mod.position.column = lit->content.length();
							}
						} else {
							--mod.position.column;
							mod.removed_range = caret_position_diff(1, 0);
						}
						mod.caret_front_before = false;
						mod.selected_before = false;
					}
					mod.caret_front_after = false;
					mod.selected_after = false;
					apply_modification_nofixup(std::move(mod));
				}
				void on_delete(caret_selection cs) {
					modification mod(cs);
					fixup_caret_position(mod);
					if (!mod.selected_before) {
						auto lit = _ctx->at(mod.position.line);
						if (mod.position.column == lit->content.length()) {
							auto nlit = lit;
							if (++nlit != _ctx->end()) {
								mod.removed_range = caret_position_diff(-static_cast<int>(lit->content.length()), 1);
							}
						} else {
							mod.removed_range = caret_position_diff(1, 0);
						}
						mod.caret_front_before = true;
						mod.selected_before = false;
					}
					mod.caret_front_after = false;
					mod.selected_after = false;
					apply_modification_nofixup(std::move(mod));
				}

				caret_set finish_edit(editor *source) {
					_ctx->append_edit_data(std::move(_edit));
					return finish_edit_nohistory(source);
				}
				caret_set finish_edit_nohistory(editor *source) {
					_ctx->modified.invoke_noret(source, std::move(_cfixup));
					_ctx = nullptr;
					return std::move(_newcarets);
				}
			protected:
				text_context *_ctx = nullptr;
				caret_position _lastmax;
				edit _edit;
				caret_fixup_info _cfixup;
				caret_fixup_info::context _cfctx;
				caret_set _newcarets;

				void _append_fixup_item(modification_positions mp) {
					_cfixup.mods.push_back(mp);
					_cfctx.append_custom_modification(mp);
				}
				void _append_caret(caret_selection sel) {
					_newcarets.add(caret_set::entry(sel, caret_data(0.0)));
				}
			};

			inline caret_set text_context::undo(editor *source) {
				assert_true_usage(can_undo(), "cannot undo");
				const edit &ce = _edithist[--_curedit];
				text_context_modifier mod(*this);
				for (auto i = ce.begin(); i != ce.end(); ++i) {
					mod.undo_modification(*i);
				}
				return mod.finish_edit_nohistory(source);
			}
			inline caret_set text_context::redo(editor *source) {
				assert_true_usage(can_redo(), "cannot redo");
				const edit &ce = _edithist[_curedit];
				++_curedit;
				text_context_modifier mod(*this);
				for (auto i = ce.begin(); i != ce.end(); ++i) {
					mod.redo_modification(*i);
				}
				return mod.finish_edit_nohistory(source);
			}
		}
	}
}
