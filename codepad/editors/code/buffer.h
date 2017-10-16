#pragma once

#include "../../utilities/bst.h"

namespace codepad {
	namespace editor {
		namespace code {
			enum class line_ending : char_t {
				none,
				r,
				n,
				rn
			};
			inline const char8_t *line_ending_to_static_u8_string(line_ending le) {
				switch (le) {
				case line_ending::r:
					return reinterpret_cast<const char8_t*>("\r");
				case line_ending::n:
					return reinterpret_cast<const char8_t*>("\n");
				case line_ending::rn:
					return reinterpret_cast<const char8_t*>("\r\n");
				default:
					break;
				}
				return reinterpret_cast<const char8_t*>("");
			}
			inline const char32_t *line_ending_to_static_u32_string(line_ending le) {
				switch (le) {
				case line_ending::r:
					return U"\r";
				case line_ending::n:
					return U"\n";
				case line_ending::rn:
					return U"\r\n";
				default:
					break;
				}
				return U"";
			}
		}
	}
	template <> inline str_t to_str<editor::code::line_ending>(editor::code::line_ending le) {
		return editor::code::line_ending_to_static_u32_string(le);
	}
	namespace editor {
		namespace code {
			template <typename Char> struct basic_string_compact {
			public:
				constexpr static double space_extend_ratio = 1.5;

				using value_type = Char;
				using iterator = Char*;
				using const_iterator = const Char*;

				basic_string_compact() = default;
				explicit basic_string_compact(const Char *cs) : basic_string_compact(cs, get_unit_count(cs)) {
				}
				basic_string_compact(const Char *cs, size_t len) : basic_string_compact(cs, len, count_codepoints(cs, cs + len)) {
				}
				basic_string_compact(const Char *cs, size_t len, size_t ncp) : _chars(_duplicate_str(cs, len)), _len(len), _res(len), _cp(ncp) {
				}
				basic_string_compact(const basic_string_compact &s) : basic_string_compact(s._chars, s._len, s._cp) {
				}
				basic_string_compact(basic_string_compact &&s) : _chars(s._chars), _len(s._len), _res(s._res), _cp(s._cp) {
					s._chars = nullptr;
					s._len = 0;
					s._res = 0;
					s._cp = 0;
				}
				basic_string_compact &operator=(const basic_string_compact &s) {
					if (_res > s._len) {
						std::memcpy(_chars, s._chars, sizeof(Char) * s._len);
					} else {
						std::free(_chars);
						_chars = _duplicate_str(s._chars, s._len);
						_res = s._len;
					}
					_len = s._len;
					_cp = s._cp;
					return *this;
				}
				basic_string_compact &operator=(basic_string_compact &&s) {
					std::swap(_chars, s._chars);
					std::swap(_len, s._len);
					std::swap(_res, s._res);
					std::swap(_cp, s._cp);
					return *this;
				}
				~basic_string_compact() {
					if (_chars) {
						std::free(_chars);
					}
				}

				Char operator[](size_t id) const {
					assert_true_logical(id < _len, "access violation");
					return _chars[id];
				}
				const Char *data() const {
					return _chars;
				}

				size_t length() const {
					return _len;
				}
				size_t num_codepoints() const {
					return _cp;
				}

				iterator begin() {
					return _chars;
				}
				const_iterator begin() const {
					return _chars;
				}
				iterator end() {
					return _chars + _len;
				}
				const_iterator end() const {
					return _chars + _len;
				}

				iterator at_codepoint(size_t cp) {
					iterator b = begin(), e = end();
					skip_codepoints(b, e, cp);
					return b;
				}
				const_iterator at_codepoint(size_t cp) const {
					const_iterator b = begin(), e = end();
					skip_codepoints(b, e, cp);
					return b;
				}

				void reserve(size_t units) {
					if (_res < units) {
						_res = units;
						Char *nc = static_cast<Char*>(std::malloc(sizeof(Char) * _res));
						if (_chars) {
							std::memcpy(nc, _chars, sizeof(Char) * _len);
							std::free(_chars);
						}
						_chars = nc;
					}
				}

				basic_string_compact substring(size_t beg, size_t len = std::numeric_limits<size_t>::max()) const {
					assert_true_logical(beg <= _len, "invalid substring range");
					return basic_string_compact(_chars + beg, std::min(len, _len - beg));
				}
				basic_string_compact substring(const_iterator beg, const_iterator end) const {
					assert_true_logical(end >= beg, "invalid substring range");
					return substring(static_cast<size_t>(beg - begin()), static_cast<size_t>(end - beg));
				}
				basic_string_compact substring(const_iterator beg, const_iterator end, size_t ncp) const {
					assert_true_logical(end >= beg, "invalid substring range");
					return basic_string_compact(beg, static_cast<size_t>(end - beg), ncp);
				}
				basic_string_compact substring(
					const codepoint_iterator_base<const_iterator> &beg, const codepoint_iterator_base<const_iterator> &end
				) const {
					return basic_string_compact(
						beg.get_raw_iterator(),
						end.get_raw_iterator() - beg.get_raw_iterator(),
						end.codepoint_position() - beg.codepoint_position()
					);
				}

				void append_as_codepoint(std::initializer_list<Char> c) {
					append_as_codepoint(c.begin(), c.end());
				}
				template <typename It> void append_as_codepoint(It beg, It end) {
					++_cp;
					size_t newlen = _len + (end - beg);
					if (newlen > _res) {
						Char *nc = _begin_resize(newlen);
						if (_chars) {
							std::memcpy(nc, _chars, sizeof(Char) * _len);
						}
						_end_resize(nc);
					}
					Char *cc = _chars + _len;
					for (auto i = beg; i != end; ++i, ++cc) {
						*cc = *i;
					}
					_len = newlen;
				}

				void insert(size_t pos, const Char *str, size_t len, size_t codepoints) {
					if (len == 0) {
						return;
					}
					assert_true_logical(pos <= _len, "invalid position");
					_cp += codepoints;
					if (_len + len > _res) {
						Char *dst = _begin_resize(_len + len);
						if (_chars) {
							std::memcpy(dst, _chars, sizeof(Char) * pos);
							std::memcpy(dst + pos + len, _chars + pos, sizeof(Char) * (_len - pos));
						}
						std::memcpy(dst + pos, str, sizeof(Char) * len);
						_end_resize(dst);
					} else {
						for (size_t i = _len, j = _len + len; i > pos; ) {
							_chars[--j] = _chars[--i];
						}
						std::memcpy(_chars + pos, str, sizeof(Char) * len);
					}
					_len += len;
				}
				void insert(size_t pos, const Char *str, size_t len) {
					insert(pos, str, len, count_codepoints(str, str + len));
				}
				void insert(size_t pos, const basic_string_compact &s) {
					insert(pos, s._chars, s._len, s._cp);
				}
				void insert(iterator pos, const basic_string_compact &s) {
					insert(static_cast<size_t>(pos - _chars), s);
				}

				void erase(iterator beg, iterator e, size_t cpc) {
					iterator oldend = end();
					assert_true_logical(beg <= e && e <= oldend, "invalid range");
					_len -= e - beg;
					for (; e != oldend; ++beg, ++e) {
						*beg = *e;
					}
					_cp -= cpc;
				}
				void erase(size_t beg, size_t len, size_t cpc) {
					auto begit = begin() + beg;
					erase(begit, begit + len, cpc);
				}
				void erase(iterator beg, iterator end) {
					erase(beg, end, count_codepoints(beg, end));
				}
				void erase(size_t beg, size_t len) {
					auto begit = begin() + beg;
					erase(begit, begit + len);
				}
				void erase(const codepoint_iterator_base<iterator> &beg, const codepoint_iterator_base<iterator> &end) {
					erase(beg.get_raw_iterator(), end.get_raw_iterator(), end.codepoint_position() - beg.codepoint_position());
				}

				void replace(iterator pos, iterator remend, size_t remcp, const Char *insstr, size_t inslen, size_t inscp) {
					assert_true_logical(pos <= remend && remend <= end(), "invalid range");
					_cp = _cp - remcp + inscp;
					iterator insend = pos + inslen;
					if (remend != insend) {
						iterator oldend = end();
						_len += inslen - (remend - pos);
						if (remend > insend) {
							for (; remend != oldend; ++remend, ++insend) {
								*insend = *remend;
							}
						} else {
							if (_len > _res) {
								Char *dst = _begin_resize(_len);
								size_t posoffset = static_cast<size_t>(pos - _chars);
								if (_chars) {
									std::memcpy(dst, _chars, sizeof(Char) * posoffset);
									std::memcpy(dst + posoffset + inslen, remend, sizeof(Char) * (_len - (posoffset + inslen)));
								}
								std::memcpy(dst + posoffset, insstr, sizeof(Char) * inslen);
								_end_resize(dst);
								return;
							} else {
								for (iterator newend = end(); oldend != remend; ) {
									*--newend = *--oldend;
								}
							}
						}
					}
					std::memcpy(pos, insstr, sizeof(Char) * inslen);
				}
				void replace(iterator pos, iterator remend, size_t remcp, const basic_string_compact &s) {
					replace(pos, remend, remcp, s._chars, s._len, s._cp);
				}
				void replace(iterator pos, iterator remend, const Char *insstr, size_t inslen) {
					replace(pos, remend, count_codepoints(pos, remend), insstr, inslen, count_codepoints(insstr, insstr + inslen));
				}
				void replace(iterator pos, iterator remend, const basic_string_compact &s) {
					replace(pos, remend, count_codepoints(pos, remend), s);
				}
			protected:
				inline static Char *_duplicate_str(const Char *cs, size_t len) {
					if (len == 0) {
						return nullptr;
					}
					Char *c = static_cast<Char*>(std::malloc(sizeof(Char) * len));
					std::memcpy(c, cs, sizeof(Char) * len);
					return c;
				}

				Char *_begin_resize(size_t nsz) {
					_res = std::max(nsz, static_cast<size_t>(static_cast<double>(_res) * space_extend_ratio));
					return static_cast<Char*>(std::malloc(sizeof(Char) * _res));
				}
				void _end_resize(Char *c) {
					if (_chars) {
						std::free(_chars);
					}
					_chars = c;
				}

				Char *_chars = nullptr;
				size_t _len = 0, _res = 0, _cp = 0;
			};
			using u8string_compact = basic_string_compact<char8_t>;
			using u16string_compact = basic_string_compact<char16_t>;
			using u32string_compact = basic_string_compact<char32_t>;

