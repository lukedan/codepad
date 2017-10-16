#pragma once

#include "codebox.h"
#include "buffer.h"

namespace codepad {
	namespace editor {
		namespace code {
			class editor;

			using caret_position = size_t;
			using caret_position_diff = std::conditional_t<sizeof(size_t) == 4, int, long long>;
			using caret_selection = std::pair<caret_position, caret_position>;

			struct caret_data {
				caret_data() = default;
				explicit caret_data(double align) : alignment(align) {
				}
				double alignment = 0.0;
			};
			struct caret_set {
				using container = std::map<caret_selection, caret_data>;
				using entry = std::pair<caret_selection, caret_data>;
				using iterator = container::iterator;
				using const_iterator = container::const_iterator;

				container carets;

				container::iterator add(entry p) {
					return add_caret(carets, p);
				}
				container::iterator add(entry p, bool &merged) {
					return add_caret(carets, p, merged);
				}

				void reset() {
					carets.clear();
					carets.insert(entry(caret_selection(caret_position(), caret_position()), caret_data(0.0)));
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
				string_buffer::string_type removed_content, added_content;
				caret_position_diff removed_range{}, added_range{};
				caret_position position{}; // position after previous modifications
			};
			using edit = std::vector<modification>;

			struct modification_positions {
				modification_positions() = default;
				explicit modification_positions(const modification &mod) :
					removed_range(mod.removed_range), added_range(mod.added_range), position(mod.position) {
				}
				modification_positions(caret_position p, caret_position_diff rem, caret_position_diff add) :
					removed_range(rem), added_range(add), position(p) {
				}

				caret_position_diff removed_range{}, added_range{};
				caret_position position{};
			};
			struct caret_fixup_info {
			public:
				struct context {
					context() = default;
					explicit context(const caret_fixup_info &src) : next(src.mods.begin()) {
					}

					void append_custom_modification(modification_positions mpos) {
						diff += mpos.added_range - mpos.removed_range;
					}

					std::vector<modification_positions>::const_iterator next;
					caret_position_diff diff{};
				};

				caret_fixup_info() = default;
				explicit caret_fixup_info(const edit &e) {
					mods.reserve(e.size());
					for (auto i = e.begin(); i != e.end(); ++i) {
						mods.push_back(modification_positions(*i));
					}
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
						if (ctx.next == mods.end()) {
							break;
						}
						ctx.append_custom_modification(*ctx.next);
						++ctx.next;
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
					return _fix_raw(cp, ctx.diff);
				}
				caret_position fixup_caret_with_mod(caret_position cp, const modification_positions &mod) const {
					return _fix_raw(cp, mod.added_range - mod.removed_range);
				}

				std::vector<modification_positions> mods;
			protected:
				caret_position _fix_raw(caret_position cp, caret_position_diff diff) const {
					return cp + diff;
				}
			};

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
				using iterator = typename std::map<caret_position, T>::iterator;
				using const_iterator = typename std::map<caret_position, T>::const_iterator;

				text_theme_parameter_info() : text_theme_parameter_info(T()) {
				}
				explicit text_theme_parameter_info(T def) {
					_changes.emplace(std::make_pair(caret_position(), std::move(def)));
				}

