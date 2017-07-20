#pragma once

#include "context.h"

namespace codepad {
	namespace editor {
		class codebox_editor : public codebox_component {
		public:
			constexpr static double
				move_speed_scale = 15.0,
				dragdrop_distance = 5.0;

			void set_context(text_context *nctx) {
				if (_ctx) {
					_ctx->modified -= _mod_tok;
					_ctx->theme_changed -= _tc_tok;
				}
				_ctx = nctx;
				if (_ctx) {
					_mod_tok = (_ctx->modified += std::bind(&codebox_editor::_on_content_modified, this));
					_tc_tok = (_ctx->theme_changed += std::bind(&codebox_editor::_on_content_theme_changed, this));
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
				CP_INFO("\\r ", n[0], ", \\n ", n[1], ", \\r\\n ", n[2], ", selected ", static_cast<int>(_le));
			}
			void set_line_ending(line_ending l) {
				assert_true_usgerr(l != line_ending::none, "invalid line ending");
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

			bool can_undo() {
				return _nmodid > 0;
			}
			void undo() {
				assert_true_usgerr(can_undo(), "cannot undo");
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
				assert_true_usgerr(can_redo(), "cannot redo");
				_redo_last();
			}
			bool try_redo() {
				if (can_redo()) {
					_redo_last();
					return true;
				}
				return false;
			}

			struct character_rendering_iterator {
				friend class codebox_editor;
			public:
				character_rendering_iterator() = delete;

				bool next_char() {
					if (_cur_line >= _tg_line || _line_it == _editor->get_context()->end()) {
						return false;
					}
					if (!_char_it.next(_theme_it.current_theme.style)) {
						do {
							++_cur_line;
							++_line_it;
							if (_cur_line >= _tg_line || _line_it == _editor->get_context()->end()) {
								return false;
							}
							_cury += _c_lh;
							_rcy = static_cast<int>(std::round(_cury));
							_cur_col = 0;
							_editor->get_context()->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, 0));
							_char_it = ui::line_character_iterator(_line_it->content, codebox_editor::get_font(), _editor->get_tab_width());
						} while (!_char_it.next(_theme_it.current_theme.style));
					} else {
						++_cur_col;
						_editor->get_context()->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, _cur_col));
					}
					return true;
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
				double y_offset() const {
					return _cury;
				}
				int rounded_y_offset() const {
					return _rcy;
				}
			protected:
				character_rendering_iterator(const codebox_editor &ce, size_t sl, size_t pel) :
					_line_it(ce.get_context()->at(sl)),
					_theme_it(ce.get_context()->get_text_theme().get_iter_at(caret_position(sl, 0))),
					_char_it(_line_it->content, codebox_editor::get_font(), ce.get_tab_width()),
					_editor(&ce), _cur_line(sl), _tg_line(pel), _c_lh(ce.get_line_height()) {
				}

