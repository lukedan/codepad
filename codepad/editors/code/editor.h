#pragma once

#include "context.h"

namespace codepad {
	namespace editor {
		namespace code {
			struct character_rendering_iterator;

			class editor : public component {
			public:
				constexpr static double
					move_speed_scale = 15.0,
					dragdrop_distance = 5.0;

				struct caret_selection_data {
					caret_selection_data() = default;
					caret_selection_data(caret_position c, caret_position se, double align) :
						caret(c), selection_end(se), alignment(align) {
					}

					caret_position caret, selection_end;
					double alignment = 0.0;
				};
				typedef std::pair<caret_position, caret_position> caret_selection;
				struct caret_data {
					caret_data() = default;
					caret_data(double align) : alignment(align) {
					}
					double alignment = 0.0;
				};
				struct caret_set {
					typedef std::map<caret_selection, caret_data> container;
					typedef std::pair<caret_selection, caret_data> entry;
					typedef container::iterator iterator;
					typedef container::const_iterator const_iterator;

					container carets;

					container::iterator add(caret_selection_data p) {
						return add_caret(carets, p);
					}
					container::iterator add(caret_selection_data p, bool &merged) {
						return add_caret(carets, p, merged);
					}

					inline static container::iterator add_caret(container &mp, caret_selection_data c) {
						bool dummy;
						return add_caret(mp, c, dummy);
					}
					inline static container::iterator add_caret(container &mp, caret_selection_data c, bool &merged) {
						merged = false;
						auto minmaxv = std::minmax({c.caret, c.selection_end});
						auto beg = mp.lower_bound(std::make_pair(minmaxv.first, minmaxv.first));
						if (beg != mp.begin()) {
							--beg;
						}
						caret_set::entry res = std::make_pair(std::make_pair(c.caret, c.selection_end), c.alignment);
						while (beg != mp.end() && std::min(beg->first.first, beg->first.second) <= minmaxv.second) {
							if (can_merge_selection(
								res.first.first, res.first.second,
								beg->first.first, beg->first.second,
								res.first.first, res.first.second
							)) {
								beg = mp.erase(beg);
								merged = true;
							} else {
								++beg;
							}
						}
						return mp.insert(res).first;
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

				void set_context(text_context *nctx) {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->theme_changed -= _tc_tok;
					}
					_ctx = nctx;
					if (_ctx) {
						_mod_tok = (_ctx->modified += std::bind(&editor::_on_content_modified, this));
						_tc_tok = (_ctx->theme_changed += std::bind(&editor::_on_content_theme_changed, this));
					}
					_on_content_modified();
				}
				text_context *get_context() const {
					return _ctx;
				}

				void set_tab_width(double v) {
					_tab_w = v;
					invalidate_visual();
				}
				double get_tab_width() const {
					return _tab_w;
				}

				void auto_set_line_ending() {
					size_t n[3] = {0, 0, 0};
					for (auto i = _ctx->begin(); i != _ctx->end(); ++i) {
						if (i->ending_type != line_ending::none) {
							++n[static_cast<int>(i->ending_type) - 1];
						}
#ifdef CP_DETECT_LOGICAL_ERRORS
						if (i->ending_type == line_ending::none) {
							auto ni = i;
							assert_true_logical((++ni) == _ctx->end(), "invalid ending type encountered");
						}
#endif
					}
					_le = static_cast<line_ending>(n[0] > n[1] && n[0] > n[2] ? 1 : (n[1] > n[2] ? 2 : 3));
					logger::get().log_info(CP_HERE, "\\r ", n[0], ", \\n ", n[1], ", \\r\\n ", n[2], ", selected ", static_cast<int>(_le));
				}
				void set_line_ending(line_ending l) {
					assert_true_usage(l != line_ending::none, "invalid line ending");
					_le = l;
				}
				line_ending get_line_ending() const {
					return _le;
				}

				double get_scroll_delta() const {
					return get_line_height() * _lines_per_scroll;
				}
				double get_vertical_scroll_range() const {
					return get_line_height() * static_cast<double>(_ctx->num_lines() - 1) + get_client_region().height();
				}

				double get_line_height() const {
					return _font.maximum_height();
				}

				ui::cursor get_current_display_cursor() const override {
					if (!_selecting && _is_in_selection(_mouse_cache)) {
						return ui::cursor::normal;
					}
					return ui::cursor::text_beam;
				}

				const caret_set &get_carets() const {
					return _cset;
				}

				bool can_undo() {
					return _nmodid > 0;
				}
				void undo() {
					assert_true_usage(can_undo(), "cannot undo");
					_undo_last();
				}
				bool try_undo() {
					if (can_undo()) {
						_undo_last();
						return true;
					}
					return false;
				}
				bool can_redo() {
					return _nmodid < _modstksz;
				}
				void redo() {
					assert_true_usage(can_redo(), "cannot redo");
					_redo_last();
				}
				bool try_redo() {
					if (can_redo()) {
						_redo_last();
						return true;
					}
					return false;
				}

				std::pair<size_t, size_t> get_visible_lines() const {
					double lh = get_line_height(), pos = _get_box()->get_vertical_position();
					return std::make_pair(static_cast<size_t>(std::max(0.0, pos / lh)), std::min(
						static_cast<size_t>((pos + get_client_region().height()) / lh) + 1, get_context()->num_lines()
					));
				}

