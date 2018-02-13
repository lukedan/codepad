#pragma once

#include <memory>

#include "../../core/bst.h"
#include "context.h"
#include "view.h"
#include "codebox.h"

namespace codepad::editor::code {
	struct character_rendering_iterator;
	template <typename ...AddOns> struct rendering_iterator;

	struct switch_char_info {
		switch_char_info() = default;
		switch_char_info(bool jmp, size_t next) : is_jump(jmp), next_position(next) {
		}
		const bool is_jump = false;
		const size_t next_position = 0;
	};
	struct switch_line_info {
		switch_line_info() = default;
		switch_line_info(line_ending t) : type(t) {
		}
		const line_ending type = line_ending::none;
	};

	// TODO syntax highlighting, drag & drop, code folding, etc.
	class editor : public ui::element {
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

		void set_context(std::shared_ptr<text_context>);
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

		os::cursor get_current_display_cursor() const override {
			if (!_selecting && _is_in_selection(_mouse_cache.first)) {
				return os::cursor::normal;
			}
			return os::cursor::text_beam;
		}

		const caret_set &get_carets() const {
			return _cset;
		}
		void set_carets(const std::vector<caret_selection> &cs) {
			assert_true_usage(cs.size() > 0, "must have at least one caret");
			caret_set set;
			for (const caret_selection &sel : cs) {
				caret_set::entry et(sel, caret_data());
				et.second.alignment = _get_caret_pos_x_caret_entry(et);
				set.add(et);
			}
			set_carets(std::move(set));
		}
		void set_carets(caret_set cs) {
			assert_true_logical(cs.carets.size() > 0, "must have at least one caret");
			_cset = std::move(cs);
			_make_caret_visible(*_cset.carets.rbegin()); // TODO move to a better position
			_on_carets_changed();
		}
		template <typename GetData> void move_carets_raw(const GetData &gd) {
			caret_set ncs;
			for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
				ncs.add(gd(*i));
			}
			set_carets(ncs);
		}
		template <typename GetPos, typename SelPos> void move_carets(
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
						return _complete_caret_entry(x, std::get<0>(x));
					} else {
						auto x = sp(et);
						return _complete_caret_entry(x, std::get<0>(x));
					}
					});
			}
		}
		template <typename GetPos> void move_carets(const GetPos &gp, bool continueselection) {
			return move_carets(gp, gp, continueselection);
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
		void insert_gizmo(size_t pos, gizmo g) {
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
		std::pair<size_t, bool> hit_test_for_caret(vec2d offset) const {
			size_t line = static_cast<size_t>(std::max(offset.y, 0.0) / get_line_height());
			line = std::min(_fmt.get_folding().folded_to_unfolded_line_number(line), get_num_visual_lines() - 1);
			return _hit_test_unfolded_linebeg(line, offset.x);
		}

		// caret movement
		void move_all_carets_left(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(_move_caret_left(et.first.first), true);
				}, [](const caret_set::entry &et) {
					if (et.first.first < et.first.second) {
						return std::make_pair(et.first.first, et.second.softbreak_next_line);
					} else {
						return std::make_pair(et.first.second, true);
					}
				}, continueselection);
		}
		void move_all_carets_right(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(_move_caret_right(et.first.first), false);
				}, [](const caret_set::entry &et) {
					if (et.first.first > et.first.second) {
						return std::make_pair(et.first.first, et.second.softbreak_next_line);
					} else {
						return std::make_pair(et.first.second, false);
					}
				}, continueselection);
		}
		void move_all_carets_up(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				auto res = _move_caret_vertically(_get_line_of_caret(et), -1, et.second.alignment);
				return std::make_pair(res.first, caret_data(et.second.alignment, res.second));
				}, [this](const caret_set::entry &et) {
					size_t ml = _get_line_of_caret(et);
					double bl = et.second.alignment;
					if (et.first.second < et.first.first) {
						ml = _get_line_of_caret(et.first.second, true);
						bl = _get_caret_pos_x_caret_pos(et.first.second, true);
					}
					auto res = _move_caret_vertically(ml, -1, bl);
					return std::make_pair(res.first, caret_data(bl, res.second));
				}, continueselection);
		}
		void move_all_carets_down(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				auto res = _move_caret_vertically(_get_line_of_caret(et), 1, et.second.alignment);
				return std::make_pair(res.first, caret_data(et.second.alignment, res.second));
				}, [this](const caret_set::entry &et) {
					size_t ml = _get_line_of_caret(et);
					double bl = et.second.alignment;
					if (et.first.second > et.first.first) {
						ml = _get_line_of_caret(et.first.second, true);
						bl = _get_caret_pos_x_caret_pos(et.first.second, true);
					}
					auto res = _move_caret_vertically(ml, 1, bl);
					return std::make_pair(res.first, caret_data(bl, res.second));
				}, continueselection);
		}
		void move_all_carets_to_line_beginning(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				return std::make_pair(
					_fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().get_beginning_line_of_folded_lines(_get_line_of_caret(et))
					).first, true
				);
				}, continueselection);
		}
		void move_all_carets_to_line_beginning_advanced(bool continueselection) {
			move_carets([this](const caret_set::entry &et) {
				size_t
					line = _get_line_of_caret(et),
					reall = _fmt.get_folding().get_beginning_line_of_folded_lines(line);
				auto linfo = _fmt.get_linebreaks().get_line_info(reall);
				size_t begp = std::max(linfo.first.first_char, linfo.second.prev_chars), exbegp = begp;
				if (linfo.first.first_char > linfo.second.prev_chars) {
					size_t nextsb = linfo.second.prev_chars + (
						linfo.second.entry == _fmt.get_linebreaks().end() ? 0 : linfo.second.entry->length
						);
					for (
						auto cit = _ctx->at_char(begp);
						!cit.is_linebreak() && (
							cit.current_character() == ' ' || cit.current_character() == '\t'
							) && exbegp < nextsb;
						++cit, ++exbegp
						) {
					}
					auto finfo = _fmt.get_folding().find_region_containing_open(exbegp);
					if (finfo.entry != _fmt.get_folding().end()) {
						exbegp = finfo.prev_chars + finfo.entry->gap;
					}
					if (nextsb <= exbegp) {
						exbegp = begp;
					}
				}
				return std::make_pair(et.first.first == exbegp ? begp : exbegp, true);
				}, continueselection);
		}
		void move_all_carets_to_line_ending(bool continueselection) {
			move_carets([this](caret_set::entry cp) {
				return std::make_pair(
					_fmt.get_linebreaks().get_past_ending_char_of_visual_line(
						_fmt.get_folding().get_past_ending_line_of_folded_lines(
							_get_line_of_caret(cp)
						) - 1
					).first, caret_data(std::numeric_limits<double>::max(), false)
				);
				}, continueselection);
		}
		void cancel_all_selections() {
			caret_set ns;
			for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
				ns.add(caret_set::entry(
					caret_selection(i->first.first, i->first.first), i->second)
				);
			}
			set_carets(std::move(ns));
		}

		// edit operations
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

		// for profiling
		void recalc_linebreaks() {
			_recalculate_wrapping_region(0, _ctx->num_chars());
		}

		event<void>
			content_visual_changed, content_modified, carets_changed,
			editing_visual_changed, folding_changed, wrapping_changed;

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
		size_t _sel_end;
		vec2d _predrag_pos;
		bool _insert = true, _predrag = false, _selecting = false;
		// cache
		std::pair<size_t, bool> _mouse_cache;
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

		size_t _get_line_of_caret(size_t cpos, bool slbnext) const {
			auto res = _fmt.get_linebreaks().get_visual_line_and_column_and_softbreak_before_or_at_char(cpos);
			if (
				!slbnext &&
				std::get<2>(res).entry != _fmt.get_linebreaks().begin() &&
				std::get<2>(res).prev_chars == cpos
				) {
				return std::get<0>(res) - 1;
			}
			return std::get<0>(res);
		}
		size_t _get_line_of_caret(const caret_set::entry &c) const {
			return _get_line_of_caret(c.first.first, c.second.softbreak_next_line);
		}
		std::pair<size_t, bool> _hit_test_unfolded_linebeg(size_t, double) const;
		double _get_caret_pos_x_unfolded_linebeg(size_t, size_t) const;
		double _get_caret_pos_x_folded_linebeg(size_t line, size_t position) const {
			return _get_caret_pos_x_unfolded_linebeg(
				_fmt.get_folding().folded_to_unfolded_line_number(line), position
			);
		}
		double _get_caret_pos_x_caret_pos(size_t pos, bool slbnext) const {
			size_t line = _get_line_of_caret(pos, slbnext);
			line = _fmt.get_folding().get_beginning_line_of_folded_lines(line);
			return _get_caret_pos_x_unfolded_linebeg(line, pos);
		}
		double _get_caret_pos_x_caret_entry(const caret_set::entry &et) const {
			return _get_caret_pos_x_caret_pos(et.first.first, et.second.softbreak_next_line);
		}

		void _make_caret_visible(const caret_set::entry &et) {
			_make_caret_visible(et.first.first, et.second.softbreak_next_line);
		}
		void _make_caret_visible(size_t cp, bool slbnext) {
			codebox *cb = _get_box();
			double fh = get_line_height();
			size_t
				ufline = _get_line_of_caret(cp, slbnext),
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
		std::pair<size_t, bool> _hit_test_for_caret_client(vec2d pos) const {
			return hit_test_for_caret(vec2d(pos.x, pos.y + _get_box()->get_vertical_position()));
		}

		void _on_content_modified(modification_info&);
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
			invalidate_visual();
		}
		void _on_folding_changed() {
			folding_changed.invoke();
			_on_editing_visual_changed();
		}
		void _on_wrapping_changed() {
			wrapping_changed.invoke();
			_on_editing_visual_changed();
		}

		std::vector<size_t> _recalculate_wrapping_region(size_t, size_t) const;

		void _on_codebox_got_focus() {
			_caretst.set_state_bit(ui::visual::get_predefined_states().focused, true);
		}
		void _on_codebox_lost_focus() {
			_caretst.set_state_bit(ui::visual::get_predefined_states().focused, false);
		}

		void _begin_mouse_selection(size_t startpos) {
			assert_true_logical(!_selecting, "_begin_mouse_selection() called when selecting");
			_selecting = true;
			_sel_end = startpos;
			_on_carets_changed();
			get_window()->set_mouse_capture(*this);
		}
		void _end_mouse_selection() {
			assert_true_logical(_selecting, "_end_mouse_selection() called when not selecting");
			_selecting = false;
			// the added caret may be merged, so alignment is calculated later
			auto it = _cset.add(caret_set::entry(
				caret_selection(_mouse_cache.first, _sel_end), caret_data(0.0, _mouse_cache.second)
			));
			it->second.alignment = _get_caret_pos_x_caret_entry(*it);
			_on_carets_changed();
			get_window()->release_mouse_capture();
		}

		bool _is_in_selection(size_t cp) const {
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
		caret_data _get_caret_data(const std::pair<size_t, bool> &p) const {
			return caret_data(_get_caret_pos_x_caret_pos(p.first, p.second), p.second);
		}
		caret_set::entry _complete_caret_entry(std::pair<size_t, bool> fst, size_t scnd) {
			return caret_set::entry(caret_selection(fst.first, scnd), _get_caret_data(fst));
		}
		caret_set::entry _complete_caret_entry(std::tuple<size_t, caret_data> fst, size_t scnd) {
			return caret_set::entry(caret_selection(std::get<0>(fst), scnd), std::get<1>(fst));
		}

		void _update_mouse_selection(vec2d pos) {
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
			auto newmouse = _hit_test_for_caret_client(clampedpos);
			if (_selecting && newmouse != _mouse_cache) {
				_on_carets_changed();
			}
			_mouse_cache = newmouse;
		}
		void _on_mouse_lbutton_up() {
			if (_selecting) {
				_end_mouse_selection();
			} else if (_predrag) {
				_predrag = false;
				auto hitp = _hit_test_for_caret_client(_predrag_pos - get_client_region().xmin_ymin());
				_cset.carets.clear();
				_cset.carets.insert(caret_set::entry(
					caret_selection(hitp.first, hitp.first), _get_caret_data(hitp)
				));
				_on_carets_changed();
				get_window()->release_mouse_capture();
			} else {
				return;
			}
		}

		size_t _move_caret_left(size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_before_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (ep >= cp && fp < cp) {
					return fp;
				}
			}
			return cp > 0 ? cp - 1 : 0;
		}
		size_t _move_caret_right(size_t cp) {
			auto res = _fmt.get_folding().find_region_containing_or_first_after_open(cp);
			auto &it = res.entry;
			if (it != _fmt.get_folding().end()) {
				size_t fp = res.prev_chars + it->gap, ep = fp + it->range;
				if (fp <= cp && ep > cp) {
					return ep;
				}
			}
			size_t nchars = _ctx->num_chars();
			return cp < nchars ? cp + 1 : nchars;
		}
		std::pair<size_t, bool> _move_caret_vertically(size_t line, int diff, double align) {
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
			_update_mouse_selection(info.new_pos);
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
			_mouse_cache = _hit_test_for_caret_client(info.position - get_client_region().xmin_ymin());
			if (info.button == os::input::mouse_button::left) {
				if (!_is_in_selection(_mouse_cache.first)) {
					if (os::input::is_key_down(os::input::key::shift)) {
						caret_set::container::iterator it = _cset.carets.end();
						--it;
						caret_selection cs = it->first;
						_cset.carets.erase(it);
						_begin_mouse_selection(cs.second);
					} else {
						if (!os::input::is_key_down(os::input::key::control)) {
							_cset.carets.clear();
						}
						_begin_mouse_selection(_mouse_cache.first);
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
						st.append_as_codepoint(utf8<>::encode_codepoint(*cc));
					} else {
						const char *cs = line_ending_to_static_u8_string(_ctx->get_default_line_ending());
						st.append_as_codepoint(
							cs, cs + get_linebreak_length(_ctx->get_default_line_ending())
						);
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
				_update_mouse_selection(get_window()->screen_to_client(
					os::input::get_mouse_position()).convert<double>()
				);
			}
		}

		void _custom_render() override;
		void _finish_layout() override {
			double cw = get_client_region().width();
			if (std::abs(cw - _view_width) > 0.1) { // TODO magik!
				_view_width = get_client_region().width();
				{
					performance_monitor(CP_STRLIT("recalculate_wrapping"));
					_fmt.set_softbreaks(_recalculate_wrapping_region(0, _ctx->num_chars()));
				}
				_on_wrapping_changed();
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
				const caret_set::container &c, size_t sp, size_t pep
			) {
				_cur = c.lower_bound(std::make_pair(sp, size_t()));
				if (_cur != c.begin()) {
					auto prev = _cur;
					--prev;
					if (prev->first.second >= sp) {
						_cur = prev;
					}
				}
				if (_cur != c.end()) {
					_minmax = std::minmax({_cur->first.first, _cur->first.second});
				}
				for (auto it = _cur; it != c.end() && it->first.first <= pep; ++it) {
					std::pair<size_t, bool> cp = {it->first.first, it->second.softbreak_next_line};
					if (_caretposs.size() > 0 && cp < _caretposs.back()) {
						std::swap(cp, _caretposs.back());
					}
					_caretposs.push_back(cp);
				}
				_curcidx = _caretposs.begin();
				_end = c.end();
			}

			void switching_char(const character_rendering_iterator&, switch_char_info&);
			void switching_line(const character_rendering_iterator&, switch_line_info&);

			const std::vector<rectd> &get_caret_rects() const {
				return _caretrects;
			}
			const std::vector<std::vector<rectd>> &get_selection_rects() const {
				return _selrgns;
			}
		protected:
			std::vector<rectd> _caretrects;
			std::vector<std::pair<size_t, bool>> _caretposs;
			std::vector<std::vector<rectd>> _selrgns;
			caret_set::const_iterator _cur, _end;
			std::pair<size_t, size_t> _minmax;
			double _lastl = 0.0;
			decltype(_caretposs)::const_iterator _curcidx;
			bool _insel = false, _last_slb = false;

			void _check_next(double cl, size_t cp) {
				if (_cur != _end && cp >= _minmax.first) {
					if (_minmax.first != _minmax.second) {
						_selrgns.emplace_back();
						_insel = true;
						_lastl = cl;
					} else {
						if (++_cur != _end) {
							_minmax = std::minmax({_cur->first.first, _cur->first.second});
						}
					}
				}
			}
			void _on_switching_char(const character_rendering_iterator&, bool);
			void _check_draw_caret(const character_rendering_iterator&, bool);
		};
		struct gizmo_renderer {
		public:
			using gizmo_rendering_info = std::pair<vec2d, gizmo_registry::const_iterator>;

			gizmo_renderer(
				const gizmo_registry &reg, size_t sp, size_t pep
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
		character_rendering_iterator(const text_context &ctx, double lh, size_t sp, size_t pep) :
			_char_it(ctx.at_char(sp)), _theme_it(ctx.get_text_theme().get_iter_at(sp)),
			_char_met(editor::get_font(), ctx.get_tab_width()),
			_ctx(&ctx), _cur_pos(sp), _tg_pos(pep), _line_h(lh) {
		}
		character_rendering_iterator(const editor &ce, size_t sp, size_t pep) :
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
				_on_linebreak(_char_it.current_linebreak());
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
		void jump_to(size_t cp) {
			switching_char.invoke_noret(true, cp);
			_cur_pos = cp;
			_char_it = _ctx->at_char(_cur_pos);
			_theme_it = _ctx->get_text_theme().get_iter_at(_cur_pos);
			_char_met.replace_current(
				is_hard_line_break() ? ' ' : _char_it.current_character(), _theme_it.current_theme.style
			);
		}
		void insert_soft_linebreak() {
			_on_linebreak(line_ending::none);
			_char_met.next(_char_it.current_character(), _theme_it.current_theme.style);
		}

		const ui::character_metrics_accumulator &character_info() const {
			return _char_met;
		}
		const text_theme_data::char_iterator &theme_info() const {
			return _theme_it;
		}
		size_t current_position() const {
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

		event<switch_line_info> switching_line;
		event<switch_char_info> switching_char;
	protected:
		text_context::iterator _char_it;
		text_theme_data::char_iterator _theme_it;
		ui::character_metrics_accumulator _char_met;
		const editor *_edt = nullptr;
		const text_context *_ctx = nullptr;
		size_t _cur_pos = 0, _tg_pos = 0;
		double _cury = 0.0, _line_h = 0.0;
		int _rcy = 0;

		void _on_linebreak(line_ending end) {
			switching_line.invoke_noret(end);
			_cury += _line_h;
			_rcy = static_cast<int>(std::round(_cury));
			_char_met.reset();
		}
	};
	struct fold_region_skipper {
	public:
		constexpr static double gizmo_size = 50.0; // TODO temporary gizmo size

		fold_region_skipper(const folding_registry &fold, size_t sp) : _max(fold.end()) {
			auto fr = fold.find_region_containing_or_first_after_open(sp);
			_nextfr = fr.entry;
			_chars = fr.prev_chars;
			_max = fold.end();
		}

		bool check(character_rendering_iterator &it) {
			size_t npos = it.current_position();
			if (_nextfr != _max && _chars + _nextfr->gap == npos) {
				_chars += _nextfr->gap + _nextfr->range;
				npos = _chars;
				it.jump_to(npos);
				_gizmo_rects.push_back(it.create_blank(gizmo_size));
				++_nextfr;
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
		soft_linebreak_inserter(const soft_linebreak_registry &reg, size_t sp) : _end(reg.end()) {
			auto pos = reg.get_softbreak_before_or_at_char(sp);
			_next = pos.entry;
			_ncs = pos.prev_chars;
		}

		bool check(character_rendering_iterator &it) {
			if (_next != _end && it.current_position() >= _next->length + _ncs) {
				it.insert_soft_linebreak();
				_ncs += _next->length;
				++_next;
			}
			return false;
		}

		// checks if the current position is a softbreak AFTER check() is called
		bool is_soft_linebreak(character_rendering_iterator &it) const {
			return it.current_position() != 0 && it.current_position() == _ncs;
		}
		bool is_linebreak(character_rendering_iterator &it) const {
			return it.is_hard_line_break() || is_soft_linebreak(it);
		}
	protected:
		soft_linebreak_registry::iterator _next, _end;
		size_t _ncs;
	};
	struct view_formatter {
	public:
		view_formatter(const view_formatting &fmt, size_t sp) :
			_fold(fmt.get_folding(), sp), _lb(fmt.get_linebreaks(), sp) {
		}

		bool check(character_rendering_iterator &it) {
			_lb.check(it);
			return _fold.check(it);
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
			template <typename C> inline static _yes _test(decltype(&C::check));
			template <typename C> inline static _no _test(...);
		public:
			constexpr static bool value = sizeof(_test<T>(0)) == sizeof(_yes);
		};
	}
	template <
		typename F, typename ...Others
	> struct rendering_iterator<F, Others...> :
		public rendering_iterator<Others...> {
	public:
		template <typename FCA, typename ...OCA> rendering_iterator(FCA&&, OCA&&...);

		void begin();
		void next();

		template <typename T> T &get_addon() {
			return _get_addon_impl(_get_helper<T>());
		}

		template <size_t ...I, typename FCA, typename SCA, typename ...OCA> rendering_iterator(
			std::index_sequence<I...>, FCA &&f, SCA &&s, OCA &&...ot
		) : _direct_base(
			std::make_index_sequence<std::tuple_size_v<std::decay_t<SCA>>>(),
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
		bool _check(_check_helper<true>);
		void _switching_char(switch_char_info&, _check_helper<false>);
		void _switching_line(switch_line_info&, _check_helper<false>);
		// disabled funcs
		bool _check(_check_helper<false>) {
			return false;
		}
		void _switching_char(switch_char_info&, _check_helper<true>) {
		}
		void _switching_line(switch_line_info&, _check_helper<true>) {
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
		void _switching_line_all(switch_line_info &info) {
			_switching_line(info, _cur_check_helper());
			_direct_base::_switching_line_all(info);
		}
	};
	template <> struct rendering_iterator<> {
	public:
		rendering_iterator(const std::tuple<const editor&, size_t, size_t> &p) :
			rendering_iterator(std::get<0>(p), std::get<1>(p), std::get<2>(p)) {
		}
		rendering_iterator(const editor &ce, size_t sp, size_t pep) : _citer(ce, sp, pep) {
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
		void _switching_char_all(switch_char_info&) {
		}
		void _switching_line_all(switch_line_info&) {
		}
	};

	template <typename F, typename ...Others> template <typename FCA, typename ...OCA>
	inline rendering_iterator<F, Others...>::rendering_iterator(
		FCA &&f, OCA &&...ot
	) : rendering_iterator(
		std::make_index_sequence<std::tuple_size_v<std::decay_t<FCA>>>(),
		std::forward<FCA>(f), std::forward<OCA>(ot)...
	) {
		_root_base::char_iter().switching_char += [this](switch_char_info &pi) {
			_switching_char_all(pi);
		};
		_root_base::char_iter().switching_line += [this](switch_line_info &pi) {
			_switching_line_all(pi);
		};
	}
	template <typename F, typename ...Others> inline void rendering_iterator<F, Others...>::begin() {
		_root_base::_begin();
		while (_check_all()) {
		}
	}
	template <typename F, typename ...Others> inline void rendering_iterator<F, Others...>::next() {
		_root_base::_next();
		while (_check_all()) {
		}
	}
	template <typename F, typename ...Others>
	inline bool rendering_iterator<F, Others...>::_check(_check_helper<true>) {
		return _curaddon.check(_root_base::_citer);
	}
	template <typename F, typename ...Others> inline void rendering_iterator<F, Others...>::_switching_char(
		switch_char_info &info, _check_helper<false>
	) {
		_curaddon.switching_char(_root_base::char_iter(), info);
	}
	template <typename F, typename ...Others> inline void rendering_iterator<F, Others...>::_switching_line(
		switch_line_info &info, _check_helper<false>
	) {
		_curaddon.switching_line(_root_base::char_iter(), info);
	}
}