			class string_buffer {
			public:
				constexpr static size_t maximum_units_per_chunk = 1000;

				using char_type = char8_t;
				using string_type = basic_string_compact<char_type>;
				struct node_data {
				public:
					using node = binary_tree_node<string_type, node_data>;

					size_t total_length = 0, total_codepoints = 0;

					void synthesize(const node &tn) {
						total_length = tn.value.length();
						total_codepoints = tn.value.num_codepoints();
						if (tn.left) {
							_add(tn.left->synth_data);
						}
						if (tn.right) {
							_add(tn.right->synth_data);
						}
					}
				protected:
					void _add(const node_data &s) {
						total_length += s.total_length;
						total_codepoints += s.total_codepoints;
					}
				};
				using tree_type = binary_tree<string_type, node_data>;
				using node_type = tree_type::node;

				template <typename TIt, typename SIt> struct iterator_base {
					friend class string_buffer;
				public:
					iterator_base() = default;
					iterator_base(const iterator_base<tree_type::iterator, string_type::iterator> &it) :
						_it(it._it), _s(it._s) {
					}

					iterator_base &operator++() {
						if (++_s == _it->end()) {
							_inc_it();
						}
						return *this;
					}
					iterator_base operator++(int) {
						iterator_base ov = *this;
						++*this;
						return ov;
					}
					iterator_base &operator--() {
						if (_it == _it.get_container()->end() || _s == _it->begin()) {
							--_it;
							_s = _it->end();
						}
						--_s;
						return *this;
					}
					iterator_base operator--(int) {
						iterator_base ov = *this;
						--*this;
						return ov;
					}

					bool next_codepoint() {
						bool res = next_codepoint(_s, nullptr);
						if (_s == _it.end()) {
							_inc_it();
						}
						return res;
					}
					bool next_codepoint(char32_t &c) {
						bool res = next_codepoint(_s, nullptr, c);
						if (_s == _it.end()) {
							_inc_it();
						}
						return res;
					}

					friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
						return lhs._it == rhs._it && lhs._s == rhs._s;
					}
					friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
						return !(lhs == rhs);
					}

