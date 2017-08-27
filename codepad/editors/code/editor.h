#pragma once

#include "context.h"

namespace codepad {
	namespace editor {
		namespace code {
			struct character_rendering_iterator;
			template <bool SkipFolded, typename ...AddOns> struct rendering_iterator;

			struct switch_char_info {
				switch_char_info() = default;
				switch_char_info(bool jmp, caret_position next) : is_jump(jmp), next_position(next) {
				}
				const bool is_jump = false;
				const caret_position next_position{};
			};

			// TODO syntax highlighting, drag & drop, code folding, etc.
			class editor : public ui::panel_base {
				friend struct codepad::globals;
				friend class codebox;
			public:
				constexpr static double
					move_speed_scale = 15.0,
					dragdrop_distance = 5.0;

				typedef std::pair<caret_position, caret_position> fold_region;
				typedef std::set<fold_region> folding_info;

				void set_context(text_context *nctx) {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
					}
					_ctx = nctx;
					_cset.reset();
					if (_ctx) {
						_mod_tok = (_ctx->modified += std::bind(&editor::_on_content_modified, this, std::placeholders::_1));
						_tc_tok = (_ctx->visual_changed += std::bind(&editor::_on_content_visual_changed, this));
					}
					_on_context_switch();
				}
				text_context *get_context() const {
					return _ctx;
				}

				size_t get_num_lines_with_folding() const {
					size_t l = _ctx->num_lines();
					for (auto i = _fold.begin(); i != _fold.end(); ++i) {
						l -= i->second.line - i->first.line;
					}
					return l;
				}
				double get_line_height() const {
					return _get_font().maximum_height();
				}
				double get_scroll_delta() const {
					return get_line_height() * _lines_per_scroll;
				}
				double get_vertical_scroll_range() const {
					return get_line_height() * static_cast<double>(get_num_lines_with_folding() - 1) + get_client_region().height();
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
				void set_carets(caret_set cs) {
					assert_true_usage(cs.carets.size() > 0, "must have at least one caret");
					for (auto i = cs.carets.begin(); i != cs.carets.end(); ++i) {
						i->second.alignment = get_horizontal_caret_position(i->first.first);
					}
					set_caret_keepalign(std::move(cs));
				}
				void set_caret_keepalign(caret_set cs) {
					assert_true_logical(cs.carets.size() > 0, "must have at least one caret");
					_cset = std::move(cs);
					_make_caret_visible(_cset.carets.rbegin()->first.first); // TODO move to a better position
					_on_carets_changed();
				}
				template <typename GetData> void move_carets_raw(const GetData &gd) {
					caret_set ncs;
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						ncs.add(gd(*i));
					}
					set_caret_keepalign(ncs);
				}
				template <typename GetPos, typename SelPos> void move_carets_special_selection(
					const GetPos &gp, const SelPos &sp
				) {
					if (os::input::is_key_down(os::input::key::shift)) {
						move_carets_raw([this, &gp](const caret_set::entry &et) {
							return _complete_caret_entry(gp(et), et.first.second);
						});
					} else {
						move_carets_raw([this, &gp, &sp](const caret_set::entry &et) {
							if (et.first.first == et.first.second) {
								auto x = gp(et);
								return _complete_caret_entry(x, x);
							} else {
								auto x = sp(et);
								return _complete_caret_entry(x, x);
							}
						});
					}
				}
				template <typename GetPos> void move_carets(const GetPos &gp) {
					return move_carets_special_selection(gp, gp);
				}

				bool is_insert_mode() const {
					return _insert;
				}
				void set_insert_mode(bool v) {
					_insert = v;
					invalidate_layout();
				}
				void toggle_insert_mode() {
					set_insert_mode(!_insert);
				}

				void undo() {
					set_carets(_ctx->undo(this));
				}
				void redo() {
					set_carets(_ctx->redo(this));
				}
				bool try_undo() {
					if (_ctx->can_undo()) {
						undo();
						return true;
					}
					return false;
				}
				bool try_redo() {
					if (_ctx->can_redo()) {
						redo();
						return true;
					}
					return false;
				}