				void clear(T def) {
					_changes.clear();
					_changes[caret_position()] = def;
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
				struct iterator {
					friend class text_context;
				public:
					iterator() = default;

					char32_t current_character() const {
						return _cit.current_codepoint();
					}
					bool current_good() const {
						return _cit.current_good();
					}

					iterator &operator++() {
						if (is_linebreak()) {
							for (size_t i = linebreak_registry::get_linebreak_length(_lbit->ending); i > 0; --i) {
								++_cit;
							}
							++_lbit;
							_col = 0;
						} else {
							++_cit;
							++_col;
						}
						return *this;
					}
					iterator operator++(int) {
						iterator oldv;
						++*this;
						return oldv;
					}

					size_t get_column() const {
						return _col;
					}
					size_t get_line_length() const {
						return _lbit->nonbreak_chars;
					}

					// EOF is also treated as a linebreak
					bool is_linebreak() const {
						return _lbit == _lbit.get_container()->end() || _col == _lbit->nonbreak_chars;
					}
					bool is_end() const {
						return _cit.is_end();
					}
				protected:
					iterator(string_buffer::codepoint_iterator c, linebreak_registry::iterator lb, size_t col) :
						_cit(c), _lbit(lb), _col(col) {
						if (_lbit == _lbit.get_container()->end() && _lbit != _lbit.get_container()->begin()) {
							--_lbit;
							_col = _lbit->nonbreak_chars;
						}
					}

					string_buffer::codepoint_iterator _cit;
					linebreak_registry::iterator _lbit;
					size_t _col = 0;
				};

				void clear() {
					_str.clear();
					_lbr.clear();
				}

				void load_from_file(const str_t &fn) {
					logger::get().log_info(CP_HERE, "starting to load file...");
					auto begt = std::chrono::high_resolution_clock::now();
					clear();
					std::ifstream fin(reinterpret_cast<const char*>(convert_to_utf8(fn).c_str()), std::ios::binary);
					fin.seekg(0, std::ios::end);
					auto len = fin.tellg();
					fin.seekg(0, std::ios::beg);
					char8_t *cs = static_cast<char8_t*>(std::malloc(static_cast<size_t>(len)));
					fin.read(reinterpret_cast<char*>(cs), static_cast<std::streamsize>(len));
					auto now = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "read complete in ",
						std::chrono::duration<double, std::milli>(now - begt).count(),
						"ms"
					);
					begt = now;
					insert_text(0, cs, cs + static_cast<int>(len));
					std::free(cs);
					now = std::chrono::high_resolution_clock::now();
					logger::get().log_info(
						CP_HERE, "decode & format complete in ",
						std::chrono::duration<double, std::milli>(now - begt).count(),
						"ms"
					);
					logger::get().log_info(CP_HERE, "file loaded");
				}
				void save_to_file(const str_t &fn) const {
					std::ofstream fout(reinterpret_cast<const char*>(convert_to_utf8(fn).c_str()), std::ios::binary);
					for (auto i = _str.node_begin(); i != _str.node_end(); ++i) {
						fout.write(
							reinterpret_cast<const char*>(i->data()),
							i->length() * sizeof(string_buffer::char_type)
						);
					}
				}