					char_type operator*() const {
						return *_s;
					}
				protected:
					iterator_base(const TIt &t, const SIt &s) : _it(t), _s(s) {
					}

					void _inc_it() {
						++_it;
						_s = _it != _it.get_container()->end() ? _it->begin() : SIt();
					}

					TIt _it;
					SIt _s{};
				};
				using iterator = iterator_base<tree_type::iterator, string_type::iterator>;
				using const_iterator = iterator_base<tree_type::const_iterator, string_type::const_iterator>;

				struct codepoint_iterator {
					friend class string_buffer;
				public:
					codepoint_iterator() = default;

					char32_t operator*() const {
						return current_codepoint();
					}
					char32_t current_codepoint() const {
						return _cit.current_codepoint();
					}
					bool current_good() const {
						return _cit.current_good();
					}

					codepoint_iterator &operator++() {
						if (_cit.next_end()) {
							++_tit;
							_cit = _get_cit();
						} else {
							++_cit;
						}
						return *this;
					}

					bool is_end() const {
						return _tit == _tit.get_container()->end();
					}
				protected:
					using _str_cp_iter = codepoint_iterator_base<string_type::iterator>;

					explicit codepoint_iterator(const tree_type::const_iterator &it) :
						_tit(it), _cit(_get_cit()) {
					}
					codepoint_iterator(const tree_type::const_iterator &it, size_t at) :
						_tit(it), _cit(_get_cit(at)) {
					}