				const folding_info &get_folding_info() const {
					return _fold;
				}
				// TODO fix carets
				template <typename C> void add_fold_region(fold_region fr, const C &callback) {
					assert_true_usage(fr.first < fr.second, "invalid fold region");
					auto
						beg = _fold.lower_bound(fold_region(fr.first, fr.first)),
						end = _fold.lower_bound(fold_region(fr.second, fr.second));
#ifdef CP_DETECT_USAGE_ERRORS
					if (end != _fold.begin()) {
						auto check = end;
						--check;
						assert_true_usage(check->second <= fr.second, "cannot have partially overlapping fold regions");
						if (beg != _fold.begin()) {
							check = beg;
							--check;
							assert_true_usage(check->second <= fr.first, "cannot have partially overlapping fold regions");
						}
					}
#endif
					for (auto i = beg; i != end; i = _fold.erase(i)) {
						callback(*i);
					}
					_fold.emplace_hint(end, fr);
					_on_folding_changed();
				}
				std::vector<fold_region> add_fold_region(fold_region fr) {
					std::vector<fold_region> res;
					add_fold_region(fr, [&res](fold_region f) {
						res.push_back(f);
					});
					return res;
				}
				bool remove_fold_region(fold_region fr) {
					auto it = _fold.lower_bound(fr);
					if (it == _fold.end() || *it != fr) {
						return false;
					}
					remove_fold_region(fr);
				}
				void remove_fold_region(folding_info::iterator it) {
					_fold.erase(it);
					_on_folding_changed();
				}
				folding_info::iterator find_fold_region_containing_max(caret_position cp) {
					auto rem = _fold.lower_bound(fold_region(cp, cp));
					if (rem != _fold.end()) {
						if (rem->first != cp) {
							if (rem == _fold.begin()) {
								return _fold.end();
							}
							--rem;
							if (rem->second < cp) {
								return _fold.end();
							}
						}
					}
					return rem;
				}
				folding_info::iterator find_fold_region_containing_min(caret_position cp) {
					auto rem = _fold.lower_bound(fold_region(cp, cp));
					if (rem != _fold.begin()) {
						auto prev = rem;
						--prev;
						if (prev->second >= cp) {
							return prev;
						}
					}
					if (rem != _fold.end() && rem->first > cp) {
						return _fold.end();
					}
					return rem;
				}

				size_t unfolded_to_folded_linebeg(size_t l) const {
					size_t offset = 0;
					for (auto fiter = _fold.begin(); fiter != _fold.end(); ++fiter) {
						if (fiter->first.line >= l) {
							break;
						}
						if (fiter->second.line > l) {
							return fiter->first.line - offset;
						}
						offset += fiter->second.line - fiter->first.line;
					}
					return l - offset;
				}
				size_t folded_to_unfolded_linebeg(size_t l) const {
					for (auto fiter = _fold.begin(); fiter != _fold.end(); ++fiter) {
						if (fiter->first.line >= l) {
							break;
						}
						l += fiter->second.line - fiter->first.line;
					}
					return l;
				}
				caret_position unfolded_to_folded_pos(caret_position p) const {
					caret_position lend;
					caret_position_diff fixup;
					for (auto fiter = _fold.begin(); fiter != _fold.end(); ++fiter) {
						if (fiter->first >= p) {
							break;
						}
						if (fiter->first.line != lend.line) {
							fixup.x = 0;
						}
						fixup += fiter->first - fiter->second;
						if (fiter->second >= p) {
							return p + fixup;
						}
						lend = fiter->second;
					}
					if (p.line != lend.line) {
						fixup.x = 0;
					}
					return p + fixup;
				}
				caret_position folded_to_unfolded_pos(caret_position p) const {
					for (auto fiter = _fold.begin(); fiter != _fold.end(); ++fiter) {
						if (fiter->first >= p) {
							break;
						}
						p.line += fiter->second.line - fiter->first.line;
						if (p.line == fiter->second.line) {
							p.line = add_unsigned_diff(p.line, unsigned_diff<int>(
								fiter->second.column, fiter->first.column
								));
						}
					}
					return p;
				}
				size_t unfolded_linebeg(size_t l) const {
					fold_region queryrgn(caret_position(l, 0), caret_position(l, 0));
					auto fiter = _fold.lower_bound(queryrgn);
					if (fiter != _fold.begin()) {
						for (--fiter; fiter->second.line >= l; --fiter) {
							l = fiter->first.line;
							if (fiter == _fold.begin()) {
								break;
							}
						}
					}
					return l;
				}
				size_t unfolded_lineend(size_t l) const {
					fold_region queryrgn(caret_position(l, 0), caret_position(l, 0));
					for (auto fiter = _fold.lower_bound(queryrgn); fiter != _fold.end(); ++fiter) {
						if (fiter->first.line > l) {
							break;
						}
						l = fiter->second.line;
					}
					return l;
				}
				std::pair<size_t, size_t> get_visible_lines() const {
					double top = _get_box()->get_vertical_position();
					return get_visible_lines(top, top + get_client_region().height());
				}
				std::pair<size_t, size_t> get_visible_lines(double top, double bottom) const {
					double lh = get_line_height();
					return std::make_pair(
						folded_to_unfolded_linebeg(static_cast<size_t>(std::max(0.0, top / lh))), std::min(
							folded_to_unfolded_linebeg(static_cast<size_t>(bottom / lh) + 1),
							get_context()->num_lines()
						)
					);
				}
				double get_horizontal_caret_position(caret_position) const;
				// assumes that offset is the offset from the top-left of the document, rather than the element(editor)
				caret_position hit_test_for_caret(vec2d offset) const {
					size_t line = static_cast<size_t>(std::max(offset.y, 0.0) / get_line_height());
					line = std::min(folded_to_unfolded_linebeg(line), _ctx->num_lines() - 1);
					return hit_test_for_caret(line, offset.x);
				}
				caret_position hit_test_for_caret(size_t, double) const;