				event<void> display_changed, content_modified;

				inline static void set_font(const ui::font_family &ff) {
					_font = ff;
				}
				inline static const ui::font_family &get_font() {
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

				inline static void set_num_lines_per_scroll(double v) {
					_lines_per_scroll = v;
				}
				inline static double get_num_lines_per_scroll() {
					return _lines_per_scroll;
				}
			protected:
				text_context *_ctx = nullptr;
				event<void>::token _mod_tok, _tc_tok;
				// settings
				double _tab_w = 4.0;
				line_ending _le;
				// current state
				double _scrolldiff = 0.0;
				caret_set _cset;
				caret_selection_data _cur_sel;
				vec2d _predrag_pos;
				bool _insert = true, _predrag = false, _selecting = false;
				// cache
				caret_position _mouse_cache;
#ifndef NDEBUG
				// debug stuff
				bool _modifying = false;
#endif

				static const ui::basic_pen *_caretpen;
				static const ui::basic_brush *_selbrush;
				static ui::font_family _font;
				static double _lines_per_scroll;

				size_t _hit_test_for_caret_x(const text_context::line &ln, size_t line_num, double pos) const {
					ui::line_character_iterator it(ln.content, _font, _tab_w);
					text_theme_data::char_iterator ci = _ctx->get_text_theme().get_iter_at(caret_position(line_num, 0));
					size_t i = 0;
					for (
						it.begin(ci.current_theme.style);
						!it.ended();
						_ctx->get_text_theme().incr_iter(ci, caret_position(line_num, ++i)), it.next(ci.current_theme.style)
						) {
						if (pos < (it.char_right() + it.prev_char_right()) * 0.5) {
							return i;
						}
					}
					return ln.content.length();
				}
				double _get_caret_pos_x(const text_context::line &ln, caret_position pos) const {
					ui::line_character_iterator it(ln.content, _font, _tab_w);
					text_theme_data::char_iterator ci = _ctx->get_text_theme().get_iter_at(caret_position(pos.line, 0));
					size_t i = 0;
					for (
						it.begin(ci.current_theme.style);
						i < pos.column;
						_ctx->get_text_theme().incr_iter(ci, caret_position(pos.line, ++i)), it.next(ci.current_theme.style)
						) {
					}
					return it.prev_char_right();
				}
				double _get_caret_pos_x(caret_position pos) const {
					return _get_caret_pos_x(*_ctx->at(pos.line), pos);
				}

				struct _modification {
					_modification() = default;
					_modification(caret_position fp, caret_position rp, bool cfront, bool csel, bool add) :
						front_pos(fp), rear_pos(rp), caret_front(cfront), caret_sel(csel), addition(add) {
					}
					_modification(caret_position fp, caret_position rp, bool cfront, bool csel, bool add, str_t s) :
						front_pos(fp), rear_pos(rp), caret_front(cfront), caret_sel(csel), addition(add), content(std::move(s)) {
					}

					caret_position front_pos, rear_pos; // positions of caret, AFTER all modifications.
					bool caret_front, caret_sel, addition, end_of_group = false;
					str_t content;
				};
				struct _modpack {
					std::vector<_modification> mods;
				};
				struct _modify_iterator {
				public:
					_modify_iterator() = default;
					_modify_iterator(const _modify_iterator&) = delete;
					_modify_iterator &operator=(const _modify_iterator&) = delete;

					void redo_modification(const _modification &mod) {
						if (mod.addition) {
							_insert_text(mod.content, mod.caret_sel, mod.caret_front);
						} else {
							_delete_selection(false);
						}
					}
					void undo_modification(const _modification &mod) {
						if (mod.addition) {
							_delete_selection(false);
						} else {
							_insert_text(mod.content, mod.caret_sel, mod.caret_front);
						}
					}

					void insert_text(const str_t &s, bool selected) { // next() or end() should be called immediately afterwards
						_modified = true;
						if (_smin != _smax) {
							_delete_selection(false);
						}
						_insert_text(s, selected, false);
						_baseline = _cb->_get_caret_pos_x(_smax);
					}
					void insert_char(char_t c) {
						_modified = true;
						bool had_selection = _smin != _smax;
						if (had_selection) {
							_delete_selection(false);
						}
						_modification mod(_smin, (
							c == U'\n' ? caret_position(_smin.line + 1, 0) : caret_position(_smin.line, _smin.column + 1)
							), false, false, true);
						if (c == U'\n') {
							mod.content = to_str(_cb->_le);
							++_dy;
							++_ly;
							_dx -= static_cast<int>(_smin.column);
							auto newline = _cb->_ctx->insert_after(
								_lit, text_context::line(_lit->content.substr(_smin.column), _lit->ending_type)
							);
							_lit->content = _lit->content.substr(0, _smin.column);
							_lit->ending_type = _cb->_le;
							_lit = newline;
							++_smin.line;
							_smin.column = 0;
						} else {
							mod.content = str_t({c});
							if (_cb->_insert || had_selection || _smin.column == _lit->content.length()) {
								_lit->content.insert(_smin.column, 1, c);
								++_dx;
							} else {
								_mpk.mods.push_back(_modification(
									_smin, caret_position(_smin.line, _smin.column + 1),
									true, false, false, str_t({_lit->content[_smin.column]})
								));
								_lit->content[_smin.column] = c;
							}
							++_smin.column;
						}
						_smax = _smin;
						_baseline = _cb->_get_caret_pos_x(_smin);
						_mpk.mods.push_back(std::move(mod));
					}
					void delete_char_before() {
						_modified = true;
						if (_smin != _smax) {
							_delete_selection(false);
						} else if (_smin != caret_position(0, 0)) {
							if (_smin.column == 0) {
								assert_true_logical(_dx == 0, "because it should be?");
								--_lit;
								--_ly;
								_smin = caret_position(_smin.line - 1, _lit->content.length());
							} else {
								--_smin.column;
							}
							_minmain = false;
							_delete_selection(true);
						}
					}
					void delete_char_after() {
						_modified = true;
						if (_smin != _smax) {
							_delete_selection(false);
						} else {
							if (_smin.column < _lit->content.length()) {
								++_smax.column;
							} else if (_smin.line + 1 < _cb->_ctx->num_lines()) {
								_smax = caret_position(_smax.line + 1, 0);
							} else {
								return;
							}
							_minmain = true;
							_delete_selection(true);
						}
					}

					void move_to(caret_position p, double baseline) {
						_smin = _smax = p;
						_baseline = baseline;
					}
					void move_to_with_selection(caret_position p, double baseline) {
						(_minmain ? _smin : _smax) = p;
						if (_smax < _smin) {
							std::swap(_smin, _smax);
							_minmain = !_minmain;
						}
						_baseline = baseline;
					}

					std::pair<caret_position, caret_position> current_position() const {
						if (_minmain) {
							return std::make_pair(_smin, _smax);
						}
						return std::make_pair(_smax, _smin);
					}
					double current_baseline() const {
						return _baseline;
					}
					caret_set::entry current_old_position() const {
						return *_cur;
					}

					editor &get_context() const {
						return *_cb;
					}
					const _modpack &get_modification() const {
						return _mpk;
					}
					_modpack &get_modification() {
						return _mpk;
					}

					void start(editor &cb) {
						_prepare_for_modification(cb);
						_cur = _cb->_cset.carets.begin();
						_switch_to_next_caret(std::make_pair(_cur->first.first, _cur->first.second), _cur->second.alignment);
					}
					void start_manual(editor &cb, std::pair<caret_position, caret_position> pos, double baseline = 0.0) {
						_prepare_for_modification(cb);
						_switch_to_next_caret(pos, baseline);
					}
					void next() {
						if (++_cur != _cb->_cset.carets.end()) {
							next_manual(std::make_pair(_cur->first.first, _cur->first.second), true, _cur->second.alignment);
						} else {
							end_manual(true);
						}
					}
					void next_manual(std::pair<caret_position, caret_position> caret, bool app_caret, double baseline = 0.0) {
						next_manual_nofixup(std::make_pair(_fixup_pos(caret.first), _fixup_pos(caret.second)), app_caret, baseline);
					}
					void next_manual_nofixup(std::pair<caret_position, caret_position> caret, bool app_caret, double baseline = 0.0) {
						_end_group(app_caret);
						_switch_to_next_caret(caret, baseline);
					}
					bool ended() {
						return _ended;
					}
					void end_manual(bool app_caret) {
						_end_group(app_caret);
						std::swap(_cb->_cset.carets, _newcs);
						_cb->_make_caret_visible(_cb->_cset.carets.rbegin()->first.first); // TODO move to a better position
						if (_modified) {
							_cb->get_context()->modified.invoke();
						}
						_ended = true;
#ifndef NDEBUG
						_cb->_modifying = false;
#endif
					}
				protected:
					caret_set::container _newcs;
					_modpack _mpk;
					caret_set::container::iterator _cur;
					caret_position _smin, _smax;
					double _baseline;
					text_context::line_iterator _lit;
					editor *_cb = nullptr;
					int _dx = 0, _dy = 0;
					size_t _ly = 0;
					bool _modified = false, _minmain = false, _ended = false;

					caret_position _fixup_pos(caret_position pos) const {
						pos.line = static_cast<size_t>(static_cast<int>(pos.line) + _dy);
						if (pos.line == _ly) {
							pos.column = static_cast<size_t>(static_cast<int>(pos.column) + _dx);
						}
						return pos;
					}

					void _on_minmax_changed() {
						if (_smin > _smax) {
							_minmain = false;
							std::swap(_smin, _smax);
						} else {
							_minmain = true;
						}
						if (_ly != _smin.line) {
							_dx = 0;
							_ly = _smin.line;
							_lit = _cb->_ctx->at(_ly);
						}
					}

					void _prepare_for_modification(editor &cb) {
						_cb = &cb;
						assert_true_usage(!_ended, "a modify iterator cannot be used twice");
#ifndef NDEBUG
						assert(!_cb->_modifying);
						_cb->_modifying = true;
#endif
						if (_cb->_selecting) {
							_cb->_end_selection();
						}
						_lit = _cb->_ctx->begin();
					}
					void _append_current_caret() {
						bool merged;
						if (_minmain) {
							std::swap(_smin, _smax);
						}
						auto it = caret_set::add_caret(_newcs, caret_selection_data(_smax, _smin, _baseline), merged);
						if (merged) {
							it->second.alignment = _cb->_get_caret_pos_x(it->first.first);
						}
					}
					void _switch_to_next_caret(std::pair<caret_position, caret_position> caret, double baseline = 0.0) {
						_smin = caret.first;
						_smax = caret.second;
						_baseline = baseline;
						_on_minmax_changed();
					}
					void _end_group(bool app_caret) {
						if (_mpk.mods.size() > 0) {
							_mpk.mods.back().end_of_group = true;
						}
						if (app_caret) {
							_append_current_caret();
						}
					}

					void _insert_text(const str_t &s, bool selected, bool cfront) {
						bool first = true;
						str_t secondpart = _lit->content.substr(_smin.column);
						line_ending ole = _lit->ending_type;
						_lit->content = _lit->content.substr(0, _smin.column);
						convert_to_lines(s, [&](str_t cline, line_ending le) {
							if (first) {
								_lit->content += cline;
								_lit->ending_type = le;
								first = false;
							} else {
								_lit = _cb->_ctx->insert_after(_lit, text_context::line(std::move(cline), le));
								++_dy;
								++_ly;
							}
						});
						_dx += static_cast<int>(_lit->content.length()) - static_cast<int>(_smin.column);
						_smax.line = _ly;
						_smax.column = _lit->content.length();
						_lit->content += secondpart;
						_lit->ending_type = ole;
						_mpk.mods.push_back(_modification(_smin, _smax, cfront, selected, true, s));
						_minmain = cfront;
						if (!selected) {
							if (_minmain) {
								_smax = _smin;
							} else {
								_smin = _smax;
							}
						}
					}
					void _delete_selection(bool vsel) {
						_modification cmod(_smin, _smax, _minmain, !vsel, false);
						if (_smin.line == _smax.line) {
							cmod.content = _lit->content.substr(_smin.column, _smax.column - _smin.column);
							_lit->content = _lit->content.substr(0, _smin.column) + _lit->content.substr(_smax.column);
							_dx += static_cast<int>(_smin.column) - static_cast<int>(_smax.column);
						} else {
							_dy -= static_cast<int>(_smax.line - _smin.line);
							_dx = static_cast<int>(_smin.column) - static_cast<int>(_smax.column);
							std::basic_ostringstream<char_t> ss;
							ss << _lit->content.substr(_smin.column) << to_str(_lit->ending_type);
							for (; _smin.line + 1 < _smax.line; --_smax.line) {
								auto nl = _lit;
								++nl;
								ss << nl->content << to_str(nl->ending_type);
								_cb->_ctx->erase(nl);
							}
							auto nl = _lit;
							++nl;
							ss << nl->content.substr(0, _smax.column);
							_lit->content = _lit->content.substr(0, _smin.column) + nl->content.substr(_smax.column);
							_lit->ending_type = nl->ending_type;
							_cb->_ctx->erase(nl);
							cmod.content = ss.str();
						}
						_smax = _smin;
						_baseline = _cb->_get_caret_pos_x(*_lit, _smin);
						_mpk.mods.push_back(std::move(cmod));
					}
				};
				inline static std::pair<caret_position, caret_position> _get_undo_caret_pos(const _modification &mod) {
					return std::make_pair(mod.front_pos, mod.addition ? mod.rear_pos : mod.front_pos);
				}
				inline static std::pair<caret_position, caret_position> _get_redo_caret_pos(const _modification &mod) {
					return std::make_pair(mod.front_pos, mod.addition ? mod.front_pos : mod.rear_pos);
				}
				void _redo_mod(const _modpack &jmp) {
					if (jmp.mods.size() == 0) {
						return;
					}
					_modify_iterator it;
					it.start_manual(*this, _get_redo_caret_pos(jmp.mods[0]));
					it.redo_modification(jmp.mods[0]);
					bool lgp = jmp.mods[0].end_of_group;
					for (auto i = jmp.mods.begin() + 1; i != jmp.mods.end(); lgp = i->end_of_group, ++i) {
						it.next_manual_nofixup(_get_redo_caret_pos(*i), lgp);
						it.redo_modification(*i);
					}
					it.end_manual(lgp);
				}
				void _redo_last() {
					_redo_mod(_modhist[_nmodid]);
					++_nmodid;
				}
				void _undo_mod(const _modpack &jmp) {
					if (jmp.mods.size() == 0) {
						return;
					}
					_modify_iterator it;
					it.start_manual(*this, _get_undo_caret_pos(jmp.mods[0]));
					it.undo_modification(jmp.mods[0]);
					bool lgp = jmp.mods[0].end_of_group, llgp = true;
					for (auto i = jmp.mods.begin() + 1; i != jmp.mods.end(); llgp = lgp, lgp = i->end_of_group, ++i) {
						it.next_manual(_get_undo_caret_pos(*i), llgp);
						it.undo_modification(*i);
					}
					it.end_manual(llgp);
				}
				void _undo_last() {
					_undo_mod(_modhist[--_nmodid]);
				}

				std::vector<_modpack> _modhist;
				size_t _nmodid = 0, _modstksz = 0;

				void _on_modify(_modpack mp) {
					if (_nmodid < _modhist.size()) {
						_modhist[_nmodid] = std::move(mp);
					} else {
						_modhist.push_back(std::move(mp));
					}
					_modstksz = ++_nmodid;
				}

				template <bool AppMod> struct _modify_iterator_rsrc_base {
				public:
					explicit _modify_iterator_rsrc_base(editor &cb) {
						_rsrc.start(cb);
					}
					~_modify_iterator_rsrc_base() {
						if (AppMod && _rsrc.get_modification().mods.size() > 0) {
							_rsrc.get_context()._on_modify(std::move(_rsrc.get_modification()));
						}
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
				typedef _modify_iterator_rsrc_base<true> _modify_iterator_rsrc;
				typedef _modify_iterator_rsrc_base<false> _modify_iterator_rsrc_no_append_modification;

				void _make_caret_visible(caret_position cp) {
					codebox *cb = _get_box();
					double fh = get_line_height();
					vec2d np(_get_caret_pos_x(cp), static_cast<double>(cp.line + 1) * fh);
					cb->make_point_visible(np);
					np.y -= fh;
					cb->make_point_visible(np);
				}

				void _on_content_modified() {
					content_modified.invoke();
					display_changed.invoke();
				}
				void _on_content_theme_changed() {
					display_changed.invoke();
				}

				void _begin_selection(caret_position cp, double basel) {
					assert_true_logical(!_selecting, "_begin_selection() called when selecting");
					_selecting = true;
					_cur_sel = caret_selection_data(cp, cp, basel);
				}
				void _end_selection() {
					assert_true_logical(_selecting, "_end_selection() called when not selecting");
					_selecting = false;
					bool merged = false;
					auto it = _cset.add_caret(_cset.carets, _cur_sel, merged);
					if (merged) {
						it->second.alignment = _get_caret_pos_x(it->first.first);
					}
				}

				caret_position _hit_test_for_caret(vec2d pos) {
					caret_position cp;
					cp.line = static_cast<size_t>(std::max(0.0, (pos.y + _get_box()->get_vertical_position()) / get_line_height()));
					if (cp.line >= _ctx->num_lines()) {
						cp.line = _ctx->num_lines() - 1;
					}
					cp.column = _hit_test_for_caret_x(*_ctx->at(cp.line), cp.line, pos.x);
					return cp;
				}
				bool _is_in_selection(caret_position cp) const {
					auto cur = _cset.carets.lower_bound(std::make_pair(cp, cp));
					if (cur != _cset.carets.begin()) {
						--cur;
					}
					while (cur != _cset.carets.end() && std::min(cur->first.first, cur->first.second) <= cp) {
						if (cur->first.first != cur->first.second) {
							auto minmaxv = std::minmax(cur->first.first, cur->first.second);
							if (cp >= minmaxv.first && cp <= minmaxv.second) {
								return true;
							}
						}
						++cur;
					}
					return false;
				}

				std::pair<caret_position, double> _get_left_position(caret_position cp) const {
					text_context::line_iterator lit = _ctx->at(cp.line);
					if (cp.column == 0) {
						if (cp.line > 0) {
							--lit;
							--cp.line;
							assert_true_logical(lit->ending_type != line_ending::none, "invalid line ending encountered");
							cp.column = lit->content.length();
						}
					} else {
						--cp.column;
					}
					return std::make_pair(cp, _get_caret_pos_x(*lit, cp));
				}
				std::pair<caret_position, double> _get_right_position(caret_position cp) const {
					text_context::line_iterator lit = _ctx->at(cp.line);
					if (cp.column == lit->content.length()) {
						if (cp.line + 1 < _ctx->num_lines()) {
							++lit;
							++cp.line;
							cp.column = 0;
						}
					} else {
						++cp.column;
					}
					return std::make_pair(cp, _get_caret_pos_x(*lit, cp));
				}
				caret_position _get_up_position(caret_position cp, double bl) const {
					if (cp.line == 0) {
						return cp;
					}
					--cp.line;
					cp.column = _hit_test_for_caret_x(*_ctx->at(cp.line), cp.line, bl);
					return cp;
				}
				caret_position _get_down_position(caret_position cp, double bl) const {
					if (cp.line + 1 == _ctx->num_lines()) {
						return cp;
					}
					++cp.line;
					cp.column = _hit_test_for_caret_x(*_ctx->at(cp.line), cp.line, bl);
					return cp;
				}

				template <typename GetPos, typename GetTg> void _on_key_down_lr(GetPos gp, GetTg gt) {
					if (os::input::is_key_down(os::input::key::shift)) {
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							auto newp = (this->*gp)(it->current_position().first);
							it->move_to_with_selection(newp.first, newp.second);
						}
					} else {
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							const auto &curp = it->current_position();
							if (curp.first == curp.second) {
								auto newp = (this->*gp)(curp.first);
								it->move_to(newp.first, newp.second);
							} else {
								caret_position newp = gt(curp.first, curp.second);
								it->move_to(newp, _get_caret_pos_x(newp));
							}
						}
					}
					invalidate_visual();
				}
				template <typename Comp, typename GetPos> void _on_key_down_ud(GetPos gp) {
					if (os::input::is_key_down(os::input::key::shift)) {
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							double bl = it->current_baseline();
							it->move_to_with_selection((this->*gp)(it->current_position().first, bl), bl);
						}
					} else {
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							const auto &curp = it->current_position();
							double bl = it->current_baseline();
							caret_position newop = curp.first;
							if (Comp()(curp.first, curp.second)) {
								newop = curp.second;
								bl = _get_caret_pos_x(newop);
							}
							it->move_to((this->*gp)(newop, bl), bl);
						}
					}
					invalidate_visual();
				}

