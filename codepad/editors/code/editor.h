#pragma once

#include "context.h"

namespace codepad {
	namespace editor {
		namespace code {
			struct character_rendering_iterator;

			// TODO syntax highlighting, drag & drop, etc.
			class editor : public component {
			public:
				constexpr static double
					move_speed_scale = 15.0,
					dragdrop_distance = 5.0;

				void set_context(text_context *nctx) {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
					}
					_ctx = nctx;
					_cset.reset();
					if (_ctx) {
						_mod_tok = (_ctx->modified += std::bind(&editor::_on_content_modified, this));
						_tc_tok = (_ctx->visual_changed += std::bind(&editor::_on_content_visual_changed, this));
					}
					_on_content_modified();
				}
				text_context *get_context() const {
					return _ctx;
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
				void set_carets(caret_set cs) {
					assert_true_usage(cs.carets.size() > 0, "must have at least one caret");
					for (auto i = cs.carets.begin(); i != cs.carets.end(); ++i) {
						i->second.alignment = _ctx->get_horizontal_caret_position(i->first.first, _font);
					}
					set_caret_keepalign(std::move(cs));
				}
				void set_caret_keepalign(caret_set cs) {
					assert_true_logical(cs.carets.size() > 0, "must have at least one caret");
					_cset = std::move(cs);
					_make_caret_visible(_cset.carets.rbegin()->first.first);
					_on_carets_changed();
				}
				template <typename GetData> void move_carets_raw(const GetData &gd) {
					caret_set ncs;
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						ncs.add(gd(*i));
					}
					set_caret_keepalign(ncs);
				}
				template <typename GetPos, typename SelPos> void move_carets_special_selection(const GetPos &gp, const SelPos &sp) {
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

				std::pair<size_t, size_t> get_visible_lines() const {
					double lh = get_line_height(), pos = _get_box()->get_vertical_position();
					return std::make_pair(static_cast<size_t>(std::max(0.0, pos / lh)), std::min(
						static_cast<size_t>((pos + get_client_region().height()) / lh) + 1, get_context()->num_lines()
					));
				}

				event<void> content_visual_changed, content_modified, carets_changed;

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
#ifndef NDEBUG
				// debug stuff
				bool _modifying = false;
#endif

				static const ui::basic_pen *_caretpen;
				static const ui::basic_brush *_selbrush;
				static ui::font_family _font;
				static double _lines_per_scroll;

				struct _context_modifier_rsrc {
				public:
					explicit _context_modifier_rsrc(editor &cb) : _rsrc(*cb.get_context()), _editor(&cb) {
					}
					~_context_modifier_rsrc() {
						_editor->_cset = _rsrc.finish_edit(_editor);
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
					vec2d np(_ctx->get_horizontal_caret_position(cp, _font), static_cast<double>(cp.line + 1) * fh);
					cb->make_point_visible(np);
					np.y -= fh;
					cb->make_point_visible(np);
				}

				void _on_content_modified() {
					content_modified.invoke();
					_on_content_visual_changed();
				}
				void _on_content_visual_changed() {
					content_visual_changed.invoke();
					invalidate_visual();
				}
				void _on_carets_changed() {
					carets_changed.invoke();
					invalidate_visual();
				}

				void _begin_selection(caret_selection csel) {
					assert_true_logical(!_selecting, "_begin_selection() called when selecting");
					_selecting = true;
					_cur_sel = csel;
					_on_carets_changed();
				}
				void _end_selection() {
					assert_true_logical(_selecting, "_end_selection() called when not selecting");
					_selecting = false;
					bool merged = false;
					auto it = _cset.add(std::make_pair(
						_cur_sel, caret_data(_ctx->get_horizontal_caret_position(_cur_sel.first, _font))
					), merged);
					if (merged) {
						it->second.alignment = _ctx->get_horizontal_caret_position(it->first.first, _font);
					}
					_on_carets_changed();
				}

				caret_position _hit_test_for_caret(vec2d pos) {
					caret_position cp;
					cp.line = static_cast<size_t>(std::max(0.0, (pos.y + _get_box()->get_vertical_position()) / get_line_height()));
					if (cp.line >= _ctx->num_lines()) {
						cp.line = _ctx->num_lines() - 1;
					}
					cp.column = _ctx->hit_test_for_caret(cp.line, pos.x, _font);
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
				caret_set::entry _complete_caret_entry(caret_position fst, caret_position scnd) {
					return caret_set::entry(caret_selection(fst, scnd), _ctx->get_horizontal_caret_position(fst, _font));
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
					_mouse_cache = _hit_test_for_caret(clampedpos);
					if (_selecting) {
						if (_mouse_cache != _cur_sel.first) {
							_cur_sel.first = _mouse_cache;
							_on_carets_changed();
						}
					}
				}
				void _on_mouse_lbutton_up() {
					if (_selecting) {
						_end_selection();
					} else if (_predrag) {
						_predrag = false;
						caret_position hitp = _hit_test_for_caret(_predrag_pos - get_client_region().xmin_ymin());
						_cset.carets.clear();
						_cset.carets.insert(std::make_pair(
							std::make_pair(hitp, hitp), caret_data(_ctx->get_horizontal_caret_position(hitp, _font))
						));
						_on_carets_changed();
					} else {
						return;
					}
					get_window()->release_mouse_capture();
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
					if (info.button == os::input::mouse_button::left) {
						_mouse_cache = _hit_test_for_caret(info.position - get_client_region().xmin_ymin());
						if (!_is_in_selection(_mouse_cache)) {
							if (os::input::is_key_down(os::input::key::shift)) {
								caret_set::container::iterator it = _cset.carets.end();
								--it;
								caret_selection cs = it->first;
								_cset.carets.erase(it);
								cs.first = _mouse_cache;
								_begin_selection(cs);
							} else {
								if (!os::input::is_key_down(os::input::key::control)) {
									_cset.carets.clear();
								}
								_begin_selection(std::make_pair(_mouse_cache, _mouse_cache));
							}
						} else {
							_predrag_pos = info.position;
							_predrag = true;
						}
						get_window()->set_mouse_capture(*this);
					} else if (info.button == os::input::mouse_button::middle) {
						// TODO block selection
					}
					element::_on_mouse_down(info);
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
							return _ctx->move_caret_left(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::min(et.first.first, et.first.second);
						});
						break;
					case os::input::key::right:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return _ctx->move_caret_right(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::max(et.first.first, et.first.second);
						});
						break;
					case os::input::key::up:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return std::make_pair(_ctx->move_caret_vertically(
								et.first.first.line, -1, et.second.alignment, _font
							), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = et.first.first.line;
							double bl = et.second.alignment;
							if (et.first.second < et.first.first) {
								ml = et.first.second.line;
								bl = _ctx->get_horizontal_caret_position(et.first.second, _font);
							}
							return std::make_pair(_ctx->move_caret_vertically(ml, -1, bl, _font), bl);
						});
						break;
					case os::input::key::down:
						move_carets_special_selection([this](const caret_set::entry &et) {
							return std::make_pair(_ctx->move_caret_vertically(
								et.first.first.line, 1, et.second.alignment, _font
							), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = et.first.first.line;
							double bl = et.second.alignment;
							if (et.first.second > et.first.first) {
								ml = et.first.second.line;
								bl = _ctx->get_horizontal_caret_position(et.first.second, _font);
							}
							return std::make_pair(_ctx->move_caret_vertically(ml, 1, bl, _font), bl);
						});
						break;
					case os::input::key::home:
						move_carets([this](const caret_set::entry &et) {
							caret_position cp = et.first.first;
							auto lit = _ctx->at(cp.line);
							size_t i = 0;
							while (i < lit->content.length() && (lit->content[i] == U' ' || lit->content[i] == U'\t')) {
								++i;
							}
							cp.column = (cp.column == i ? 0 : i);
							return cp;
						});
						break;
					case os::input::key::end:
						move_carets([this](caret_set::entry cp) {
							auto it = _ctx->at(cp.first.first.line);
							return std::make_pair(
								caret_position(cp.first.first.line, it->content.length()),
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
						_insert = !_insert;
						invalidate_visual();
						break;

					default:
						break;
					}
					element::_on_key_down(info);
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
					component::_initialize();
					set_padding(ui::thickness(2.0, 0.0, 0.0, 0.0));
				}
				void _dispose() override {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
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

					void switching_char(character_rendering_iterator&);
					void switching_line(character_rendering_iterator&);
				protected:
					caret_set::const_iterator _beg, _end;
					std::pair<caret_position, caret_position> _minmax;
					double _lastl = 0.0, _lh = 0.0;
					bool _rendering = false;
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
				character_rendering_iterator(const text_context &ctx, double lh, size_t sl, size_t pel) :
					_line_it(ctx.at(sl)),
					_theme_it(ctx.get_text_theme().get_iter_at(caret_position(sl, 0))),
					_char_it(_line_it->content, editor::get_font(), ctx.get_tab_width()),
					_ctx(&ctx), _cur_line(sl), _tg_line(pel), _line_h(lh) {
				}
				character_rendering_iterator(const editor &ce, size_t sl, size_t pel) :
					character_rendering_iterator(*ce.get_context(), ce.get_line_height(), sl, pel) {
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
							_char_it = ui::line_character_iterator(_line_it->content, editor::get_font(), _ctx->get_tab_width());
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
				double _cury = 0.0, _line_h = 0.0;
				int _rcy = 0;
			};

			struct rendering_iterator {
			public:
				rendering_iterator(
					const editor &ce, const caret_set::container &c,
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
					_check_next(cx, it.current_position());
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
					caret_set::add_caret(ncon, std::make_pair(_cur_sel, caret_data(0.0)));
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