					_str_cp_iter _get_cit() const {
						if (_tit == _tit.get_container()->end()) {
							return _str_cp_iter(nullptr, nullptr, 0);
						}
						return _str_cp_iter(_tit.get_node()->value.begin(), _tit.get_node()->value.end());
					}
					_str_cp_iter _get_cit(size_t pos) const {
						if (_tit == _tit.get_container()->end()) {
							return _str_cp_iter(nullptr, nullptr, 0);
						}
						return _str_cp_iter(_tit.get_node()->value.at_codepoint(pos), _tit.get_node()->value.end(), pos);
					}

					tree_type::const_iterator _tit;
					_str_cp_iter _cit;
				};

				iterator begin() {
					auto t = _t.begin();
					return iterator(t, t->begin());
				}
				const_iterator begin() const {
					auto t = _t.begin();
					return const_iterator(t, t->begin());
				}
				iterator end() {
					return iterator(_t.end(), string_type::iterator());
				}
				const_iterator end() const {
					return const_iterator(_t.end(), string_type::const_iterator());
				}

				tree_type::const_iterator node_begin() const {
					return _t.begin();
				}
				tree_type::const_iterator node_end() const {
					return _t.end();
				}

				iterator at_codepoint(size_t cp) {
					auto t = _t.find_custom(_codepoint_index_finder(), cp);
					if (t == _t.end()) {
						return iterator(t, string_type::iterator());
					}
					return iterator(t, t->at_codepoint(cp));
				}
				const_iterator at_codepoint(size_t cp) const {
					auto t = _t.find_custom(_codepoint_index_finder(), cp);
					if (t == _t.end()) {
						return const_iterator(t, string_type::const_iterator());
					}
					return const_iterator(t, t->at_codepoint(cp));
				}
				iterator at_unit(size_t cp) {
					auto t = _t.find_custom(_unit_index_finder(), cp);
					if (t == _t.end()) {
						return iterator(t, string_type::iterator());
					}
					return iterator(t, t->begin() + cp);
				}
				const_iterator at_unit(size_t cp) const {
					auto t = _t.find_custom(_unit_index_finder(), cp);
					if (t == _t.end()) {
						return const_iterator(t, string_type::const_iterator());
					}
					return const_iterator(t, t->begin() + cp);
				}

				size_t get_position_units(const_iterator i) const {
					return _units_before(i._it.get_node()) + (i._s - i._it->begin());
				}

				codepoint_iterator begin_codepoint() const {
					return codepoint_iterator(_t.begin());
				}
				codepoint_iterator at_codepoint_iterator(size_t pos) const {
					auto it = _t.find_custom(_codepoint_index_finder(), pos);
					return codepoint_iterator(it, pos);
				}
				size_t get_position(const codepoint_iterator &i) const {
					return _codepoints_before(i._tit.get_node()) + i._cit.codepoint_position();
				}