				void auto_set_default_line_ending() {
					size_t n[3]{};
					for (auto i = _lbr.begin(); i != _lbr.end(); ++i) {
						if (i->ending != line_ending::none) {
							++n[static_cast<int>(i->ending) - 1];
						}
					}
					auto choice = std::max_element(n, n + 3) - n;
					logger::get().log_info(CP_HERE, "choosing line ending r: ", n[0], " n: ", n[1], " rn: ", n[2], " chose ", choice);
					set_default_line_ending(static_cast<line_ending>(choice + 1));
				}
				void set_default_line_ending(line_ending l) {
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

				iterator at_char(caret_position pos) const {
					auto pr = _lbr.get_line_and_column_and_codepoint_of_char(pos);
					return iterator(_str.at_codepoint_iterator(std::get<3>(pr)), std::get<0>(pr), std::get<2>(pr));
				}

				size_t num_lines() const {
					return _lbr.num_linebreaks() + 1;
				}

				string_buffer::string_type substring(caret_position beg, caret_position end) const {
					return _str.substring(
						_str.at_codepoint_iterator(_lbr.position_char_to_codepoint(beg)),
						_str.at_codepoint_iterator(_lbr.position_char_to_codepoint(end))
					);
				}

				// modification made by these functions will neither invoke modified nor be recorded
				template <typename Iter> caret_position_diff insert_text(caret_position cp, Iter beg, Iter end) {
					codepoint_iterator_base<Iter> it(beg, end);
					auto pos = _lbr.get_line_and_column_and_codepoint_of_char(cp);
					char32_t last = U'\0';
					std::vector<linebreak_registry::line_info> lines;
					linebreak_registry::line_info curl;
					caret_position_diff totchars = 0;
					_str.insert(_str.at_codepoint_iterator(std::get<3>(pos)), [&](char32_t &c) {
						if (it.at_end()) {
							return false;
						}
						c = it.current_codepoint();
						if (c == U'\n' || last == U'\r') {
							if (c == U'\n') {
								curl.ending = last == U'\r' ? line_ending::rn : line_ending::n;
							} else {
								curl.ending = line_ending::r;
							}
							totchars += curl.nonbreak_chars + 1;
							lines.push_back(curl);
							curl = linebreak_registry::line_info();
						}
						if (c != U'\r' && c != U'\n') {
							++curl.nonbreak_chars;
						}
						last = c;
						++it;
						return true;
					});
					if (last == U'\r') {
						curl.ending = line_ending::r;
						totchars += curl.nonbreak_chars + 1;
						lines.push_back(curl);
						curl = linebreak_registry::line_info();
					}
					totchars += curl.nonbreak_chars;
					lines.push_back(curl);
					_lbr.insert_chars(std::get<0>(pos), std::get<2>(pos), std::move(lines));
					return totchars;
				}
				caret_position_diff insert_text(caret_position cp, const string_buffer::string_type &str) {
					return insert_text(cp, str.begin(), str.end());
				}

				void delete_text(caret_position p1, caret_position p2) {
					auto
						p1i = _lbr.get_line_and_column_and_codepoint_of_char(p1),
						p2i = _lbr.get_line_and_column_and_codepoint_of_char(p2);
					_str.erase(
						_str.at_codepoint_iterator(std::get<3>(p1i)),
						_str.at_codepoint_iterator(std::get<3>(p2i))
					);
					_lbr.erase_chars(std::get<0>(p1i), std::get<2>(p1i), std::get<0>(p2i), std::get<2>(p2i));
				}
				// till here

				const text_theme_data &get_text_theme() const {
					return _theme;
				}
				void set_text_theme(text_theme_data td) {
					_theme = std::move(td);
					visual_changed.invoke();
				}

				const string_buffer &get_string_buffer() const {
					return _str;
				}
				const linebreak_registry &get_linebreak_registry() const {
					return _lbr;
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
				// content
				string_buffer _str;
				linebreak_registry _lbr;
				// text theme
				text_theme_data _theme;
				// undo & redo
				std::vector<edit> _edithist;
				size_t _curedit = 0;
				// settings
				double _tab_w = 4.0;
				line_ending _le = line_ending::n;
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
					if (mod.removed_range != 0) {
						mod.removed_content = _ctx->substring(mod.position, mod.position + mod.removed_range);
						_ctx->delete_text(mod.position, mod.position + mod.removed_range);
					}
					if (mod.added_content.length() > 0) {
						mod.added_range = _ctx->insert_text(mod.position, mod.added_content);
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
					if (mod.removed_content.length() > 0) {
						_ctx->delete_text(mod.position, mod.position + mod.removed_range);
					}
					if (mod.added_content.length() > 0) {
						_ctx->insert_text(mod.position, mod.added_content);
					}
					_append_fixup_item(modification_positions(mod));
					_append_caret(get_caret_selection_after(mod));
				}
				void undo_modification(const modification &mod) {
					caret_position
						pos = fixup_caret_position(mod.position),
						addend = fixup_caret_position(mod.position + mod.added_range),
						delend = fixup_caret_position(mod.position + mod.removed_range);
					if (mod.added_content.length() > 0) {
						_ctx->delete_text(pos, addend);
					}
					if (mod.removed_content.length() > 0) {
						_ctx->insert_text(pos, mod.removed_content);
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

				void on_text_insert(caret_selection cs, string_buffer::string_type s) {
					modification mod(cs);
					mod.caret_front_after = false;
					mod.selected_after = false;
					mod.added_content = std::move(s);
					apply_modification(std::move(mod));
				}
				void on_text_overwrite(caret_selection cs, string_buffer::string_type s) {
					modification mod(cs);
					fixup_caret_position(mod);
					if (!mod.selected_before) {
						text_context::iterator it = _ctx->at_char(mod.position);
						size_t col = it.get_column();
						for (
							codepoint_iterator_base<string_buffer::string_type::const_iterator> cit(s.begin(), s.end());
							!cit.at_end();
							++cit
						) {
							if (!is_newline(cit.current_codepoint()) && col < it.get_line_length()) {
								++mod.removed_range;
							}
						}
						mod.caret_front_before = true;
					}
					mod.added_content = std::move(s);
					apply_modification_nofixup(std::move(mod));
				}
				void on_text(caret_selection cs, string_buffer::string_type s, bool insert) {
					if (insert) {
						on_text_insert(cs, std::move(s));
					} else {
						on_text_overwrite(cs, std::move(s));
					}
				}
				void on_backspace(caret_selection cs) {
					modification mod(cs);
					fixup_caret_position(mod);
					if (!mod.selected_before && mod.position > 0) {
						--mod.position;
						mod.removed_range = 1;
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
					if (!mod.selected_before && mod.position < _ctx->get_linebreak_registry().num_chars()) {
						mod.removed_range = 1;
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
