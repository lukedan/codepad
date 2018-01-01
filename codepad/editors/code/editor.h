#pragma once

#include <memory>

#include "../../utilities/bst.h"
#include "context.h"
#include "view.h"

namespace codepad {
	namespace editor {
		namespace code {
			struct character_rendering_iterator;
			template <typename ...AddOns> struct rendering_iterator;

			struct switch_char_info {
				switch_char_info() = default;
				switch_char_info(bool jmp, caret_position next) : is_jump(jmp), next_position(next) {
				}
				const bool is_jump = false;
				const caret_position next_position{};
			};

			// TODO syntax highlighting, drag & drop, code folding, etc.
			class editor : public ui::element {
				friend struct codepad::globals;
				friend class codebox;
			public:
				constexpr static double
					move_speed_scale = 15.0,
					dragdrop_distance = 5.0;

				struct gizmo {
					gizmo() = default;
					gizmo(double w, std::shared_ptr<os::texture> tex) : width(w), texture(std::move(tex)) {
					}

					double width = 0.0;
					std::shared_ptr<os::texture> texture;
				};
				using gizmo_registry = incremental_positional_registry<gizmo>;

				void set_context(std::shared_ptr<text_context> nctx) {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
					}
					_ctx = std::move(nctx);
					_cset.reset();
					if (_ctx) {
						_mod_tok = (_ctx->modified += [this](modification_info &info) {
							_on_content_modified(info);
							});
						_tc_tok = (_ctx->visual_changed += std::bind(&editor::_on_content_visual_changed, this));
						_fmt = _ctx->create_view_formatting();
					} else {
						_fmt = view_formatting();
					}
					_on_context_switch();
				}
				const std::shared_ptr<text_context> &get_context() const {
					return _ctx;
				}