				template <typename T> void set(const T &getcodepoint) {
					std::vector<string_type> strings;
					string_type current;
					current.reserve(maximum_units_per_chunk);
					for (char32_t cc = U'\0'; getcodepoint(cc); translate_codepoint_utf8(
						[&current, &strings](std::initializer_list<char_type> chars) {
							if (current.length() + chars.size() > maximum_units_per_chunk) {
								strings.push_back(std::move(current));
								current = string_type();
								current.reserve(maximum_units_per_chunk);
							}
							current.append_as_codepoint(std::move(chars));
						}, cc
					)) {
					}
					if (current.length() > 0) {
						strings.push_back(std::move(current));
					}
					_t.clear();
					_t = tree_type(std::move(strings));
				}
				string_type substring(codepoint_iterator beg, codepoint_iterator end) const {
					if (beg._tit.get_node() == nullptr) {
						return string_type();
					}
					if (beg._tit == end._tit) {
						return beg._tit->substring(beg._cit, end._cit);
					}
					string_type result;
					result.reserve(get_position(end) - get_position(beg));
					result.insert(
						0, beg._cit.get_raw_iterator(),
						beg._tit->end() - beg._cit.get_raw_iterator(),
						beg._tit->num_codepoints() - beg._cit.codepoint_position()
					);
					tree_type::const_iterator it = beg._tit;
					for (++it; it != end._tit; ++it) {
						result.insert(result.end(), *it);
					}
					if (end._tit != _t.end()) {
						result.insert(
							result.length(), end._tit->begin(),
							end._cit.get_raw_iterator() - end._tit->begin(),
							end._cit.codepoint_position()
						);
					}
					return result;
				}
				void erase(const codepoint_iterator &beg, const codepoint_iterator &end) {
					node_type *bnode = beg._tit.get_node(), *enode = end._tit.get_node();
					if (bnode == nullptr) {
						return;
					}
					if (beg._tit == end._tit) {
						bnode->value.erase(beg._cit, end._cit);
						_t.refresh_synthesized_result(bnode);
						_try_merge_small_nodes(bnode);
						return;
					}
					_t.erase(bnode->next(), enode);
					bnode->value.erase(
						beg._cit.get_raw_iterator(), bnode->value.end(),
						bnode->value.num_codepoints() - beg._cit.codepoint_position()
					);
					_t.refresh_synthesized_result(bnode);
					if (enode) {
						enode->value.erase(
							enode->value.begin(), end._cit.get_raw_iterator(),
							end._cit.codepoint_position()
						);
						_t.refresh_synthesized_result(enode);
						_try_merge_small_nodes(enode);
					}
					_try_merge_small_nodes(bnode);
				}
				template <typename T> void insert(const codepoint_iterator &pos, const T &getcodepoint) {
					node_type *node2ins = pos._tit.get_node(), *node2upd = nullptr;
					string_type afterstr, *curstr;
					std::vector<string_type> strs;
					if (node2ins != nullptr && pos._cit.get_raw_iterator() != pos._tit->begin()) {
						size_t ncp = node2ins->value.num_codepoints() - pos._cit.codepoint_position();
						afterstr = node2ins->value.substring(pos._cit.get_raw_iterator(), node2ins->value.end(), ncp);
						node2ins->value.erase(pos._cit.get_raw_iterator(), node2ins->value.end(), ncp);
						node2upd = node2ins;
						node2ins = node2ins->next();
						curstr = &node2upd->value;
					} else if (pos._tit == _t.begin()) {
						string_type st;
						st.reserve(maximum_units_per_chunk);
						strs.push_back(std::move(st));
						curstr = &strs.back();
					} else {
						tree_type::const_iterator it = pos._tit;
						--it;
						node2upd = it.get_node();
						curstr = &node2upd->value;
					}
					for (char32_t cc = U'\0'; getcodepoint(cc); translate_codepoint_utf8(
						[&curstr, &strs](std::initializer_list<char_type> chars) {
							if (curstr->length() + chars.size() > maximum_units_per_chunk) {
								string_type ss;
								ss.reserve(maximum_units_per_chunk);
								strs.push_back(std::move(ss));
								curstr = &strs.back();
							}
							curstr->append_as_codepoint(std::move(chars));
						}, cc
					)) {
					}
					if (afterstr.length() > 0) {
						if (curstr->length() + afterstr.length() <= maximum_units_per_chunk) {
							curstr->insert(curstr->end(), afterstr);
						} else {
							strs.push_back(std::move(afterstr)); // curstr is not changed!
						}
					} else if (curstr->length() == 0) {
						assert_true_logical(strs.size() > 0, "corrupted string buffer");
						strs.pop_back();
					}
					_t.refresh_synthesized_result(node2upd);
					if (strs.size() > 0) {
						_t.insert_tree_before(node2ins, std::move(strs));
					}
					node_type
						*lstnode = node2ins ? node2ins->prev() : _t.max(),
						*llstnode = lstnode ? lstnode->prev() : nullptr;
					_try_merge_small_nodes(lstnode);
					_try_merge_small_nodes(llstnode);
				}

				size_t length() const {
					node_type *n = _t.root();
					return n ? n->synth_data.total_length : 0;
				}
				size_t num_codepoints() const {
					node_type *n = _t.root();
					return n ? n->synth_data.total_codepoints : 0;
				}

				void clear() {
					_t.clear();
				}
			protected:
				template <size_t node_data::*Index, size_t(string_type::*NodeProp)() const> struct _index_finder {
					int select_find(const node_type &cur, size_t &target) const {
						if (cur.left) {
							if (target < cur.left->synth_data.*Index) {
								return -1;
							}
							target -= cur.left->synth_data.*Index;
						}
						if (target < (cur.value.*NodeProp)()) {
							return 0;
						}
						target -= (cur.value.*NodeProp)();
						return 1;
					}
				};
				using _codepoint_index_finder = _index_finder<&node_data::total_codepoints, &string_type::num_codepoints>;
				using _unit_index_finder = _index_finder<&node_data::total_length, &string_type::length>;

				template <
					size_t node_data::*Field, size_t(string_type::*NodeProp)() const
				> size_t _synth_sum_before(const node_type *n) const {
					if (n == nullptr) {
						return _t.root() ? _t.root()->synth_data.*Field : 0;
					}
					size_t result = n->left ? n->left->synth_data.*Field : 0;
					_t.synthesize_root_path(n, [&result](const node_type &p, const node_type &c) {
						if (&c == p.right) {
							result += (p.value.*NodeProp)();
							if (p.left) {
								result += p.left->synth_data.*Field;
							}
						}
						});
					return result;
				}
				size_t _codepoints_before(const node_type *n) const {
					return _synth_sum_before<&node_data::total_codepoints, &string_type::num_codepoints>(n);
				}
				size_t _units_before(const node_type *n) const {
					return _synth_sum_before<&node_data::total_length, &string_type::length>(n);
				}