				event<void>
					content_visual_changed, content_modified,
					carets_changed, editing_visual_changed, folding_changed;

				inline static void set_font(const ui::font_family &ff) {
					_get_font() = ff;
				}
				inline static const ui::font_family &get_font() {
					return _get_font();
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
				event<void>::token _tc_tok;
				event<modification_info>::token _mod_tok;
				// current state
				double _scrolldiff = 0.0;
				caret_set _cset;
				caret_selection _cur_sel;
				vec2d _predrag_pos;
				bool _insert = true, _predrag = false, _selecting = false;
				// cache
				caret_position _mouse_cache;
				// folding
				folding_info _fold;

				struct _used_font_family {
					ui::font_family family;
				};
				static const ui::basic_pen *_caretpen;
				static const ui::basic_brush *_selbrush;
				static double _lines_per_scroll;
				static ui::font_family &_get_font();

				codebox *_get_box() const {
#ifdef CP_DETECT_LOGICAL_ERRORS
					codebox *cb = dynamic_cast<codebox*>(_parent);
					assert_true_logical(cb != nullptr, "the parent an of editor must be a codebox");
					return cb;
#else
					return static_cast<codebox*>(_parent);
#endif
				}

				struct _context_modifier_rsrc {
				public:
					explicit _context_modifier_rsrc(editor &cb) : _rsrc(*cb.get_context()), _editor(&cb) {
						if (cb._selecting) {
							cb._end_mouse_selection();
						}
					}
					~_context_modifier_rsrc() {
						_editor->set_carets(_rsrc.finish_edit(_editor));
					}

					text_context_modifier *operator->() {
						return &_rsrc;
					}
					text_context_modifier &get() {
						return _rsrc;
					}
					text_context_modifier &operator*() {
						return get();
					}
				protected:
					text_context_modifier _rsrc;
					editor *_editor;
				};

				void _make_caret_visible(caret_position cp) {
					codebox *cb = _get_box();
					double fh = get_line_height();
					vec2d np(get_horizontal_caret_position(cp), static_cast<double>(unfolded_to_folded_linebeg(cp.line) + 1) * fh);
					cb->make_point_visible(np);
					np.y -= fh;
					cb->make_point_visible(np);
				}
				caret_position _hit_test_for_caret_client(vec2d pos) const {
					return hit_test_for_caret(vec2d(pos.x, pos.y + _get_box()->get_vertical_position()));
				}

				void _on_content_modified(modification_info &mi) {
					caret_fixup_info::context fctx(mi.caret_fixup);
					std::list<fold_region> frs;
					for (auto i = _fold.begin(); i != _fold.end(); ++i) {
						fold_region fr;
						fr.first = mi.caret_fixup.fixup_caret_max(i->first, fctx);
						fr.second = mi.caret_fixup.fixup_caret_min(i->second, fctx);
						if (fr.first < fr.second) {
							frs.push_back(fr);
						}
					}
					_fold = folding_info(frs.begin(), frs.end());

					content_modified.invoke();
					_on_content_visual_changed();
					if (mi.source != this) {
						caret_fixup_info::context cctx(mi.caret_fixup);
						caret_set nset;
						for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
							bool cfront = i->first.first < i->first.second;
							caret_selection cs(i->first.first, i->first.second);
							if (!cfront) {
								std::swap(cs.first, cs.second);
							}
							cs.first = mi.caret_fixup.fixup_caret_max(cs.first, cctx);
							cs.second = mi.caret_fixup.fixup_caret_min(cs.second, cctx);
							if (cs.first > cs.second) {
								cs.second = cs.first;
							}
							if (!cfront) {
								std::swap(cs.first, cs.second);
							}
							nset.add(caret_set::entry(cs, caret_data(0.0)));
						}
						set_carets(nset);
					}
				}
				void _on_context_switch() {
					content_modified.invoke();
					_on_content_visual_changed();
				}
				void _on_content_visual_changed() {
					content_visual_changed.invoke();
					_on_editing_visual_changed();
				}
				void _on_editing_visual_changed() {
					editing_visual_changed.invoke();
					invalidate_visual();
				}
				void _on_carets_changed() {
					carets_changed.invoke();
					_on_editing_visual_changed();
				}
				void _on_folding_changed() {
					folding_changed.invoke();
					_on_editing_visual_changed();
				}