				size_t get_num_visual_lines() const {
					return _fmt.get_linebreaks().num_visual_lines() - _fmt.get_folding().folded_linebreaks();
				}
				double get_line_height() const {
					return get_font().maximum_height();
				}
				double get_scroll_delta() const {
					return get_line_height() * _lines_per_scroll;
				}
				double get_vertical_scroll_range() const {
					return get_line_height() * static_cast<double>(get_num_visual_lines() - 1) + get_client_region().height();
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
						i->second.alignment = _get_caret_pos_x_caretpos(i->first.first);
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
					const GetPos &gp, const SelPos &sp, bool continueselection
				) {
					if (continueselection) {
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
				template <typename GetPos> void move_carets(const GetPos &gp, bool continueselection) {
					return move_carets_special_selection(gp, gp, continueselection);
				}

				bool is_insert_mode() const {
					return _insert;
				}
				void set_insert_mode(bool v) {
					_insert = v;
					_reset_caret_animation();
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

				const view_formatting &get_formatting() const {
					return _fmt;
				}
				// TODO fix carets
				void add_folded_region(const view_formatting::fold_region &fr) {
					_fmt.add_folded_region(fr);
					_on_folding_changed();
				}
				void remove_folded_region(folding_registry::iterator f) {
					_fmt.remove_folded_region(f);
					_on_folding_changed();
				}
				void clear_folded_regions() {
					_fmt.clear_folded_regions();
					_on_folding_changed();
				}

				const gizmo_registry &get_gizmos() const {
					return _gizmos;
				}
				void insert_gizmo(const typename gizmo_registry::const_iterator &pos, size_t offset, gizmo g) {
					_gizmos.insert_at(pos, offset, std::move(g));
					_on_editing_visual_changed();
				}
				void insert_gizmo(caret_position pos, gizmo g) {
					_gizmos.insert_at(pos, std::move(g));
					_on_editing_visual_changed();
				}
				void remove_gizmo(const typename gizmo_registry::const_iterator &it) {
					_gizmos.erase(it);
					_on_editing_visual_changed();
				}

				std::pair<size_t, size_t> get_visible_lines() const {
					double top = _get_box()->get_vertical_position();
					return get_visible_lines(top, top + get_client_region().height());
				}
				std::pair<size_t, size_t> get_visible_lines(double top, double bottom) const {
					double lh = get_line_height();
					return std::make_pair(_fmt.get_folding().folded_to_unfolded_line_number(
						static_cast<size_t>(std::max(0.0, top / lh))
					), std::min(
						_fmt.get_folding().folded_to_unfolded_line_number(static_cast<size_t>(bottom / lh) + 1),
						_fmt.get_linebreaks().num_visual_lines()
					));
				}
				// assumes that offset is the offset from the top-left of the document, rather than the element(editor)
				caret_position hit_test_for_caret(vec2d offset) const {
					size_t line = static_cast<size_t>(std::max(offset.y, 0.0) / get_line_height());
					line = std::min(_fmt.get_folding().folded_to_unfolded_line_number(line), get_num_visual_lines() - 1);
					return _hit_test_unfolded_linebeg(line, offset.x);
				}

				// edit operations
				void move_all_carets_left(bool continueselection) {
					move_carets_special_selection([this](const caret_set::entry &et) {
						return _move_caret_left(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::min(et.first.first, et.first.second);
						}, continueselection);
				}
				void move_all_carets_right(bool continueselection) {
					move_carets_special_selection([this](const caret_set::entry &et) {
						return _move_caret_right(et.first.first);
						}, [](const caret_set::entry &et) {
							return std::max(et.first.first, et.first.second);
						}, continueselection);
				}
				void move_all_carets_up(bool continueselection) {
					move_carets_special_selection([this](const caret_set::entry &et) {
						return std::make_pair(_move_caret_vertically(
							_get_line_of_caret(et.first.first), -1, et.second.alignment
						), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = _get_line_of_caret(et.first.first);
							double bl = et.second.alignment;
							if (et.first.second < et.first.first) {
								ml = _get_line_of_caret(et.first.second);
								bl = _get_caret_pos_x_caretpos(et.first.second);
							}
							return std::make_pair(_move_caret_vertically(ml, -1, bl), bl);
						}, continueselection);
				}
				void move_all_carets_down(bool continueselection) {
					move_carets_special_selection([this](const caret_set::entry &et) {
						return std::make_pair(_move_caret_vertically(
							_get_line_of_caret(et.first.first), 1, et.second.alignment
						), et.second.alignment);
						}, [this](const caret_set::entry &et) {
							size_t ml = _get_line_of_caret(et.first.first);
							double bl = et.second.alignment;
							if (et.first.second > et.first.first) {
								ml = _get_line_of_caret(et.first.second);
								bl = _get_caret_pos_x_caretpos(et.first.second);
							}
							return std::make_pair(_move_caret_vertically(ml, 1, bl), bl);
						}, continueselection);
				}
				void move_all_carets_to_line_beginning(bool continueselection) {
					move_carets([this](const caret_set::entry &et) {
						size_t line = _get_line_of_caret(et.first.first);
						return _fmt.get_linebreaks().get_beginning_char_of_visual_line(
							_fmt.get_folding().get_beginning_line_of_folded_lines(line)
						);
						}, continueselection);
				}
				void move_all_carets_to_line_beginning_advanced(bool continueselection) {
					move_carets([this](const caret_set::entry &et) {
						caret_position cp = et.first.first;
						size_t
							line = _get_line_of_caret(cp),
							reall = _fmt.get_folding().get_beginning_line_of_folded_lines(line),
							begp = _fmt.get_linebreaks().get_beginning_char_of_visual_line(reall),
							exbegp = begp;
						for (
							auto cit = _ctx->at_char(begp);
							!cit.is_linebreak() && (cit.current_character() == ' ' || cit.current_character() == '\t');
							++cit, ++exbegp
							) {
						}
						auto fiter = _fmt.get_folding().find_region_containing_open(exbegp);
						if (std::get<0>(fiter) != _fmt.get_folding().end()) {
							exbegp = std::get<1>(fiter) + std::get<0>(fiter)->gap;
						}
						return cp == exbegp ? begp : exbegp;
						}, continueselection);
				}
				void move_all_carets_to_line_ending(bool continueselection) {
					move_carets([this](caret_set::entry cp) {
						return std::make_pair(_fmt.get_linebreaks().get_past_ending_char_of_visual_line(
							_fmt.get_folding().get_past_ending_line_of_folded_lines(
								_get_line_of_caret(cp.first.first)
							) - 1
						), std::numeric_limits<double>::max());
						}, continueselection);
				}
				void cancel_all_selections() {
					caret_set ns;
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						ns.add(caret_set::entry(
							caret_selection(i->first.first, i->first.first), caret_data(i->second.alignment))
						);
					}
					set_caret_keepalign(std::move(ns));
				}

				void delete_selection_or_char_before() {
					if (
						_cset.carets.size() > 1 ||
						_cset.carets.begin()->first.first != _cset.carets.begin()->first.second ||
						_cset.carets.begin()->first.first != 0
						) {
						_context_modifier_rsrc it(*this);
						for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
							it->on_backspace(i->first);
						}
					}
				}
				void delete_selection_or_char_after() {
					if (
						_cset.carets.size() > 1 ||
						_cset.carets.begin()->first.first != _cset.carets.begin()->first.second ||
						_cset.carets.begin()->first.first != _ctx->num_chars()
						) {
						_context_modifier_rsrc it(*this);
						for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
							it->on_delete(i->first);
						}
					}
				}

				void test_full_layout();

				event<void>
					content_visual_changed, content_modified,
					carets_changed, editing_visual_changed, folding_changed;

				inline static void set_font(const ui::font_family &ff) {
					_get_appearance().family = ff;
				}
				inline static const ui::font_family &get_font() {
					return _get_appearance().family;
				}
				inline static void set_selection_brush(std::shared_ptr<ui::basic_brush> b) {
					_get_appearance().selection_brush = std::move(b);
				}
				inline static const std::shared_ptr<ui::basic_brush> &get_selection_brush() {
					return _get_appearance().selection_brush;
				}

				inline static void set_folding_gizmo(gizmo gz) {
					_get_appearance().folding_gizmo = std::move(gz);
				}
				inline static const gizmo &get_folding_gizmo() {
					return _get_appearance().folding_gizmo;
				}

				inline static void set_num_lines_per_scroll(double v) {
					_lines_per_scroll = v;
				}
				inline static double get_num_lines_per_scroll() {
					return _lines_per_scroll;
				}

				inline static str_t get_default_class() {
					return CP_STRLIT("editor");
				}
				inline static str_t get_insert_caret_class() {
					return CP_STRLIT("caret_insert");
				}
				inline static str_t get_overwrite_caret_class() {
					return CP_STRLIT("caret_overwrite");
				}
			protected:
				std::shared_ptr<text_context> _ctx;
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
				// view
				view_formatting _fmt;
				double _view_width = 0.0;
				// gizmos
				gizmo_registry _gizmos;
				// rendering
				ui::visual::render_state _caretst;

				struct _appearance_config {
					gizmo folding_gizmo;
					ui::font_family family;
					std::shared_ptr<ui::basic_brush> selection_brush;
				};
				static double _lines_per_scroll;
				static _appearance_config &_get_appearance();

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
					editor *_editor = nullptr;
				};

				size_t _get_line_of_caret(caret_position cpos) const {
					return _fmt.get_linebreaks().get_visual_line_and_column_of_char(cpos).first;
				}
				caret_position _hit_test_unfolded_linebeg(size_t, double) const;
				double _get_caret_pos_x_unfolded_linebeg(size_t, caret_position) const;
				double _get_caret_pos_x_folded_linebeg(size_t line, caret_position position) const {
					return _get_caret_pos_x_unfolded_linebeg(
						_fmt.get_folding().folded_to_unfolded_line_number(line), position
					);
				}
				double _get_caret_pos_x_caretpos(caret_position cpos) const {
					size_t line = _get_line_of_caret(cpos);
					line = _fmt.get_folding().get_beginning_line_of_folded_lines(line);
					return _get_caret_pos_x_unfolded_linebeg(line, cpos);
				}

				void _make_caret_visible(caret_position cp) {
					codebox *cb = _get_box();
					double fh = get_line_height();
					size_t
						ufline = _get_line_of_caret(cp),
						fline = _fmt.get_folding().unfolded_to_folded_line_number(ufline);
					vec2d np(
						_get_caret_pos_x_folded_linebeg(fline, cp),
						static_cast<double>(fline + 1) * fh
					);
					cb->make_point_visible(np);
					np.y -= fh;
					cb->make_point_visible(np);
				}
				void _reset_caret_animation() {
					_caretst.set_class(_insert ? get_insert_caret_class() : get_overwrite_caret_class());
				}
				caret_position _hit_test_for_caret_client(vec2d pos) const {
					return hit_test_for_caret(vec2d(pos.x, pos.y + _get_box()->get_vertical_position()));
				}

				void _on_content_modified(modification_info &mi) {
					_gizmos.fixup(mi.caret_fixup);

					content_modified.invoke();
					_on_content_visual_changed();
					if (mi.source != this) { // fixup carets
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
					_reset_caret_animation();
					carets_changed.invoke();
					_on_editing_visual_changed();
				}
				void _on_folding_changed() {
					folding_changed.invoke();
					_on_editing_visual_changed();
				}
				void _on_width_changed();

				void _on_codebox_got_focus() {
					_caretst.set_state_bit(ui::visual_manager::default_states().focused, true);
				}
				void _on_codebox_lost_focus() {
					_caretst.set_state_bit(ui::visual_manager::default_states().focused, false);
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
					it->second.alignment = _get_caret_pos_x_caretpos(it->first.first);
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
					return caret_set::entry(caret_selection(fst, scnd), caret_data(
						_get_caret_pos_x_caretpos(fst)));
				}
				caret_set::entry _complete_caret_entry(std::pair<caret_position, double> fst, caret_position scnd) {
					return caret_set::entry(caret_selection(fst.first, scnd), caret_data(fst.second));
				}
				caret_set::entry _complete_caret_entry(std::pair<caret_position, double> fst, std::pair<caret_position, double> scnd) {
					return caret_set::entry(caret_selection(fst.first, scnd.first), caret_data(fst.second));
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
							std::make_pair(hitp, hitp), caret_data(_get_caret_pos_x_caretpos(hitp))
						));
						_on_carets_changed();
						get_window()->release_mouse_capture();
					} else {
						return;
					}
				}