				void _on_selecting_mouse_move(vec2d pos) {
					vec2d
						rtextpos = pos - get_client_region().xmin_ymin(), clampedpos = rtextpos,
						relempos = pos - get_layout().xmin_ymin();
					if (relempos.y < 0.0) {
						clampedpos.y = 0.0;
						_scrolldiff = relempos.y;
						ui::manager::get().schedule_update(*this);
					} else {
						double h = get_layout().height();
						if (relempos.y > h) {
							clampedpos.y = h;
							_scrolldiff = relempos.y - get_layout().height();
							ui::manager::get().schedule_update(*this);
						}
					}
					_mouse_cache = _hit_test_for_caret(clampedpos);
					if (_selecting) {
						if (_mouse_cache != _cur_sel.caret) {
							_cur_sel.caret = _mouse_cache;
							_cur_sel.alignment = _get_caret_pos_x(_cur_sel.caret);
							invalidate_visual();
						}
					}
				}

				void _on_mouse_move(ui::mouse_move_info &info) override {
					_on_selecting_mouse_move(info.new_pos);
					if (_predrag) {
						if ((info.new_pos - _predrag_pos).length_sqr() > dragdrop_distance * dragdrop_distance) {
							_predrag = false;
							logger::get().log_info(CP_HERE, "starting drag & drop of text");
							// TODO start drag drop
						}
					}
					element::_on_mouse_move(info);
				}
				void _on_mouse_down(ui::mouse_button_info &info) override {
					element::_on_mouse_down(info);
					if (info.button == os::input::mouse_button::left) {
						_mouse_cache = _hit_test_for_caret(info.position - get_client_region().xmin_ymin());
						if (!_is_in_selection(_mouse_cache)) {
							if (!os::input::is_key_down(os::input::key::control)) {
								_cset.carets.clear();
							}
							_begin_selection(_mouse_cache, _get_caret_pos_x(_mouse_cache));
							invalidate_visual();
						} else {
							_predrag_pos = info.position;
							_predrag = true;
						}
						get_window()->set_mouse_capture(*this);
					} else if (info.button == os::input::mouse_button::middle) {
						// TODO block selection
					}
				}
				void _on_mouse_lbutton_up() {
					if (_selecting) {
						_end_selection();
						invalidate_visual();
					} else if (_predrag) {
						_predrag = false;
						caret_position hitp = _hit_test_for_caret(_predrag_pos - get_client_region().xmin_ymin());
						_cset.carets.clear();
						_cset.carets.insert(std::make_pair(std::make_pair(hitp, hitp), caret_data(_get_caret_pos_x(hitp))));
						invalidate_visual();
					} else {
						return;
					}
					get_window()->release_mouse_capture();
				}
				void _on_capture_lost() override {
					_on_mouse_lbutton_up();
				}
				void _on_mouse_up(ui::mouse_button_info &info) override {
					if (info.button == os::input::mouse_button::left) {
						_on_mouse_lbutton_up();
					}
				}
				void _on_key_down(ui::key_info &info) override {
					switch (info.key) {
						// deleting stuff
					case os::input::key::backspace:
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							it->delete_char_before();
						}
						break;
					case os::input::key::del:
						for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
							it->delete_char_after();
						}
						break;