				text_context::line_iterator _line_it;
				text_theme_data::char_iterator _theme_it;
				ui::line_character_iterator _char_it;
				const codebox_editor *_editor = nullptr;
				size_t _cur_line = 0, _cur_col = 0, _tg_line = 0;
				double _cury = 0.0, _c_lh = 0.0;
				int _rcy = 0;
			};
			character_rendering_iterator start_rendering(size_t s, size_t e) const {
				return character_rendering_iterator(*this, s, e);
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

				std::multimap<caret_position, _caret_range>::iterator add(std::pair<caret_position, _caret_range> p) {
					return add_caret(carets, p);
				}
				std::multimap<caret_position, _caret_range>::iterator add(std::pair<caret_position, _caret_range> p, bool &merged) {
					return add_caret(carets, p, merged);
				}

				inline static std::multimap<caret_position, _caret_range>::iterator add_caret(
					std::multimap<caret_position, _caret_range> &mp,
					std::pair<caret_position, _caret_range> c
				) {
					bool dummy;
					return add_caret(mp, c, dummy);
				}
				inline static std::multimap<caret_position, _caret_range>::iterator add_caret(
					std::multimap<caret_position, _caret_range> &mp,
					std::pair<caret_position, _caret_range> c,
					bool &merged
				) {
					merged = false;
					auto minmaxv = std::minmax({c.first, c.second.selection_end});
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

			text_context *_ctx = nullptr;
			event<void>::token _mod_tok, _tc_tok;
			// settings
			double _tab_w = 4.0;
			line_ending _le;
			// current state
			double _scrolldiff = 0.0;
			_caret_set _cset;
			std::pair<caret_position, _caret_range> _cur_sel;
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
				for (size_t i = 0; it.next(ci.current_theme.style); _ctx->get_text_theme().incr_iter(ci, caret_position(line_num, ++i))) {
					if (pos < (it.char_left() + it.next_char_left()) * 0.5) {
						return i;
					}
				}
				return ln.content.length();
			}
			double _get_caret_pos_x(const text_context::line &ln, caret_position pos) const {
				ui::line_character_iterator it(ln.content, _font, _tab_w);
				text_theme_data::char_iterator ci = _ctx->get_text_theme().get_iter_at(caret_position(pos.line, 0));
				for (
					size_t i = 0;
					i < pos.column;
					it.next(ci.current_theme.style), _ctx->get_text_theme().incr_iter(ci, caret_position(pos.line, ++i))
					) {
				}
				return it.next_char_left();
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
				std::pair<caret_position, _caret_range> current_old_position() const {
					return *_cur;
				}

				codebox_editor &get_context() const {
					return *_cb;
				}
				const _modpack &get_modification() const {
					return _mpk;
				}
				_modpack &get_modification() {
					return _mpk;
				}

				void start(codebox_editor &cb) {
					_prepare_for_modification(cb);
					_cur = _cb->_cset.carets.begin();
					_switch_to_next_caret(std::make_pair(_cur->first, _cur->second.selection_end), _cur->second.baseline);
				}
				void start_manual(codebox_editor &cb, std::pair<caret_position, caret_position> pos, double baseline = 0.0) {
					_prepare_for_modification(cb);
					_switch_to_next_caret(pos, baseline);
				}
				void next() {
					if (++_cur != _cb->_cset.carets.end()) {
						next_manual(std::make_pair(_cur->first, _cur->second.selection_end), true, _cur->second.baseline);
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
					_cb->_rebuild_selection_cache();
					_cb->_make_caret_visible(_cb->_cset.carets.rbegin()->first); // TODO move to a better position
					if (_modified) {
						_cb->get_context()->modified.invoke();
					}
					_ended = true;
#ifndef NDEBUG
					_cb->_modifying = false;
#endif
				}
			protected:
				std::multimap<caret_position, _caret_range> _newcs;
				_modpack _mpk;
				std::map<caret_position, _caret_range>::iterator _cur;
				caret_position _smin, _smax;
				double _baseline;
				text_context::line_iterator _lit;
				codebox_editor *_cb = nullptr;
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

				void _prepare_for_modification(codebox_editor &cb) {
					_cb = &cb;
					assert_true_usgerr(!_ended, "a modify iterator cannot be used twice");
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
					auto it = _caret_set::add_caret(_newcs, std::make_pair(_smax, _caret_range(_smin, _baseline)), merged);
					if (merged) {
						it->second.baseline = _cb->_get_caret_pos_x(it->first);
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
				_modify_iterator_rsrc_base(codebox_editor &cb) {
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
				assert(!_selecting);
				_selecting = true;
				_cur_sel = std::make_pair(cp, _caret_range(cp, basel));
			}
			void _end_selection() {
				assert(_selecting);
				_selecting = false;
				bool merged = false;
				auto it = _cset.add_caret(_cset.carets, _cur_sel, merged);
				if (merged) {
					it->second.baseline = _get_caret_pos_x(it->first);
				}
				it->second.selection_cache.clear();
				_make_selection_cache_of(it, get_line_height());
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
					if (_mouse_cache != _cur_sel.first) {
						_cur_sel.first = _mouse_cache;
						_cur_sel.second.baseline = _get_caret_pos_x(_cur_sel.first);
						_cur_sel.second.selection_cache.clear();
						_make_selection_cache_of(&_cur_sel, get_line_height());
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
			void _on_mouse_down(ui::mouse_button_info &info) override {
				element::_on_mouse_down(info);
				if (info.button == os::input::mouse_button::left) {
					_mouse_cache = _hit_test_for_caret(info.position - get_client_region().xmin_ymin());
					if (!_is_in_selection(_mouse_cache)) {
						if (!os::input::is_key_down(os::input::key::control)) {
							_cset.carets.clear();
						}
						_begin_selection(_mouse_cache, _get_caret_pos_x(_mouse_cache));
						_make_selection_cache_of(&_cur_sel, get_line_height());
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
					_cset.carets.insert(std::make_pair(hitp, _caret_range(hitp, _get_caret_pos_x(hitp))));
					_rebuild_selection_cache();
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
					_on_key_down_lr(&codebox_editor::_get_left_position, static_cast<
						const caret_position&(*)(const caret_position&, const caret_position&)
					>(std::min));
					break;
				case os::input::key::right:
					_on_key_down_lr(&codebox_editor::_get_right_position, static_cast<
						const caret_position&(*)(const caret_position&, const caret_position&)
					>(std::max));
					break;
				case os::input::key::up:
					_on_key_down_ud<std::greater<caret_position>>(&codebox_editor::_get_up_position);
					break;
				case os::input::key::down:
					_on_key_down_ud<std::less<caret_position>>(&codebox_editor::_get_down_position);
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

			template <typename T> void _make_selection_cache_of(T it, double h) const {
				assert_true_logical(it->second.selection_cache.size() == 0, "cache has already been built");
				it->second.pos_cache = _get_caret_pos_x(it->first);
				if (it->first != it->second.selection_end) {
					double begp = it->second.pos_cache, endp = _get_caret_pos_x(it->second.selection_end);
					caret_position begcp = it->first, endcp = it->second.selection_end;
					if (begcp > endcp) {
						std::swap(begp, endp);
						std::swap(begcp, endcp);
					}
					double y = static_cast<double>(begcp.line) * h;
					if (begcp.line == endcp.line) {
						it->second.selection_cache.push_back(rectd(begp, endp, y, y + h));
					} else {
						auto lit = _ctx->at(begcp.line);
						double
							end = _get_caret_pos_x(caret_position(begcp.line, lit->content.length())),
							sadv = _font.normal->get_char_entry(U' ').advance;
						if (lit->ending_type != line_ending::none) {
							end += sadv;
						}
						it->second.selection_cache.push_back(rectd(begp, end, y, y + h));
						size_t i = begcp.line + 1;
						for (++lit, y += h; i < endcp.line; ++i, ++lit, y += h) {
							end = _get_caret_pos_x(caret_position(i, lit->content.length()));
							if (lit->ending_type != line_ending::none) {
								end += sadv;
							}
							it->second.selection_cache.push_back(rectd(0.0, end, y, y + h));
						}
						it->second.selection_cache.push_back(rectd(0.0, endp, y, y + h));
					}
				}
			}
			void _rebuild_selection_cache() {
				double h = get_line_height();
				for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
					_make_selection_cache_of(i, h);
				}
				invalidate_visual();
			}
			void _draw_caret_and_selection(const std::pair<caret_position, _caret_range> &sp, std::vector<vec2d> &ls, double h) const {
				double
					pos = _get_box()->get_vertical_position(),
					x = get_client_region().xmin + sp.second.pos_cache,
					y = get_client_region().ymin - pos + static_cast<double>(sp.first.line) * h;
				if (_insert) {
					ls.push_back(vec2d(x, y));
					ls.push_back(vec2d(x, y + h));
				} else {
					auto lit = _ctx->at(sp.first.line);
					double cw =
						sp.first.column < lit->content.length() ?
						_get_caret_pos_x(*lit, caret_position(sp.first.line, sp.first.column + 1)) :
						sp.second.pos_cache + _font.normal->get_char_entry(U'\n').advance;
					double yv = y + h;
					ls.push_back(vec2d(x, yv));
					ls.push_back(vec2d(get_client_region().xmin + cw, yv));
				}
				if (sp.first != sp.second.selection_end) {
					vec2d pdiff(get_client_region().xmin, get_client_region().ymin - pos);
					for (auto i = sp.second.selection_cache.begin(); i != sp.second.selection_cache.end(); ++i) {
						_selbrush->fill_rect(i->translated(pdiff));
					}
				}
			}
			void _render() const override { // TODO baseline alignment for different fonts
#ifndef NDEBUG
				assert(!_modifying);
#endif
				if (get_client_region().height() < 0.0) {
					return;
				}
				double lh = get_line_height();
				std::pair<size_t, size_t> be = get_visible_lines();

				// render text
				int
					sx = static_cast<int>(std::round(get_client_region().xmin)),
					sy = static_cast<int>(std::round(
						get_client_region().ymin - _get_box()->get_vertical_position() + static_cast<double>(be.first) * lh
					));
				ui::font_family::baseline_info bi = _font.get_baseline_info();
				for (character_rendering_iterator it = start_rendering(be.first, be.second); it.next_char(); ) {
					const ui::line_character_iterator &ci = it.character_info();
					const text_theme_data::char_iterator &ti = it.theme_info();
					if (is_graphical_char(ci.current_char())) {
						os::renderer_base::get().draw_character(
							ci.current_char_entry().texture,
							vec2d(sx + ci.char_left(), (sy + it.rounded_y_offset()) + bi.get(ti.current_theme.style)) +
							ci.current_char_entry().placement.xmin_ymin(),
							ti.current_theme.color
						);
					}
				}

				// render carets
				std::vector<vec2d> cls;
				std::multimap<caret_position, _caret_range> val;
				const std::multimap<caret_position, _caret_range> *csetptr = &_cset.carets;
				if (_selecting) {
					val = _cset.carets;
					auto it = _caret_set::add_caret(val, _cur_sel);
					it->second.selection_cache.clear();
					_make_selection_cache_of(it, lh);
					csetptr = &val;
				}
				auto
					caret_beg = csetptr->lower_bound(caret_position(be.first, 0)),
					caret_end = csetptr->lower_bound(caret_position(be.second, 0));
				if (caret_beg != csetptr->begin()) {
					auto prev = caret_beg;
					--prev;
					if (prev->second.selection_end.line >= be.first) {
						caret_beg = prev;
					}
				}
				if (caret_end != csetptr->end() && caret_end->second.selection_end.line < be.second) {
					++caret_end;
				}
				for (auto i = caret_beg; i != caret_end; ++i) {
					_draw_caret_and_selection(*i, cls, lh);
				}
				_caretpen->draw_lines(cls);
			}

			void _initialize() override {
				codebox_component::_initialize();
				set_padding(ui::thickness(2.0, 0.0, 0.0, 0.0));
			}
			void _dispose() override {
				if (_ctx) {
					_ctx->modified -= _mod_tok;
				}
				codebox_component::_dispose();
			}
		};

		inline codebox_editor *codebox_component::_get_editor() const {
			return _get_box()->get_editor();
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

			_editor = element::create<codebox_editor>();
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