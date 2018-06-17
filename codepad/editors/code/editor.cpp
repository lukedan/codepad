#include "editor.h"

/// \file
/// Implementation of certain methods of \ref codepad::editor::code::editor.

#include "rendering.h"

using namespace std;
using namespace codepad::ui;
using namespace codepad::os;

namespace codepad::editor::code {
	void editor::set_document(shared_ptr<document> nctx) {
		if (_doc) {
			_doc->modified -= _ctx_mod_tok;
			_doc->visual_changed -= _ctx_vis_change_tok;
		}
		_doc = move(nctx);
		_cset.reset();
		if (_doc) {
			_ctx_mod_tok = (_doc->modified += [this](modification_info &info) {
				_on_content_modified(info);
				});
			_ctx_vis_change_tok = (_doc->visual_changed += [this]() {
				_on_content_visual_changed();
				});
			_fmt = _doc->create_view_formatting();
		} else { // empty document, only used when the editor's being disposed
			_fmt = view_formatting();
		}
		_on_context_switch();
	}

	/// \todo Cannot deal with very long lines.
	void editor::_custom_render() {
		performance_monitor mon(CP_HERE);

		double lh = get_line_height(), layoutw = get_layout().width();
		pair<size_t, size_t> be = get_visible_lines();
		caret_set::container ncon;
		const caret_set::container *used = &_cset.carets;
		if (_selecting) { // when selecting, use temporarily merged caret set for rendering
			ncon = *used;
			used = &ncon;
			caret_set::add_caret(
				ncon, {{_mouse_cache.position, _sel_end}, caret_data(0.0, _mouse_cache.at_back)}
			);
		}
		font_family::baseline_info bi = get_font().get_baseline_info();

		rectd client = get_client_region();
		renderer_base::get().push_matrix(matd3x3::translate(vec2d(
			round(client.xmin),
			round(
				client.ymin - _get_box()->get_vertical_position() +
				static_cast<double>(_fmt.get_folding().unfolded_to_folded_line_number(be.first)) * lh
			)
		)));
		{ // matrix pushed
			auto flineinfo = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.first);
			size_t
				firstchar = flineinfo.first,
				plastchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(be.second).first;
			rendering_iterator<view_formatter, caret_renderer> it(
				make_tuple(cref(_fmt), firstchar),
				make_tuple(cref(*used), firstchar, flineinfo.second == linebreak_type::soft),
				make_tuple(cref(*this), firstchar, plastchar)
			);
			for (it.begin(); !it.ended(); it.next()) {
				const character_rendering_iterator &cri = it.char_iter();
				if (!cri.is_hard_line_break()) {
					const character_metrics_accumulator &lci = cri.character_info();
					const text_theme_data::char_iterator &ti = cri.theme_info();
					if (is_graphical_char(lci.current_char())) { // render character
						vec2d xpos =
							vec2d(lci.char_left(), round(cri.rounded_y_offset() + bi.get(ti.current_theme.style))) +
							lci.current_char_entry().placement.xmin_ymin();
						if (xpos.x < layoutw) { // only render characters that are visible
							renderer_base::get().draw_character(
								lci.current_char_entry().texture, xpos, ti.current_theme.color
							);
						}
					}
				}
			}
			// render carets
			caret_renderer &caretrend = it.get_addon<caret_renderer>();
			caretrend.finish(it.char_iter());
			_selst.update();
			for (const auto &selrgn : caretrend.get_selection_rects()) {
				for (const auto &rgn : selrgn) {
					_selst.render(rgn);
				}
			}
			if (!_selst.stationary()) {
				invalidate_visual();
			}
			if (_caretst.update_and_render_multiple(it.get_addon<caret_renderer>().get_caret_rects())) {
				invalidate_visual();
			}
		}
		renderer_base::get().pop_matrix();
	}

	/// \todo Also consider folded regions.
	vector<size_t> editor::_recalculate_wrapping_region(size_t beg, size_t end) const {
		rendering_iterator<fold_region_skipper> it(
			make_tuple(cref(_fmt.get_folding()), beg),
			make_tuple(cref(*this), beg, _doc->num_chars())
		);
		vector<size_t> poss;
		bool lb = true; // used to ensure that soft linebreaks are not in the place of hard linebreaks
		it.char_iter().switching_char += [this, &it, &lb, &poss](switch_char_info &info) {
			// check for folded regions
			if (info.is_jump) {
				// TODO using char_left may be inaccurate, but using prev_char_right cannot handle consecutive
				//      folded regions
				// TODO magic number
				if (!lb && it.char_iter().character_info().char_left() + 30.0 > _view_width) {
					it.char_iter().insert_soft_linebreak();
					poss.push_back(it.char_iter().current_position());
				}
				lb = false;
			}
		};
		for (
			it.begin();
			!it.ended() && (
				it.char_iter().current_position() <= end ||
				!it.char_iter().is_hard_line_break()
				);
			it.next()
			) { // iterate over the range of text
			if (
				!lb && !it.char_iter().is_hard_line_break() &&
				it.char_iter().character_info().char_right() > _view_width
				) {
				it.char_iter().insert_soft_linebreak(); // insert softbreak before current char
				poss.push_back(it.char_iter().current_position());
				lb = false;
			} else {
				// if the current character is a hard linebreak, inserting a soft linebreak when at the next char
				// will actually put that soft linebreak at the same position
				lb = it.char_iter().is_hard_line_break();
			}
		}
		return poss;
	}
	double editor::_get_caret_pos_x_unfolded_linebeg(size_t line, size_t position) const {
		size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
		rendering_iterator<fold_region_skipper> iter(
			make_tuple(cref(_fmt.get_folding()), begc),
			make_tuple(cref(*this), begc, _doc->num_chars())
		);
		for (iter.begin(); iter.char_iter().current_position() < position; iter.next()) {
		}
		return iter.char_iter().character_info().prev_char_right();
	}

	caret_position editor::_hit_test_unfolded_linebeg(size_t line, double x) const {
		size_t begc = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
		rendering_iterator<view_formatter> iter(
			make_tuple(cref(_fmt), begc),
			make_tuple(cref(*this), begc, _doc->num_chars())
		);
		bool stop = false;
		caret_position result;
		iter.char_iter().switching_char += [&](switch_char_info &pi) {
			if (!stop && pi.is_jump) { // handle jumps (folded regions)
				// the end horizontal position of the jump is unclear yet
				if (x < iter.char_iter().character_info().char_left() + 15.0) { // TODO magic number
					result = caret_position(iter.char_iter().current_position(), true);
					stop = true;
				}
			}
		};
		iter.char_iter().switching_line += [&](switch_line_info&) {
			if (!stop) { // stop whenever a linebreak is encountered
				result = caret_position(iter.char_iter().current_position(), false);
				stop = true;
			}
		};
		for (iter.begin(); !iter.ended(); iter.next()) {
			if (stop) {
				return result;
			}
			double midv = 0.5 * (
				iter.char_iter().character_info().char_left() + iter.char_iter().character_info().char_right()
				);
			if (x < midv) {
				// on the left of the character, so the second parameter is always true
				return caret_position(iter.char_iter().current_position(), true);
			}
		}
		// late stop, or the end of the document
		return stop ? result : caret_position(iter.char_iter().current_position(), true);
	}

	void editor::_on_content_modified(modification_info &mi) {
		// fixup view
		// TODO improve performance
		_fmt.fixup_folding_positions(mi.caret_fixup);
		_fmt.set_softbreaks(_recalculate_wrapping_region(0, _doc->num_chars()));

		if (mi.source != this) { // fixup carets
			caret_fixup_info::context cctx(mi.caret_fixup);
			caret_set nset;
			for (auto i = _cset.carets.begin(); i != _cset.carets.end(); ++i) {
				bool cfront = i->first.first < i->first.second;
				caret_selection cs(i->first.first, i->first.second);
				if (!cfront) {
					swap(cs.first, cs.second);
				}
				cs.first = mi.caret_fixup.fixup_position_max(cs.first, cctx);
				cs.second = mi.caret_fixup.fixup_position_min(cs.second, cctx);
				if (cs.first > cs.second) {
					cs.second = cs.first;
				}
				if (!cfront) {
					swap(cs.first, cs.second);
				}
				nset.add(caret_set::entry(cs, caret_data(0.0, i->second.softbreak_next_line)));
			}
			set_carets(std::move(nset));
		}

		content_modified.invoke();
		_on_content_visual_changed();
	}
}
