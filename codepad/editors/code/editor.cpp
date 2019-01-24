// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "editor.h"

/// \file
/// Implementation of certain methods of \ref codepad::editor::code::editor.

#include "rendering.h"

using namespace std;
using namespace codepad::ui;
using namespace codepad::os;

namespace codepad::editor::code {
	/// \todo Also consider folded regions.
	vector<size_t> editor::_recalculate_wrapping_region(size_t beg, size_t end) const {
		/*rendering_token_iterator<folded_region_skipper> it(
			make_tuple(cref(_fmt.get_folding()), beg),
			make_tuple(cref(*_doc), beg)
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
		return poss;*/
		return {};
	}

	double editor::_get_caret_pos_x_unfolded_linebeg(size_t line, size_t position) const {
		size_t linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
		rendering_token_iterator<folded_region_skipper> iter(
			make_tuple(cref(_fmt.get_folding()), linebeg),
			make_tuple(cref(*_doc), linebeg)
		);
		character_metrics_accumulator metrics(get_font(), _fmt.get_tab_width());
		while (iter.get_position() < position) {
			token res = iter.generate_and_update();
			text_metrics_accumulator::measure_token(metrics, res);
		}
		return metrics.char_right();
	}

	caret_position editor::_hit_test_unfolded_linebeg(size_t line, double x) const {
		size_t linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(line).first;
		rendering_token_iterator<soft_linebreak_inserter, folded_region_skipper> iter(
			make_tuple(cref(_fmt.get_linebreaks()), linebeg),
			make_tuple(cref(_fmt.get_folding()), linebeg),
			make_tuple(cref(*_doc), linebeg)
		);
		character_metrics_accumulator metrics(get_font(), _fmt.get_tab_width());
		while (iter.get_position() < _doc->get_linebreaks().num_chars()) {
			token_generation_result res = iter.generate();
			if (holds_alternative<linebreak_token>(res.result)) { // end of the line
				// explicitly require that it's at the end of the line, rather than at the beginning of the next
				return caret_position(iter.get_position(), false);
			}
			text_metrics_accumulator::measure_token(metrics, res.result);
			if (0.5 * (metrics.char_left() + metrics.char_right()) > x) {
				return caret_position(iter.get_position(), true);
			}
			iter.update(res.steps);
		}
		return caret_position(_doc->get_linebreaks().num_chars(), true);
	}

	void editor::_on_end_edit(buffer::end_edit_info &info) {
		// fixup view
		_fmt.fixup_after_edit(info, *_doc);
		// TODO improve performance
		_fmt.set_softbreaks(_recalculate_wrapping_region(0, _doc->get_linebreaks().num_chars()));

		// fixup carets
		_adjust_recalculate_caret_char_positions(info);

		content_modified.invoke();
		_on_content_visual_changed();
	}

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
		get_manager().get_renderer().push_matrix(matd3x3::translate(vec2d(
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
			rendering_token_iterator<soft_linebreak_inserter, folded_region_skipper> it(
				make_tuple(cref(_fmt.get_linebreaks()), firstchar),
				make_tuple(cref(_fmt.get_folding()), firstchar),
				make_tuple(cref(*_doc), firstchar)
			);
			const font_family &fnt = get_font();
			text_metrics_accumulator metrics(fnt, get_line_height(), _fmt.get_tab_width());
			atlas::batch_renderer brend(fnt.normal->get_manager().get_atlas());
			caret_renderer caretrend(*used, firstchar, flineinfo.second == linebreak_type::soft);
			while (it.get_position() < plastchar) {
				token_generation_result tok = it.generate();
				// text gizmo measurement is deferred
				metrics.next<token_measurement_flags::defer_text_gizmo_measurement>(tok.result);
				if (holds_alternative<character_token>(tok.result)) {
					auto &chartok = get<character_token>(tok.result);
					if (is_graphical_char(chartok.value) && metrics.get_character().char_left() < layoutw) {
						font::character_rendering_info info = fnt.get_by_style(chartok.style)->draw_character(
							chartok.value,
							vec2d(metrics.get_character().char_left(), metrics.get_y() + bi.get(chartok.style))
						);
						brend.add_sprite(info.texture, info.placement, chartok.color);
					}
				} else if (holds_alternative<text_gizmo_token>(tok.result)) {
					// text gizmo
					auto &tgtok = get<text_gizmo_token>(tok.result);
					vec2d sz = text_renderer::render_plain_text(
						tgtok.contents,
						*(tgtok.font ? tgtok.font : fnt.normal),
						vec2d(metrics.get_character().char_right(), metrics.get_y()),
						tgtok.color
					);
					metrics.get_modify_character().next_gizmo(sz.x);
				} else if (holds_alternative<image_gizmo_token>(tok.result)) {
					// TODO
				}
				// metrics is fully updated only by now
				caretrend.on_update(it.get_base(), metrics, tok);
				it.update(tok.steps);
			}
			// render carets
			caretrend.finish(it.get_base(), metrics);
			for (const rectd &rgn : caretrend.get_caret_rects()) {
				_caret_cfg.render(get_manager().get_renderer(), rgn);
			}
			for (const auto &selrgn : caretrend.get_selection_rects()) {
				for (const auto &rgn : selrgn) {
					_sel_cfg.render(get_manager().get_renderer(), rgn);
				}
			}
		}
		get_manager().get_renderer().pop_matrix();
	}
}