						// movement control
					case os::input::key::left:
						_on_key_down_lr(&editor::_get_left_position, static_cast<
							const caret_position&(*)(const caret_position&, const caret_position&)
						>(std::min));
						break;
					case os::input::key::right:
						_on_key_down_lr(&editor::_get_right_position, static_cast<
							const caret_position&(*)(const caret_position&, const caret_position&)
						>(std::max));
						break;
					case os::input::key::up:
						_on_key_down_ud<std::greater<caret_position>>(&editor::_get_up_position);
						break;
					case os::input::key::down:
						_on_key_down_ud<std::less<caret_position>>(&editor::_get_down_position);
						break;
					case os::input::key::home:
						{
							auto fptr =
								os::input::is_key_down(os::input::key::shift) ?
								&_modify_iterator::move_to_with_selection :
								&_modify_iterator::move_to;
							for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
								caret_position cp = it->current_position().first;
								auto lit = _ctx->at(cp.line);
								size_t i = 0;
								while (i < lit->content.length() && (lit->content[i] == U' ' || lit->content[i] == U'\t')) {
									++i;
								}
								if (cp.column == i) {
									cp.column = 0;
									((*it).*fptr)(cp, 0.0);
								} else {
									cp.column = i;
									((*it).*fptr)(cp, _get_caret_pos_x(*lit, cp));
								}
							}
						}
						break;
					case os::input::key::end:
						{
							auto fptr =
								os::input::is_key_down(os::input::key::shift) ?
								&_modify_iterator::move_to_with_selection :
								&_modify_iterator::move_to;
							for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
								caret_position cp = it->current_position().first;
								auto lit = _ctx->at(cp.line);
								cp.column = lit->content.length();
								((*it).*fptr)(cp, std::numeric_limits<double>::infinity());
							}
						}
						break;
					case os::input::key::page_up:
						// TODO page_up
						break;
					case os::input::key::page_down:
						// TODO page_down
						break;
					case os::input::key::escape:
						{
							for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
								it->move_to(it->current_position().first, it->current_baseline());
							}
						}
						break;