				void _try_merge_small_nodes(node_type *n) {
					if (n == nullptr) {
						return;
					}
					size_t nvl = n->value.length();
					if (nvl * 2 > maximum_units_per_chunk) {
						return;
					}
					node_type *prev = n->prev();
					if (prev && prev->value.length() + nvl < maximum_units_per_chunk) {
						prev->value.insert(prev->value.end(), n->value);
						_t.refresh_synthesized_result(prev);
						_t.erase(n);
						return;
					}
					node_type *next = n->next();
					if (next && next->value.length() + nvl < maximum_units_per_chunk) {
						n->value.insert(n->value.end(), next->value);
						_t.refresh_synthesized_result(n);
						_t.erase(next);
						return;
					}
					// TODO maybe average the two nodes? or just give up?
				}

				tree_type _t;
			};

			template <typename Data> struct incremental_positional_registry {
			public:
				struct node_data {
					node_data() = default;
					node_data(size_t len, Data obj) : length(len), object(std::move(obj)) {
					}

					size_t length = 0;
					Data object;
				};
				struct node_synth_data {
					using node_type = binary_tree_node<node_data, node_synth_data>;

					size_t total_length = 0;

					void synthesize(const node_type &n) {
						total_length = n.value.length;
						if (n.left) {
							total_length += n.left->synth_data.total_length;
						}
						if (n.right) {
							total_length += n.right->synth_data.total_length;
						}
					}
				};
				using tree_type = binary_tree<node_data, node_synth_data>;
				using node_type = typename tree_type::node;
				template <bool Const> struct iterator_base {
				public:
					using raw_iterator_t = std::conditional_t<
						Const, typename tree_type::const_iterator, typename tree_type::iterator
					>;
					using dereferenced_t = std::conditional_t<Const, const Data, Data>;
					using reference = dereferenced_t&;
					using pointer = dereferenced_t*;

					iterator_base() = default;
					explicit iterator_base(const raw_iterator_t &it) : _it(it) {
					}

					reference operator*() const {
						return _it->object;
					}
					pointer operator->() const {
						return &operator*();
					}

					friend bool operator==(const iterator_base &lhs, const iterator_base &rhs) {
						return lhs._it == rhs._it;
					}
					friend bool operator!=(const iterator_base &lhs, const iterator_base &rhs) {
						return !(lhs == rhs);
					}

					iterator_base &operator++() {
						++_it;
						return *this;
					}
					iterator_base operator++(int) {
						iterator_base oldv = *this;
						++*this;
						return oldv;
					}

					iterator_base &operator--() {
						--_it;
						return *this;
					}
					iterator_base operator--(int) {
						iterator_base oldv = *this;
						--*this;
						return oldv;
					}

					const node_data &data() const {
						return *get_raw();
					}
					const raw_iterator_t &get_raw() const {
						return _it;
					}
				protected:
					raw_iterator_t _it;
				};
				using iterator = iterator_base<false>;
				using const_iterator = iterator_base<true>;

				void insert_at(const iterator &pos, size_t offset, Data d) {
					if (pos.get_raw() != _t.end()) {
						assert_true_usage(offset <= pos.data().length, "invalid position");
						pos.get_raw().get_node()->value.length -= offset;
						_t.refresh_synthesized_result(pos.get_raw().get_node());
					}
					_t.insert_node_before(pos.get_raw().get_node(), offset, std::move(d));
				}
				void insert_at(size_t pos, Data d) {
					auto it = _t.find_custom(_find_obj(), pos);
					insert_at(iterator(it), pos, std::move(d));
				}

				void remove(const_iterator it) {
					assert_true_usage(it.get_raw() != _t.end(), "invalid position");
					auto next = it;
					++next;
					if (next.get_raw() != _t.end()) {
						next.get_raw().get_node()->value.length += it.data().length;
						_t.refresh_synthesized_result(next.get_raw().get_node());
					}
					_t.erase(it.get_raw());
				}

				iterator begin() {
					return iterator(_t.begin());
				}
				iterator end() {
					return iterator(_t.end());
				}
				const_iterator begin() const {
					return const_iterator(_t.begin());
				}
				const_iterator end() const {
					return const_iterator(_t.end());
				}

				iterator find_at_or_first_after(size_t pos) {
					return _find_at_or_first_after_impl<iterator>(*this, pos);
				}
				const_iterator find_at_or_first_after(size_t pos) const {
					return _find_at_or_first_after_impl<const_iterator>(*this, pos);
				}

				void clear() {
					_t.clear();
				}
			protected:
				template <
					typename It, typename Cont
				> inline static It _find_at_or_first_after_impl(Cont &cnt, size_t &pos) {
					return It(cnt._t.find_custom(_find_obj(), pos));
				}

				tree_type _t;

				struct _find_obj {
					int select_find(const node_type &n, size_t &c) {
						if (n.left) {
							if (c <= n.left->synth_data.total_length) {
								return -1;
							}
							c -= n.left->synth_data.total_length;
						}
						if (c <= n.value.length) {
							return 0;
						}
						c -= n.value.length;
						return 1;
					}
				};
			};
			class linebreak_registry {
			public:
				struct line_info {
					line_info() = default;
					line_info(size_t d, line_ending t) : nonbreak_chars(d), ending(t) {
					}