				caret_position _move_caret_left(caret_position cp) {
					auto res = _fmt.get_folding().find_region_containing_or_first_before_open(cp);
					auto &it = std::get<0>(res);
					if (it != _fmt.get_folding().end()) {
						size_t fp = std::get<1>(res) + it->gap, ep = fp + it->range;
						if (ep >= cp && fp < cp) {
							return fp;
						}
					}
					return cp > 0 ? cp - 1 : 0;
				}
				caret_position _move_caret_right(caret_position cp) {
					auto res = _fmt.get_folding().find_region_containing_or_first_after_open(cp);
					auto &it = std::get<0>(res);
					if (it != _fmt.get_folding().end()) {
						size_t fp = std::get<1>(res) + it->gap, ep = fp + it->range;
						if (fp <= cp && ep > cp) {
							return ep;
						}
					}
					size_t nchars = _ctx->num_chars();
					return cp < nchars ? cp + 1 : nchars;
				}
				caret_position _move_caret_vertically(size_t line, int diff, double align) {
					line = _fmt.get_folding().unfolded_to_folded_line_number(line);
					if (diff < 0 && -diff > static_cast<int>(line)) {
						line = 0;
					} else {
						line = _fmt.get_folding().folded_to_unfolded_line_number(line + diff);
						line = std::min(line, get_num_visual_lines() - 1);
					}
					return _hit_test_unfolded_linebeg(line, align);
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
					element::_on_mouse_move(info);
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
						// movement control
					case os::input::key::page_up:
						// TODO page_up
						break;
					case os::input::key::page_down:
						// TODO page_down
						break;
					case os::input::key::escape:
						break;

					default:
						break;
					}
					element::_on_key_down(info);
				}
				void _on_keyboard_text(ui::text_info &info) override {
					_context_modifier_rsrc it(*this);
					for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
						string_buffer::string_type st;
						st.reserve(info.content.length());
						for (
							string_codepoint_iterator cc(info.content.begin(), info.content.end());
							!cc.at_end();
							cc.next()
							) {
							if (*cc != '\n') {
								translate_codepoint_utf8([&st](std::initializer_list<char8_t> cs) {
									st.append_as_codepoint(std::move(cs));
									}, *cc);
							} else {
								char8_t tmp[2], *end = &tmp[1];
								switch (_ctx->get_default_line_ending()) {
								case line_ending::rn:
									end = static_cast<char8_t*>(tmp) + 2;
									tmp[0] = '\r';
									tmp[1] = '\n';
									break;
								case line_ending::r:
									tmp[0] = '\r';
									break;
								case line_ending::n:
									tmp[0] = '\n';
									break;
								default:
									continue;
								}
								st.append_as_codepoint(tmp, end);
							}
						}
						it->on_text(i->first, std::move(st), _insert);
					}
				}
				void _on_update() override {
					if (_selecting) {
						codebox *editor = _get_box();
						editor->set_vertical_position(
							editor->get_vertical_position() +
							move_speed_scale * _scrolldiff * ui::manager::get().update_delta_time()
						);
						_on_selecting_mouse_move(get_window()->screen_to_client(os::input::get_mouse_position()).convert<double>());
					}
				}

				void _custom_render() override;
				void _finish_layout() override {
					double cw = get_client_region().width();
					if (std::abs(cw - _view_width) > 0.1) { // TODO magik!
						_on_width_changed();
					}
					element::_finish_layout();
				}

				void _initialize() override {
					element::_initialize();
					set_padding(ui::thickness(2.0, 0.0, 0.0, 0.0));
					_can_focus = false;
					_reset_caret_animation();
				}
				void _dispose() override {
					if (_ctx) {
						_ctx->modified -= _mod_tok;
						_ctx->visual_changed -= _tc_tok;
					}
					element::_dispose();
				}
			public:
				struct caret_renderer {
				public:
					caret_renderer(
						const caret_set::container &c, caret_position sp, caret_position pep
					) {
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

					const std::vector<rectd> &get_caret_rects() const {
						return _carets;
					}
					const std::vector<std::vector<rectd>> &get_selection_rects() const {
						return _selrgns;
					}
				protected:
					std::vector<rectd> _carets;
					std::vector<std::vector<rectd>> _selrgns;
					caret_set::const_iterator _beg, _end;
					std::pair<caret_position, caret_position> _minmax;
					double _lastl = 0.0;
					bool _insel = false;
					caret_position _cpos;

					void _check_next(double cl, caret_position cp) {
						if (_beg != _end) {
							_minmax = std::minmax({_beg->first.first, _beg->first.second});
							if (cp >= _minmax.first) {
								_cpos = _beg->first.first;
								++_beg;
								if (_minmax.first != _minmax.second) {
									_selrgns.emplace_back();
									_insel = true;
									_lastl = cl;
								}
							}
						}
					}
				};
				struct gizmo_renderer {
				public:
					using gizmo_rendering_info = std::pair<vec2d, gizmo_registry::const_iterator>;

					gizmo_renderer(
						const gizmo_registry &reg, caret_position sp, caret_position pep
					) : _beg(reg.find_at_or_first_after(sp)), _end(reg.find_at_or_first_after(pep)) {
					}

					void switching_char(const character_rendering_iterator&, switch_char_info&);
					void switching_line(const character_rendering_iterator&);

					const std::vector<gizmo_rendering_info> &get_gizmos() const {
						return _gzs;
					}
				protected:
					std::vector<gizmo_rendering_info> _gzs;
					gizmo_registry::const_iterator _beg, _end;
				};
			};

			struct character_rendering_iterator {
			public:
				character_rendering_iterator(const text_context &ctx, double lh, caret_position sp, caret_position pep) :
					_char_it(ctx.at_char(sp)), _theme_it(ctx.get_text_theme().get_iter_at(sp)),
					_char_met(editor::get_font(), ctx.get_tab_width()),
					_ctx(&ctx), _cur_pos(sp), _tg_pos(pep), _line_h(lh) {
				}
				character_rendering_iterator(const editor &ce, caret_position sp, caret_position pep) :
					character_rendering_iterator(*ce.get_context(), ce.get_line_height(), sp, pep) {
					_edt = &ce;
				}

				bool is_hard_line_break() const {
					return _char_it.is_linebreak();
				}
				bool ended() const {
					return _cur_pos > _tg_pos || _char_it.is_end();
				}
				void begin() {
					if (!is_hard_line_break()) {
						_char_met.next(_char_it.current_character(), _theme_it.current_theme.style);
					}
				}
				void next_char() {
					assert_true_usage(!ended(), "incrementing an invalid rendering iterator");
					if (is_hard_line_break()) {
						switching_line.invoke();
						_cury += _line_h;
						_rcy = static_cast<int>(std::round(_cury));
						_char_met.reset();
					} else {
						switching_char.invoke_noret(false, _cur_pos + 1);
					}
					++_cur_pos;
					++_char_it;
					_ctx->get_text_theme().incr_iter(_theme_it, _cur_pos);
					_char_met.next(
						is_hard_line_break() ? ' ' : _char_it.current_character(), _theme_it.current_theme.style
					);
				}
				rectd create_blank(double w) {
					_char_met.create_blank_before(w);
					return rectd(_char_met.prev_char_right(), _char_met.char_left(), _cury, _cury + _line_h);
				}
				void jump_to(caret_position cp) {
					switching_char.invoke_noret(true, cp);
					_cur_pos = cp;
					_char_it = _ctx->at_char(_cur_pos);
					_theme_it = _ctx->get_text_theme().get_iter_at(_cur_pos);
					_char_met.replace_current(
						is_hard_line_break() ? ' ' : _char_it.current_character(), _theme_it.current_theme.style
					);
				}
				void insert_soft_linebreak() { // TODO extract duplicate code
					switching_line.invoke();
					_cury += _line_h;
					_rcy = static_cast<int>(std::round(_cury));
					_char_met.reset();
					_char_met.next(_char_it.current_character(), _theme_it.current_theme.style);
				}

				const ui::character_metrics_accumulator &character_info() const {
					return _char_met;
				}
				const text_theme_data::char_iterator &theme_info() const {
					return _theme_it;
				}
				caret_position current_position() const {
					return _cur_pos;
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
				text_context::iterator _char_it;
				text_theme_data::char_iterator _theme_it;
				ui::character_metrics_accumulator _char_met;
				const editor *_edt = nullptr;
				const text_context *_ctx = nullptr;
				caret_position _cur_pos{}, _tg_pos{};
				double _cury = 0.0, _line_h = 0.0;
				int _rcy = 0;
			};
			struct fold_region_skipper {
			public:
				constexpr static double gizmo_size = 20.0; // TODO temporary gizmo size

				fold_region_skipper(const folding_registry &fold, caret_position sp, caret_position pep) {
					auto fr = fold.find_region_containing_or_first_after_open(sp);
					_nextfr = std::get<0>(fr);
					_chars = std::get<1>(fr);
					_max = std::get<0>(fold.find_region_containing_or_first_after_open(pep)); // TODO maybe change this to end()
				}

				bool check(character_rendering_iterator &it) {
					caret_position npos = it.current_position();
					if (_nextfr != _max && _chars + _nextfr->gap == npos) {
						do {
							_chars += _nextfr->gap + _nextfr->range;
							npos = _chars;
							it.jump_to(npos);
							_gizmo_rects.push_back(it.create_blank(gizmo_size));
							++_nextfr;
						} while (_nextfr != _max && _nextfr->gap == 0);
						return true;
					}
					return false;
				}

				const std::vector<rectd> &get_gizmos() const {
					return _gizmo_rects;
				}
			protected:
				std::vector<rectd> _gizmo_rects;
				folding_registry::iterator _nextfr, _max;
				size_t _chars = 0;
			};
			struct soft_linebreak_inserter {
			public:
				soft_linebreak_inserter(
					const soft_linebreak_registry &reg, caret_position sp, caret_position pep
				) : _end(reg.end()) {
					auto pos = reg.get_softbreak_before_char(sp);
					_next = std::get<0>(pos);
					_ncs = std::get<2>(pos);
				}

				bool check(character_rendering_iterator &it) {
					if (_next != _end) {
						if (it.current_position() == _next->length + _ncs) {
							it.insert_soft_linebreak();
							_ncs += _next->length;
							++_next;
						}
					}
					return false;
				}
			protected:
				soft_linebreak_registry::iterator _next, _end;
				size_t _ncs;
			};
			struct view_formatter {
			public:
				view_formatter(const view_formatting &fmt, caret_position sp, caret_position pep) :
					_fold(fmt.get_folding(), sp, pep), _lb(fmt.get_linebreaks(), sp, pep) {
				}

				bool check(character_rendering_iterator &it) {
					if (!_fold.check(it)) {
						return _lb.check(it);
					}
					return true;
				}

				const fold_region_skipper &get_fold_region_skipper() const {
					return _fold;
				}
				const soft_linebreak_inserter &get_soft_linebreak_inserter() const {
					return _lb;
				}
			protected:
				fold_region_skipper _fold;
				soft_linebreak_inserter _lb;
			};

			namespace _details {
				template <typename T> class is_checker {
				protected:
					using _yes = std::int8_t;
					using _no = std::int64_t;
					template <typename C> inline static _yes _test(decltype(&C::check)) {
					}
					template <typename C> inline static _no _test(...) {
					}
				public:
					constexpr static bool value = sizeof(_test<T>(0)) == sizeof(_yes);
				};
			}
			template <
				typename F, typename ...Others
			> struct rendering_iterator<F, Others...> :
				public rendering_iterator<Others...> {
			public:
				template <typename FCA, typename ...OCA> rendering_iterator(
					FCA &&f, OCA &&...ot
				) : rendering_iterator(
					std::make_index_sequence<std::tuple_size<std::decay_t<FCA>>::value>(),
					std::forward<FCA>(f), std::forward<OCA>(ot)...
				) {
					_root_base::char_iter().switching_char += [this](switch_char_info &pi) {
						_switching_char_all(pi);
					};
					_root_base::char_iter().switching_line += [this]() {
						_switching_line_all();
					};
				}

				bool begin() {
					_root_base::_begin();
					return _check_all();
				}
				bool next() {
					_root_base::_next();
					return _check_all();
				}

				template <typename T> T &get_addon() {
					return _get_addon_impl(_get_helper<T>());
				}

				template <size_t ...I, typename FCA, typename SCA, typename ...OCA> rendering_iterator(
					std::index_sequence<I...>, FCA &&f, SCA &&s, OCA &&...ot
				) : _direct_base(
					std::make_index_sequence<std::tuple_size<std::decay_t<SCA>>::value>(),
					std::forward<SCA>(s), std::forward<OCA>(ot)...
				), _curaddon(std::get<I>(std::forward<FCA>(f))...) {
				}
			private:
				using _direct_base = rendering_iterator<Others...>;
				using _root_base = rendering_iterator<>;

				template <typename T> struct _get_helper {
				};
				template <typename T> T &_get_addon_impl(_get_helper<T>) {
					return _direct_base::template get_addon<T>();
				}
				F &_get_addon_impl(_get_helper<F>) {
					return _curaddon;
				}

				template <bool V> struct _check_helper {
				};
				using _cur_check_helper = _check_helper<_details::is_checker<F>::value>;
				bool _check(_check_helper<true>) {
					return _curaddon.check(_root_base::_citer);
				}
				void _switching_char(switch_char_info &info, _check_helper<false>) {
					_curaddon.switching_char(_root_base::char_iter(), info);
				}
				void _switching_line(_check_helper<false>) {
					_curaddon.switching_line(_root_base::char_iter());
				}
				// disabled funcs
				bool _check(_check_helper<false>) {
					return false;
				}
				void _switching_char(switch_char_info&, _check_helper<true>) {
				}
				void _switching_line(_check_helper<true>) {
				}

				F _curaddon;
			protected:
				bool _check_all() {
					if (!_check(_cur_check_helper())) {
						return _direct_base::_check_all();
					}
					return true;
				}
				void _switching_char_all(switch_char_info &info) {
					_switching_char(info, _cur_check_helper());
					_direct_base::_switching_char_all(info);
				}
				void _switching_line_all() {
					_switching_line(_cur_check_helper());
					_direct_base::_switching_line_all();
				}
			};
			template <> struct rendering_iterator<> {
			public:
				rendering_iterator(const std::tuple<const editor&, caret_position, caret_position> &p) :
					rendering_iterator(std::get<0>(p), std::get<1>(p), std::get<2>(p)) {
				}
				rendering_iterator(const editor &ce, caret_position sp, caret_position pep) : _citer(ce, sp, pep) {
				}
				virtual ~rendering_iterator() {
				}

				bool ended() const {
					return _citer.ended();
				}

				character_rendering_iterator &char_iter() {
					return _citer;
				}
			protected:
				template <size_t ...I, typename T> rendering_iterator(
					std::index_sequence<I...>, T &&arg
				) : rendering_iterator(std::forward<T>(arg)) {
				}

				character_rendering_iterator _citer;

				void _begin() {
					_citer.begin();
				}
				void _next() {
					_citer.next_char();
				}

				bool _check_all() {
					return false;
				}
				void _switching_char_all(switch_char_info &info) {
				}
				void _switching_line_all() {
				}
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
						_selrgns.back().push_back(rectd(_lastl, cx, cy, cy + it.line_height()));
						_insel = false;
					}
				}
				if (!_insel) {
					_check_next(cx, it.current_position());
				}
				if (it.current_position() == _cpos) {
					if (it.is_hard_line_break()) {
						_carets.push_back(rectd::from_xywh(
							cx, cy, get_font().normal->get_char_entry(' ').advance, it.line_height()
						));
					} else {
						_carets.push_back(rectd(cx, it.character_info().char_right(), cy, cy + it.line_height()));
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
						)->get_char_entry(' ').advance;
					_selrgns.back().push_back(rectd(_lastl, finx, it.y_offset(), it.y_offset() + it.line_height()));
					_lastl = 0.0;
				}
			}

			inline void editor::gizmo_renderer::switching_char(const character_rendering_iterator &it, switch_char_info&) {
				// TODO
			}
			inline void editor::gizmo_renderer::switching_line(const character_rendering_iterator &it) {
				// TODO
			}

			inline void editor::_custom_render() {
				monitor_performance mon(CP_HERE);
				if (get_client_region().height() < 0.0) {
					return;
				}
				double lh = get_line_height(), layoutw = get_layout().width();
				std::pair<size_t, size_t> be = get_visible_lines();
				caret_set::container ncon;
				const caret_set::container *used = &_cset.carets;
				if (_selecting) {
					ncon = *used;
					used = &ncon;
					caret_set::add_caret(ncon, std::make_pair(_cur_sel, caret_data(0.0)));
				}
				ui::font_family::baseline_info bi = get_font().get_baseline_info();

				os::renderer_base::get().push_matrix(matd3x3::translate(vec2d(
					std::round(get_client_region().xmin),
					std::round(
						get_client_region().ymin - _get_box()->get_vertical_position() +
						static_cast<double>(_fmt.get_folding().unfolded_to_folded_line_number(be.first)) * lh
					)
				)));
				caret_position
					firstchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.first),
					plastchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.second);
				rendering_iterator<view_formatter, caret_renderer> it(
					std::make_tuple(std::cref(_fmt), firstchar, plastchar),
					std::make_tuple(std::cref(*used), firstchar, plastchar),
					std::make_tuple(std::cref(*this), firstchar, plastchar)
				);
				for (it.begin(); !it.ended(); it.next()) {
					const character_rendering_iterator &cri = it.char_iter();
					if (!cri.is_hard_line_break()) {
						const ui::character_metrics_accumulator &lci = cri.character_info();
						const text_theme_data::char_iterator &ti = cri.theme_info();
						if (is_graphical_char(lci.current_char())) {
							vec2d xpos =
								vec2d(lci.char_left(), std::round(cri.rounded_y_offset() + bi.get(ti.current_theme.style))) +
								lci.current_char_entry().placement.xmin_ymin();
							if (xpos.x < layoutw) {
								os::renderer_base::get().draw_character(
									lci.current_char_entry().texture, xpos, ti.current_theme.color
								);
							}
						}
					}
				}
				if (_get_appearance().folding_gizmo.texture) {
					ui::render_batch batch;
					for (const rectd &r : it.get_addon<view_formatter>().get_fold_region_skipper().get_gizmos()) {
						batch.add_quad(r, rectd(0.0, 1.0, 0.0, 1.0), colord());
					}
					batch.draw(*_get_appearance().folding_gizmo.texture);
				}
				for (const auto &selrgn : it.get_addon<caret_renderer>().get_selection_rects()) {
					for (const auto &rgn : selrgn) {
						_get_appearance().selection_brush->fill_rect(rgn);
					}
				}
				if (_caretst.update_and_render_multiple(it.get_addon<caret_renderer>().get_caret_rects())) {
					invalidate_visual();
				}
				os::renderer_base::get().pop_matrix();
			}
			inline void editor::_on_width_changed() {
				_view_width = get_client_region().width();
				logger::get().log_info(CP_HERE, "recalculating soft breaks");
				rendering_iterator<fold_region_skipper> it(
					std::make_tuple(std::cref(_fmt.get_folding()), 0, _ctx->num_chars()),
					std::make_tuple(std::cref(*this), 0, _ctx->num_chars())
				);
				std::vector<size_t> poss;
				bool lb = true;
				for (it.begin(); !it.ended(); it.next()) {
					if (
						!lb && !it.char_iter().is_hard_line_break() &&
						it.char_iter().character_info().char_right() > _view_width
						) {
						it.char_iter().insert_soft_linebreak();
						poss.push_back(it.char_iter().current_position());
						lb = true;
					} else {
						lb = false;
					}
				}
				_fmt.set_softbreaks(poss);
				_on_editing_visual_changed();
			}
			inline double editor::_get_caret_pos_x_unfolded_linebeg(size_t line, caret_position position) const {
				size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line);
				rendering_iterator<fold_region_skipper> iter(
					std::make_tuple(std::cref(_fmt.get_folding()), begc, _ctx->num_chars()),
					std::make_tuple(std::cref(*this), begc, _ctx->num_chars())
				);
				for (iter.begin(); iter.char_iter().current_position() < position; iter.next()) {
				}
				return iter.char_iter().character_info().prev_char_right();
			}
			inline caret_position editor::_hit_test_unfolded_linebeg(size_t line, double x) const {
				size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line);
				rendering_iterator<fold_region_skipper> iter(
					std::make_tuple(std::cref(_fmt.get_folding()), begc, _ctx->num_chars()),
					std::make_tuple(std::cref(*this), begc, _ctx->num_chars())
				);
				caret_position lastpos = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line);
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
				for (iter.begin(); !stop && !iter.char_iter().is_hard_line_break(); iter.next()) {
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
			inline void editor::test_full_layout() {
				auto begt = std::chrono::high_resolution_clock::now();
				rendering_iterator<fold_region_skipper> it(
					std::make_tuple(std::cref(_fmt.get_folding()), 0, _ctx->num_chars()),
					std::make_tuple(std::cref(*this), 0, _ctx->num_chars())
				);
				for (it.begin(); !it.ended(); it.next()) {
				}
				logger::get().log_info(CP_HERE, "full layout took ", std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - begt).count(), "s");
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
			inline void codebox::_on_got_focus() {
				panel_base::_on_got_focus();
				_editor->_on_codebox_got_focus();
			}
			inline void codebox::_on_lost_focus() {
				_editor->_on_codebox_lost_focus();
				panel_base::_on_lost_focus();
			}
		}
	}
}