						// edit mode toggle
					case os::input::key::insert:
						_insert = !_insert;
						invalidate_visual();
						break;

					default:
						break;
					}
					element::_on_key_down(info);
				}
				void _on_keyboard_text(ui::text_info &info) override {
					for (_modify_iterator_rsrc it(*this); !it->ended(); it->next()) {
						it->insert_char(info.character);
					}
				}
				void _on_update() override {
					if (_selecting) {
						codebox *editor = _get_box();
						editor->set_vertical_position(
							editor->get_vertical_position() +
							move_speed_scale * _scrolldiff * ui::manager::get().delta_time().count()
						);
						_on_selecting_mouse_move(get_window()->screen_to_client(os::input::get_mouse_position()).convert<double>());
					}
				}

				void _render() const override;

				void _initialize() override {
					component::_initialize();
					set_padding(ui::thickness(2.0, 0.0, 0.0, 0.0));
				}
				void _dispose() override {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
					}
					component::_dispose();
				}

			public:
				struct caret_renderer {
				public:
					caret_renderer(
						const caret_set::container &c, size_t sl, size_t pel, double lh,
						const ui::basic_brush *b, const ui::basic_pen *p
					) : _lh(lh), _brush(b), _pen(p) {
						caret_position sp(sl, 0), pep(pel, 0);
						_beg = c.lower_bound(std::make_pair(sp, caret_position()));
						if (_beg != c.begin()) {
							auto prev = _beg;
							--prev;
							if (prev->first.second >= sp) {
								_beg = prev;
							}
						}
						_end = c.upper_bound(std::make_pair(pep, pep));
						if (_end != c.end()) {
							if (_end->first.second < pep) {
								++_end;
							}
						}
					}

					void switching_char(character_rendering_iterator&);
					void switching_line(character_rendering_iterator&);
				protected:
					editor::caret_set::const_iterator _beg, _end;
					std::pair<caret_position, caret_position> _minmax;
					double _lastl = 0.0, _lh = 0.0;
					bool _rendering = false;
					caret_position _cpos;
					const ui::basic_brush *_brush = nullptr;
					const ui::basic_pen *_pen = nullptr;

					void _check_next(double cl, double cy, caret_position cp) {
						if (_beg != _end) {
							_minmax = std::minmax({_beg->first.first, _beg->first.second});
							if (cp >= _minmax.first) {
								_cpos = _beg->first.first;
								++_beg;
								if (_minmax.first != _minmax.second) {
									_rendering = true;
									_lastl = cl;
								}
							}
						}
					}
				};
			};

			struct character_rendering_iterator {
			public:
				character_rendering_iterator() = delete;
				character_rendering_iterator(const text_context &ctx, double lh, double tw, size_t sl, size_t pel) :
					_line_it(ctx.at(sl)),
					_theme_it(ctx.get_text_theme().get_iter_at(caret_position(sl, 0))),
					_char_it(_line_it->content, editor::get_font(), tw),
					_ctx(&ctx), _cur_line(sl), _tg_line(pel), _line_h(lh), _tab_w(tw) {
				}
				character_rendering_iterator(const editor &ce, size_t sl, size_t pel) :
					character_rendering_iterator(*ce.get_context(), ce.get_line_height(), ce.get_tab_width(), sl, pel) {
				}

				bool is_line_break() const {
					return _char_it.ended();
				}
				bool ended() const {
					return _cur_line >= _tg_line || _line_it == _ctx->end();
				}
				void begin() {
					_char_it.begin(_theme_it.current_theme.style);
				}
				void next_char() {
					assert_true_usage(!ended(), "incrementing an invalid rendering iterator");
					if (_char_it.ended()) {
						switching_line.invoke();
						++_cur_line;
						++_line_it;
						_cury += _line_h;
						_rcy = static_cast<int>(std::round(_cury));
						_cur_col = 0;
						_ctx->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, 0));
						if (!ended()) {
							_char_it = ui::line_character_iterator(_line_it->content, editor::get_font(), _tab_w);
							_char_it.begin(_theme_it.current_theme.style);
						}
					} else {
						switching_char.invoke();
						++_cur_col;
						_ctx->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, _cur_col));
						_char_it.next(_theme_it.current_theme.style);
					}
				}

				const ui::line_character_iterator &character_info() const {
					return _char_it;
				}
				const text_theme_data::char_iterator &theme_info() const {
					return _theme_it;
				}
				size_t current_line() const {
					return _cur_line;
				}
				size_t current_column() const {
					return _cur_col;
				}
				caret_position current_position() const {
					return caret_position(_cur_line, _cur_col);
				}
				double y_offset() const {
					return _cury;
				}
				int rounded_y_offset() const {
					return _rcy;
				}
				double line_height() const {
					return _line_h;
				}

				event<void> switching_line, switching_char;
			protected:
				text_context::const_line_iterator _line_it;
				text_theme_data::char_iterator _theme_it;
				ui::line_character_iterator _char_it;
				const text_context *_ctx = nullptr;
				size_t _cur_line = 0, _cur_col = 0, _tg_line = 0;
				double _cury = 0.0, _line_h = 0.0, _tab_w;
				int _rcy = 0;
			};

			struct rendering_iterator {
			public:
				rendering_iterator(
					const editor &ce, const editor::caret_set::container &c,
					size_t sl, size_t pel, const ui::basic_brush *b, const ui::basic_pen *p
				) : _citer(ce, sl, pel), _caret(c, sl, pel, ce.get_line_height(), b, p) {
					_citer.switching_line += std::bind(&rendering_iterator::_on_switching_line, this);
					_citer.switching_char += std::bind(&rendering_iterator::_on_switching_char, this);
					_citer.begin();
				}

				bool ended() {
					return _citer.ended();
				}
				void next() {
					_citer.next_char();
				}

				const character_rendering_iterator &char_iter() const {
					return _citer;
				}
			protected:
				character_rendering_iterator _citer;
				editor::caret_renderer _caret;

				void _on_switching_line() {
					_caret.switching_line(_citer);
				}
				void _on_switching_char() {
					_caret.switching_char(_citer);
				}
			};

			inline void editor::caret_renderer::switching_char(character_rendering_iterator &it) {
				double cx = it.character_info().char_left(), cy = it.y_offset();
				if (_rendering) {
					if (_minmax.second <= it.current_position()) {
						_brush->fill_rect(rectd(_lastl, cx, cy, cy + it.line_height()));
						_rendering = false;
					}
				}
				if (!_rendering) {
					_check_next(cx, cy, it.current_position());
				}
				if (it.current_position() == _cpos) {
					_pen->draw_lines({vec2d(cx, cy), vec2d(cx, cy + it.line_height())});
				}
			}
			inline void editor::caret_renderer::switching_line(character_rendering_iterator &it) {
				switching_char(it);
				if (_rendering) {
					double finx =
						it.character_info().char_left() +
						it.character_info().get_font_family().get_by_style(
							it.theme_info().current_theme.style
						)->get_char_entry(U' ').advance;
					_brush->fill_rect(rectd(_lastl, finx, it.y_offset(), it.y_offset() + it.line_height()));
					_lastl = 0.0;
				}
			}

			inline editor *component::_get_editor() const {
				return _get_box()->get_editor();
			}

			inline void editor::_render() const {
#ifndef NDEBUG
				assert(!_modifying);
#endif
				if (get_client_region().height() < 0.0) {
					return;
				}
				double lh = get_line_height();
				std::pair<size_t, size_t> be = get_visible_lines();

				// render text
				os::renderer_base::get().push_matrix(matd3x3::translate(vec2d(
					std::round(get_client_region().xmin),
					std::round(get_client_region().ymin - _get_box()->get_vertical_position() + static_cast<double>(be.first) * lh)
				)));
				caret_set::container ncon;
				const caret_set::container *used = &_cset.carets;
				if (_selecting) {
					ncon = *used;
					used = &ncon;
					caret_set::add_caret(ncon, _cur_sel);
				}
				ui::font_family::baseline_info bi = _font.get_baseline_info();
				for (rendering_iterator it(*this, *used, be.first, be.second, _selbrush, _caretpen); !it.ended(); it.next()) {
					const character_rendering_iterator &cri = it.char_iter();
					if (!cri.is_line_break()) {
						const ui::line_character_iterator &lci = cri.character_info();
						const text_theme_data::char_iterator &ti = cri.theme_info();
						if (is_graphical_char(lci.current_char())) {
							os::renderer_base::get().draw_character(
								lci.current_char_entry().texture,
								vec2d(lci.char_left(), std::round(cri.rounded_y_offset() + bi.get(ti.current_theme.style))) +
								lci.current_char_entry().placement.xmin_ymin(),
								ti.current_theme.color
							);
						}
					}
				}
				os::renderer_base::get().pop_matrix();
			}

			inline void codebox::_initialize() {
				panel_base::_initialize();

				_vscroll = element::create<ui::scroll_bar>();
				_vscroll->set_anchor(ui::anchor::dock_right);
				_vscroll->value_changed += [this](value_update_info<double> &info) {
					vertical_viewport_changed.invoke(info);
					invalidate_visual();
				};
				_children.add(*_vscroll);

				_editor = element::create<editor>();
				_editor->content_modified += std::bind(&codebox::_on_content_modified, this);
				_children.add(*_editor);
			}
			inline void codebox::_reset_scrollbars() {
				_vscroll->set_params(_editor->get_vertical_scroll_range(), get_layout().height());
			}
			inline void codebox::_on_mouse_scroll(ui::mouse_scroll_info &p) {
				_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.delta);
				p.mark_handled();
			}
			inline void codebox::_finish_layout() {
				rectd lo = get_client_region();
				_child_recalc_layout(_vscroll, lo);

				double lpos = lo.xmin;
				for (auto i = _lcs.begin(); i != _lcs.end(); ++i) {
					double cw = (*i)->get_desired_size().x;
					ui::thickness mg = (*i)->get_margin();
					lpos += mg.left;
					_child_set_layout(*i, rectd(lpos, lpos + cw, lo.ymin, lo.ymax));
					lpos += cw + mg.right;
				}

				double rpos = _vscroll->get_layout().xmin;
				for (auto i = _rcs.rbegin(); i != _rcs.rend(); ++i) {
					double cw = (*i)->get_desired_size().x;
					ui::thickness mg = (*i)->get_margin();
					rpos -= mg.right;
					_child_set_layout(*i, rectd(rpos - cw, rpos, lo.ymin, lo.ymax));
					rpos -= cw - mg.left;
				}

				ui::thickness emg = _editor->get_margin();
				_child_set_layout(_editor, rectd(lpos + emg.left, rpos - emg.right, lo.ymin, lo.ymax));

				_reset_scrollbars();
				panel_base::_finish_layout();
			}
		}
	}
}