					size_t nonbreak_chars = 0;
					line_ending ending = line_ending::none;
				};
				struct line_synth_data {
				public:
					using node_type = binary_tree_node<line_info, line_synth_data>;

					size_t
						total_codepoints = 0, node_codepoints = 0,
						total_chars = 0, node_chars = 0,
						num_linebreaks = 0;

					void synthesize(const node_type &n) {
						num_linebreaks = (n.value.ending == line_ending::none ? 0 : 1);
						total_codepoints = node_codepoints = n.value.nonbreak_chars + get_linebreak_length(n.value.ending);
						total_chars = node_chars = n.value.nonbreak_chars + num_linebreaks;
						if (n.left) {
							_add(n.left->synth_data);
						}
						if (n.right) {
							_add(n.right->synth_data);
						}
					}
				protected:
					void _add(const line_synth_data &lt) {
						total_codepoints += lt.total_codepoints;
						total_chars += lt.total_chars;
						num_linebreaks += lt.num_linebreaks;
					}
				};
				using tree_type = binary_tree<line_info, line_synth_data>;
				using node_type = tree_type::node;
				using iterator = tree_type::const_iterator;

				linebreak_registry() {
					_t.insert_node_before(nullptr);
				}

				size_t position_char_to_codepoint(size_t c) const {
					_pos_char_to_cp selector;
					_t.find_custom(selector, c);
					return selector.total_codepoints + c;
				}
				size_t get_beginning_char_of_line(size_t l) const {
					_get_line_beg<&line_synth_data::total_chars, &line_synth_data::node_chars> selector;
					_t.find_custom(selector, l);
					return selector.total;
				}
				size_t get_past_ending_char_of_line(size_t l) const {
					_get_line_beg<&line_synth_data::total_chars, &line_synth_data::node_chars> selector;
					auto it = _t.find_custom(selector, l);
					if (it != _t.end()) {
						return selector.total + it->nonbreak_chars;
					}
					return selector.total;
				}
				size_t get_beginning_codepoint_of_line(size_t l) const {
					_get_line_beg<&line_synth_data::total_codepoints, &line_synth_data::node_codepoints> selector;
					_t.find_custom(selector, l);
					return selector.total;
				}

				iterator begin() const {
					return _t.begin();
				}
				iterator end() const {
					return _t.end();
				}
				iterator at_line(size_t line) const {
					return _t.find_custom(_get_line_beg<nullptr, nullptr>(), line);
				}

				std::tuple<iterator, size_t, size_t> get_line_and_column_of_codepoint(size_t cp) const {
					_get_line<&line_synth_data::total_codepoints, &line_synth_data::node_codepoints> selector;
					iterator it = _t.find_custom(selector, cp);
					return std::make_tuple(it, selector.total_lines, cp);
				}
				std::tuple<iterator, size_t, size_t> get_line_and_column_of_char(size_t c) const {
					_get_line<&line_synth_data::total_chars, &line_synth_data::node_chars> selector;
					iterator it = _t.find_custom(selector, c);
					return std::make_tuple(it, selector.total_lines, c);
				}
				std::tuple<iterator, size_t, size_t, size_t> get_line_and_column_and_codepoint_of_char(size_t c) const {
					_pos_char_to_cp selector;
					iterator it = _t.find_custom(selector, c);
					return std::make_tuple(it, selector.total_lines, c, selector.total_codepoints + c);
				}

				size_t get_line(iterator i) const {
					return get_line_of_iterator(i);
				}
				size_t get_beginning_codepoint_of(iterator i) const {
					return _get_node_sum_before<&line_synth_data::total_codepoints, &line_synth_data::node_codepoints>(i.get_node());
				}
				size_t get_beginning_char_of(iterator i) const {
					return _get_node_sum_before<&line_synth_data::total_chars, &line_synth_data::node_chars>(i.get_node());
				}

				void insert_chars(iterator at, size_t offset, std::vector<line_info> lines) {
					assert_true_logical(!(at == _t.end() && offset != 0), "invalid insert position");
					assert_true_logical(lines.size() > 0 && lines.back().ending == line_ending::none, "invalid insertion");
					if (at == _t.end()) {
						node_type *maxn = _t.max();
						if (maxn) {
							lines[0].nonbreak_chars += maxn->value.nonbreak_chars;
							_t.erase(maxn);
						}
						_t.insert_tree_before(at.get_node(), std::move(lines));
					} else {
						lines[0].nonbreak_chars += offset;
						at.get_node()->value.nonbreak_chars += lines.back().nonbreak_chars - offset;
						_t.refresh_synthesized_result(at.get_node());
						lines.pop_back();
						if (lines.size() > 0) {
							_t.insert_tree_before(at.get_node(), std::move(lines));
						}
					}
				}