				void _begin_mouse_selection(caret_selection csel) {
					assert_true_logical(!_selecting, "_begin_mouse_selection() called when selecting");
					_selecting = true;
					_cur_sel = csel;
					_on_carets_changed();
					get_window()->set_mouse_capture(*this);
				}
				void _end_mouse_selection() {
					assert_true_logical(_selecting, "_end_mouse_selection() called when not selecting");
					_selecting = false;
					// the added caret may be merged, so alignment is calculated later
					auto it = _cset.add(std::make_pair(_cur_sel, caret_data(0.0)));
					it->second.alignment = get_horizontal_caret_position(it->first.first);
					_on_carets_changed();
					get_window()->release_mouse_capture();
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
				caret_set::entry _complete_caret_entry(caret_position fst, caret_position scnd) {
					return caret_set::entry(caret_selection(fst, scnd), get_horizontal_caret_position(fst));
				}
				caret_set::entry _complete_caret_entry(std::pair<caret_position, double> fst, caret_position scnd) {
					return caret_set::entry(caret_selection(fst.first, scnd), fst.second);
				}
				caret_set::entry _complete_caret_entry(std::pair<caret_position, double> fst, std::pair<caret_position, double> scnd) {
					return caret_set::entry(caret_selection(fst.first, scnd.first), fst.second);
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
					_mouse_cache = _hit_test_for_caret_client(clampedpos);
					if (_selecting) {
						if (_mouse_cache != _cur_sel.first) {
							_cur_sel.first = _mouse_cache;
							_on_carets_changed();
						}
					}
				}
				void _on_mouse_lbutton_up() {
					if (_selecting) {
						_end_mouse_selection();
					} else if (_predrag) {
						_predrag = false;
						caret_position hitp = _hit_test_for_caret_client(_predrag_pos - get_client_region().xmin_ymin());
						_cset.carets.clear();
						_cset.carets.insert(std::make_pair(
							std::make_pair(hitp, hitp), caret_data(get_horizontal_caret_position(hitp))
						));
						_on_carets_changed();
						get_window()->release_mouse_capture();
					} else {
						return;
					}
				}

				caret_position _move_caret_left(caret_position cp) {
					auto iter = find_fold_region_containing_min(cp);
					if (iter != _fold.end() && iter->first < cp) {
						return iter->first;
					}
					if (cp.column == 0) {
						if (cp.line == 0) {
							return cp;
						}
						--cp.line;
						auto lit = _ctx->at(cp.line);
						return caret_position(cp.line, lit->content.length());
					}
					return caret_position(cp.line, cp.column - 1);
				}
				caret_position _move_caret_right(caret_position cp) {
					auto iter = find_fold_region_containing_max(cp);
					if (iter != _fold.end() && iter->second > cp) {
						return iter->second;
					}
					auto lit = _ctx->at(cp.line);
					if (cp.column == lit->content.length()) {
						if (cp.line + 1 == _ctx->num_lines()) {
							return cp;
						}
						return caret_position(cp.line + 1, 0);
					}
					return caret_position(cp.line, cp.column + 1);
				}
				caret_position _move_caret_vertically(size_t line, int diff, double align) {
					line = unfolded_to_folded_linebeg(line);
					if (diff < 0 && -diff > static_cast<int>(line)) {
						line = 0;
					} else {
						line = folded_to_unfolded_linebeg(add_unsigned_diff(line, diff));
						line = std::min(line, _ctx->num_lines() - 1);
					}
					return hit_test_for_caret(line, align);
				}

				void _on_mouse_move(ui::mouse_move_info &info) override {
					_on_selecting_mouse_move(info.new_pos);
					if (_predrag) {
						if ((info.new_pos - _predrag_pos).length_sqr() > dragdrop_distance * dragdrop_distance) {
							_predrag = false;
							logger::get().log_info(CP_HERE, "starting drag & drop of text");
							get_window()->release_mouse_capture();
							// TODO start drag drop
						}
					}
					panel_base::_on_mouse_move(info);
				}
				void _on_mouse_down(ui::mouse_button_info &info) override {
					if (info.button == os::input::mouse_button::left) {
						_mouse_cache = _hit_test_for_caret_client(info.position - get_client_region().xmin_ymin());
						if (!_is_in_selection(_mouse_cache)) {
							if (os::input::is_key_down(os::input::key::shift)) {
								caret_set::container::iterator it = _cset.carets.end();
								--it;
								caret_selection cs = it->first;
								_cset.carets.erase(it);
								cs.first = _mouse_cache;
								_begin_mouse_selection(cs);
							} else {
								if (!os::input::is_key_down(os::input::key::control)) {
									_cset.carets.clear();
								}
								_begin_mouse_selection(std::make_pair(_mouse_cache, _mouse_cache));
							}
						} else {
							_predrag_pos = info.position;
							_predrag = true;
							get_window()->set_mouse_capture(*this);
						}
					} else if (info.button == os::input::mouse_button::middle) {
						// TODO block selection
					}
					panel_base::_on_mouse_down(info);
				}
				void _on_mouse_up(ui::mouse_button_info &info) override {
					if (info.button == os::input::mouse_button::left) {
						_on_mouse_lbutton_up();
					}
				}
				void _on_capture_lost() override {
					_on_mouse_lbutton_up();
				}
				void _on_key_down(ui::key_info &info) override {
					switch (info.key) {
						// deleting stuff
					case os::input::key::backspace:
						{
							_context_modifier_rsrc it(*this);
							for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
								it->on_backspace(i->first);
							}
						}
						break;
					case os::input::key::del:
						{
							_context_modifier_rsrc it(*this);
							for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
								it->on_delete(i->first);
							}
						}
						break;

						// movement control
					case os::input::key::left:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return _move_caret_left(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::min(et.first.first, et.first.second);
						});
						break;
					case os::input::key::right:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return _move_caret_right(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::max(et.first.first, et.first.second);
						});
						break;
					case os::input::key::up:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return std::make_pair(_move_caret_vertically(
								et.first.first.line, -1, et.second.alignment
							), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = et.first.first.line;
							double bl = et.second.alignment;
							if (et.first.second < et.first.first) {
								ml = et.first.second.line;
								bl = get_horizontal_caret_position(et.first.second);
							}
							return std::make_pair(_move_caret_vertically(ml, -1, bl), bl);
						});
						break;
					case os::input::key::down:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return std::make_pair(_move_caret_vertically(
								et.first.first.line, 1, et.second.alignment
							), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = et.first.first.line;
							double bl = et.second.alignment;
							if (et.first.second > et.first.first) {
								ml = et.first.second.line;
								bl = get_horizontal_caret_position(et.first.second);
							}
							return std::make_pair(_move_caret_vertically(ml, 1, bl), bl);
						});
						break;
					case os::input::key::home:
						move_carets([this](const caret_set::entry &et) {
							caret_position cp = et.first.first;
							size_t reall = unfolded_linebeg(cp.line);
							if (reall != cp.line) {
								cp.line = reall;
								cp.column = 0;
							}
							auto lit = _ctx->at(cp.line);
							size_t i = 0;
							while (i < lit->content.length() && (lit->content[i] == U' ' || lit->content[i] == U'\t')) {
								++i;
							}
							caret_position begp(cp.line, 0);
							auto fiter = _fold.lower_bound(fold_region(begp, begp));
							if (fiter != _fold.end() && fiter->first.line == cp.line) {
								i = std::min(i, fiter->first.column);
							}
							cp.column = (cp.column == i ? 0 : i);
							return cp;
						});
						break;
					case os::input::key::end:
						move_carets([this](caret_set::entry cp) {
							size_t line = unfolded_lineend(cp.first.first.line);
							auto it = _ctx->at(line);
							return std::make_pair(
								caret_position(line, it->content.length()),
								std::numeric_limits<double>::max()
							);
						});
						break;
					case os::input::key::page_up:
						// TODO page_up
						break;
					case os::input::key::page_down:
						// TODO page_down
						break;
					case os::input::key::escape:
						{
							caret_set ns;
							for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
								ns.add(caret_set::entry(caret_selection(i->first.first, i->first.first), caret_data(i->second.alignment)));
							}
							set_caret_keepalign(std::move(ns));
						}
						break;

						// edit mode toggle
					case os::input::key::insert:
						toggle_insert_mode();
						break;

					default:
						break;
					}
					panel_base::_on_key_down(info);
				}
				void _on_keyboard_text(ui::text_info &info) override {
					_context_modifier_rsrc it(*this);
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						if (_insert) {
							it->on_text(i->first, info.character);
						} else {
							it->on_text_overwrite(i->first, info.character);
						}
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
					panel_base::_initialize();
					set_padding(ui::thickness(2.0, 0.0, 0.0, 0.0));
					_can_focus = false;
				}
				void _dispose() override {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
					}
					panel_base::_dispose();
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
						if (_beg != c.end()) {
							_cpos = _beg->first.first;
						}
						_end = c.upper_bound(std::make_pair(pep, pep));
						if (_end != c.end()) {
							if (_end->first.second < pep) {
								++_end;
							}
						}
						_check_next(0.0, sp);
					}

					void switching_char(const character_rendering_iterator&, switch_char_info&);
					void switching_line(const character_rendering_iterator&);
				protected:
					caret_set::const_iterator _beg, _end;
					std::pair<caret_position, caret_position> _minmax;
					double _lastl = 0.0, _lh = 0.0;
					bool _insel = false;
					caret_position _cpos;
					const ui::basic_brush *_brush = nullptr;
					const ui::basic_pen *_pen = nullptr;

					void _check_next(double cl, caret_position cp) {
						if (_beg != _end) {
							_minmax = std::minmax({_beg->first.first, _beg->first.second});
							if (cp >= _minmax.first) {
								_cpos = _beg->first.first;
								++_beg;
								if (_minmax.first != _minmax.second) {
									_insel = true;
									_lastl = cl;
								}
							}
						}
					}
				};

				struct folded_region_skipper {
				public:
				protected:
					folding_info::const_iterator _cp, _end;
				};
			};

			struct character_rendering_iterator {
			public:
				character_rendering_iterator(const text_context &ctx, double lh, size_t sl, size_t pel) :
					_line_it(ctx.at(sl)),
					_theme_it(ctx.get_text_theme().get_iter_at(caret_position(sl, 0))),
					_char_it(_line_it->content, editor::get_font(), ctx.get_tab_width()),
					_ctx(&ctx), _cur_line(sl), _tg_line(pel), _line_h(lh) {
				}
				character_rendering_iterator(const editor &ce, size_t sl, size_t pel) :
					character_rendering_iterator(*ce.get_context(), ce.get_line_height(), sl, pel) {
					_edt = &ce;
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
						_cur_col = 0;
						++_cur_line;
						++_line_it;
						_cury += _line_h;
						_rcy = static_cast<int>(std::round(_cury));
						_ctx->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, 0));
						if (!ended()) {
							_char_it = ui::line_character_iterator(
								_line_it->content, editor::get_font(), _ctx->get_tab_width()
							);
							_char_it.begin(_theme_it.current_theme.style);
						}
					} else {
						switching_char.invoke_noret(false, caret_position(_cur_line, _cur_col + 1));
						++_cur_col;
						_ctx->get_text_theme().incr_iter(_theme_it, caret_position(_cur_line, _cur_col));
						_char_it.next(_theme_it.current_theme.style);
					}
				}
				void jump_to(caret_position cp) {
					switching_char.invoke_noret(true, cp);
					_cur_col = cp.column;
					if (cp.line != _cur_line) {
						_cur_line = cp.line;
						_line_it = _ctx->at(_cur_line);
					}
					_theme_it = _ctx->get_text_theme().get_iter_at(cp);
					_char_it.reposition(_line_it->content.begin() + cp.column, _line_it->content.end(), _theme_it.current_theme.style);
				}

				ui::line_character_iterator &character_info() {
					return _char_it;
				}
				const ui::line_character_iterator &character_info() const {
					return _char_it;
				}
				text_theme_data::char_iterator &theme_info() {
					return _theme_it;
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
				caret_position next_position() const {
					if (_char_it.ended()) {
						return caret_position(_cur_line + 1, 0);
					}
					return caret_position(_cur_line, _cur_col + 1);
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
				const editor *get_editor() const {
					return _edt;
				}

				event<void> switching_line;
				event<switch_char_info> switching_char;
			protected:
				text_context::const_line_iterator _line_it;
				text_theme_data::char_iterator _theme_it;
				ui::line_character_iterator _char_it;
				const editor *_edt = nullptr;
				const text_context *_ctx = nullptr;
				size_t _cur_line = 0, _cur_col = 0, _tg_line = 0;
				double _cury = 0.0, _line_h = 0.0;
				int _rcy = 0;
			};
			struct fold_region_skipper {
			public:
				fold_region_skipper(const editor::folding_info &fold, size_t sl, size_t pel) {
					caret_position sp(sl, 0), pep(pel, 0);
					_nextfr = fold.upper_bound(editor::fold_region(sp, sp));
					_max = fold.upper_bound(editor::fold_region(pep, pep));
				}

				bool check(character_rendering_iterator &it) {
					caret_position npos = it.current_position();
					if (_nextfr != _max && _nextfr->first == npos) {
						do {
							npos = _nextfr->second;
							it.jump_to(npos);
							it.character_info().create_blank(20.0);
							++_nextfr;
						} while (_nextfr != _max && _nextfr->first == npos);
						return true;
					}
					return false;
				}
			protected:
				editor::folding_info::const_iterator _nextfr, _max;
			};

			template <
				bool SkipFolded, typename F, typename ...Others
			> struct rendering_iterator<SkipFolded, F, Others...> :
				public rendering_iterator<SkipFolded, Others...> {
			public:
				template <size_t ...I, typename FCA, typename ...OCA> rendering_iterator(
					FCA &&f, OCA &&...ot
				) : rendering_iterator(
					std::make_index_sequence<std::tuple_size<typename std::decay<FCA>::type>::value>(),
					std::forward<FCA>(f), std::forward<OCA>(ot)...
				) {
				}

				template <typename T> T &get_addon() {
					return _get_addon_impl(_helper<T>());
				}
			protected:
				typedef rendering_iterator<SkipFolded, Others...> _direct_base;
				typedef rendering_iterator<SkipFolded> _root_base;

				template <size_t ...I, typename FCA, typename ...OCA> rendering_iterator(
					std::index_sequence<I...>, FCA &&f, OCA &&...ot
				) : _direct_base(std::forward<OCA>(ot)...), _curaddon(std::get<I>(std::forward<FCA>(f))...) {
					_root_base::char_iter().switching_char += [this](switch_char_info &pi) {
						_curaddon.switching_char(_root_base::char_iter(), pi);
					};
					_root_base::char_iter().switching_line += [this]() {
						_curaddon.switching_line(_root_base::char_iter());
					};
				}

				template <typename T> struct _helper {
				};
				template <typename T> T &_get_addon_impl(_helper<T>) {
					return _direct_base::template get_addon<T>();
				}
				F &_get_addon_impl(_helper<F>) {
					return _curaddon;
				}

				F _curaddon;
			};
			namespace _helper {
				template <bool Bv> struct optional_skipper {
					optional_skipper(const editor&, size_t, size_t) {
					}
					bool check(character_rendering_iterator &r) {
						return false;
					}
				};
				template <> struct optional_skipper<true> : public fold_region_skipper {
					optional_skipper(const editor &e, size_t sl, size_t pel) :
						fold_region_skipper(e.get_folding_info(), sl, pel) {
					}
				};
			}
			template <bool SkipFolded> struct rendering_iterator<SkipFolded> {
			public:
				rendering_iterator(const std::tuple<const editor&, size_t, size_t> &p) :
					rendering_iterator(std::get<0>(p), std::get<1>(p), std::get<2>(p)) {
				}
				rendering_iterator(const editor &ce, size_t sl, size_t pel) :
					_jiter(ce, sl, pel), _citer(ce, sl, pel) {
				}
				virtual ~rendering_iterator() {
				}

				bool begin() {
					_citer.begin();
					return _jiter.check(_citer);
				}
				bool next() {
					_citer.next_char();
					return _jiter.check(_citer);
				}
				bool ended() const {
					return _citer.ended();
				}

				template <typename T> typename std::enable_if<
					std::is_same<T, fold_region_skipper>::value && SkipFolded, T
				>::type &get_addon() {
					return _jiter;
				}
				character_rendering_iterator &char_iter() {
					return _citer;
				}
			protected:
				_helper::optional_skipper<SkipFolded> _jiter;
				character_rendering_iterator _citer;
			};

			inline editor *component::_get_editor() const {
				return _get_box()->get_editor();
			}

			inline void editor::caret_renderer::switching_char(
				const character_rendering_iterator &it, switch_char_info&
			) {
				double cx = it.character_info().char_left(), cy = it.y_offset();
				if (_insel) {
					if (_minmax.second <= it.current_position()) {
						_brush->fill_rect(rectd(_lastl, cx, cy, cy + it.line_height()));
						_insel = false;
					}
				}
				if (!_insel) {
					_check_next(cx, it.current_position());
				}
				if (it.current_position() == _cpos) {
					const editor *e = it.get_editor();
					if (e && e->is_insert_mode()) {
						_pen->draw_lines({vec2d(cx, cy), vec2d(cx, cy + it.line_height())});
					} else {
						if (it.is_line_break()) {
							_pen->draw_lines({
								vec2d(cx, cy + it.line_height()),
								vec2d(cx + _get_font().normal->get_char_entry(U' ').advance, cy + it.line_height())
							});
						} else {
							_pen->draw_lines({
								vec2d(cx, cy + it.line_height()),
								vec2d(it.character_info().char_right(), cy + it.line_height())
							});
						}
					}
				}
			}
			inline void editor::caret_renderer::switching_line(const character_rendering_iterator &it) {
				switch_char_info dummy;
				switching_char(it, dummy);
				if (_insel) {
					double finx =
						it.character_info().char_left() +
						it.character_info().get_font_family().get_by_style(
							it.theme_info().current_theme.style
						)->get_char_entry(U' ').advance;
					_brush->fill_rect(rectd(_lastl, finx, it.y_offset(), it.y_offset() + it.line_height()));
					_lastl = 0.0;
				}
			}

			inline void editor::_render() const {
				if (get_client_region().height() < 0.0) {
					return;
				}
				double lh = get_line_height();
				std::pair<size_t, size_t> be = get_visible_lines();
				caret_set::container ncon;
				const caret_set::container *used = &_cset.carets;
				if (_selecting) {
					ncon = *used;
					used = &ncon;
					caret_set::add_caret(ncon, std::make_pair(_cur_sel, caret_data(0.0)));
				}
				ui::font_family::baseline_info bi = _get_font().get_baseline_info();

				os::renderer_base::get().push_matrix(matd3x3::translate(vec2d(
					std::round(get_client_region().xmin),
					std::round(
						get_client_region().ymin - _get_box()->get_vertical_position() +
						static_cast<double>(unfolded_to_folded_linebeg(be.first)) * lh
					)
				)));
				rendering_iterator<true, caret_renderer> it(
					std::make_tuple(std::cref(*used), be.first, be.second, get_line_height(), _selbrush, _caretpen),
					std::make_tuple(std::cref(*this), be.first, be.second)
				);
				for (it.begin(); !it.ended(); it.next()) {
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
			inline double editor::get_horizontal_caret_position(caret_position pos) const {
				size_t fline = unfolded_to_folded_linebeg(pos.line);
				rendering_iterator<true> iter(*this, fline, _ctx->num_lines());
				for (iter.begin(); iter.char_iter().current_position() < pos; iter.next()) {
				}
				return iter.char_iter().character_info().prev_char_right();
			}
			inline caret_position editor::hit_test_for_caret(size_t line, double x) const {
				rendering_iterator<true> iter(*this, line, _ctx->num_lines());
				caret_position lastpos(line, 0);
				bool stop = false, lastjmp = false;
				iter.char_iter().switching_char += [&](switch_char_info &pi) {
					if (!stop) {
						if (lastjmp) {
							double midv = (
								iter.char_iter().character_info().prev_char_right() +
								iter.char_iter().character_info().char_left()
								) * 0.5;
							if (x < midv) {
								lastjmp = false;
								stop = true;
								return;
							}
							lastpos = iter.char_iter().current_position();
						}
						lastjmp = pi.is_jump;
						if (!pi.is_jump) {
							double midv = (
								iter.char_iter().character_info().char_left() +
								iter.char_iter().character_info().char_right()
								) * 0.5;
							if (x < midv) {
								stop = true;
								return;
							}
							lastpos = pi.next_position;
						} else {
							lastpos = iter.char_iter().current_position();
						}
					}
				};
				for (iter.begin(); !stop && !iter.char_iter().is_line_break(); iter.next()) {
				}
				if (lastjmp) {
					double midv = (
						iter.char_iter().character_info().prev_char_right() +
						iter.char_iter().character_info().char_left()
						) * 0.5;
					if (x > midv) {
						return iter.char_iter().current_position();
					}
				}
				return lastpos;
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
				_editor->content_modified += std::bind(&codebox::_reset_scrollbars, this);
				_editor->folding_changed += std::bind(&codebox::_reset_scrollbars, this);
				_children.add(*_editor);
			}
			inline void codebox::_reset_scrollbars() {
				_vscroll->set_params(_editor->get_vertical_scroll_range(), get_layout().height());
			}
			inline void codebox::_on_mouse_scroll(ui::mouse_scroll_info &p) {
				_vscroll->set_value(_vscroll->get_value() - _editor->get_scroll_delta() * p.delta);
				p.mark_handled();
			}
			inline void codebox::_on_key_down(ui::key_info &p) {
				_editor->_on_key_down(p);
			}
			inline void codebox::_on_key_up(ui::key_info &p) {
				_editor->_on_key_up(p);
			}
			inline void codebox::_on_keyboard_text(ui::text_info &p) {
				_editor->_on_keyboard_text(p);
			}
			inline void codebox::_finish_layout() {
				rectd lo = get_client_region();
				_child_recalc_layout(_vscroll, lo);

				rectd r = get_components_region();
				double lpos = r.xmin;
				for (auto i = _lcs.begin(); i != _lcs.end(); ++i) {
					double cw = (*i)->get_desired_size().x;
					ui::thickness mg = (*i)->get_margin();
					lpos += mg.left;
					_child_set_layout(*i, rectd(lpos, lpos + cw, lo.ymin, lo.ymax));
					lpos += cw + mg.right;
				}

				double rpos = r.xmax;
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