				void erase_chars(size_t beg, size_t end) {
					std::tuple<iterator, size_t, size_t>
						begp = get_line_and_column_of_char(beg),
						endp = get_line_and_column_of_char(end);
					erase_chars(std::get<0>(begp), std::get<2>(begp), std::get<0>(endp), std::get<2>(endp));
				}
				void erase_chars(iterator beg, size_t begoff, iterator end, size_t endoff) {
					assert_true_logical(!(end == _t.end() && endoff != 0), "invalid iterator position");
					_t.erase(beg.get_node(), end.get_node());
					if (end != _t.end()) {
						end.get_node()->value.nonbreak_chars += begoff - endoff;
						_t.refresh_synthesized_result(end.get_node());
					}
				}

				size_t num_linebreaks() const {
					return _t.root() ? _t.root()->synth_data.num_linebreaks : 0;
				}
				size_t num_chars() const {
					return _t.root() ? _t.root()->synth_data.total_chars : 0;
				}
				void clear() {
					_t.clear();
				}

				inline static size_t get_linebreak_length(line_ending le) {
					switch (le) {
					case line_ending::none:
						return 0;
					case line_ending::n:
					case line_ending::r:
						return 1;
					case line_ending::rn:
						return 2;
					}
					return 0;
				}
				inline static size_t get_line_of_iterator(iterator i) {
					const tree_type *t = i.get_container();
					if (i == t->end()) {
						return t->root() ? t->root()->synth_data.num_linebreaks : 0;
					}
					size_t result = i.get_node()->left ? i.get_node()->left->synth_data.num_linebreaks : 0;
					t->synthesize_root_path(i.get_node(), [&result](const node_type &p, const node_type &n) {
						if (&n == p.right) {
							++result;
							if (p.left) {
								result += p.left->synth_data.num_linebreaks;
							}
						}
						});
					return result;
				}
			protected:
				tree_type _t; // one line break per node

				struct _pos_char_to_cp {
					int select_find(const node_type &n, size_t &c) {
						if (n.left) {
							if (c < n.left->synth_data.total_chars) {
								return -1;
							}
							c -= n.left->synth_data.total_chars;
							total_codepoints += n.left->synth_data.total_codepoints;
							total_lines += n.left->synth_data.num_linebreaks;
						}
						if (c < n.synth_data.node_chars || n.right == nullptr) {
							return 0;
						}
						c -= n.synth_data.node_chars;
						total_codepoints += n.synth_data.node_codepoints;
						++total_lines;
						return 1;
					}
					size_t total_codepoints = 0, total_lines = 0;
				};
				template <size_t line_synth_data::*Total, size_t line_synth_data::*Node> struct _get_line {
					int select_find(const node_type &n, size_t &c) {
						if (n.left) {
							if (c < n.left->synth_data.*Total) {
								return -1;
							}
							c -= n.left->synth_data.*Total;
							total_lines += n.left->synth_data.num_linebreaks;
						}
						if (c < n.synth_data.*Node || n.right == nullptr) {
							return 0;
						}
						c -= n.synth_data.*Node;
						++total_lines;
						return 1;
					}
					size_t total_lines = 0;
				};
				template <size_t line_synth_data::*Total, size_t line_synth_data::*Node> struct _get_line_beg {
					int select_find(const node_type &n, size_t &l) {
						if (n.left) {
							if (l < n.left->synth_data.num_linebreaks) {
								return -1;
							}
							l -= n.left->synth_data.num_linebreaks;
							if (Total != nullptr) {
								total += n.left->synth_data.*Total;
							}
						}
						if (l == 0) {
							return 0;
						}
						--l;
						if (Node != nullptr) {
							total += n.synth_data.*Node;
						}
						return 1;
					}
					size_t total = 0;
				};

				template <
					size_t line_synth_data::*Total, size_t line_synth_data::*Node
				> size_t _get_node_sum_before(const node_type *tg) const {
					if (tg == nullptr) {
						return _t.root() ? _t.root()->synth_data.*Total : 0;
					}
					size_t result = tg->left ? tg->left->synth_data.*Total : 0;
					_t.synthesize_root_path(tg, [&result](const node_type &p, const node_type &n) {
						if (&n == p.right) {
							result += p.synth_data.*Node;
							if (p.left) {
								result += p.left->synth_data.*Total;
							}
						}
						});
					return result;
				};
			};
		}
	}
}
