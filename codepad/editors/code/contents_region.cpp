// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "contents_region.h"

/// \file
/// Implementation of certain methods of \ref codepad::editors::code::contents_region.

#include "rendering.h"

using namespace std;
using namespace codepad::ui;
using namespace codepad::os;

namespace codepad::editors::code {
	/// \todo Also consider folded regions.
	vector<size_t> contents_region::_recalculate_wrapping_region(size_t beg, size_t end) const {
		vector<size_t> poss;
		size_t last = 0;

		rendering_token_iterator<folded_region_skipper> iter(
			make_tuple(cref(_fmt.get_folding()), beg),
			make_tuple(cref(*_doc), beg)
		);
		text_metrics_accumulator metrics(get_font(), get_line_height(), _fmt.get_tab_width());

		while (iter.get_position() < end) {
			token_generation_result res = iter.generate();
			metrics.next<>(res.result);
			if (iter.get_position() > last && metrics.get_character().char_right() > _view_width) { // wrap
				poss.emplace_back(iter.get_position()); // TODO stalls not taken into account
				metrics.next_line();
				metrics.next<>(res.result); // TODO better way to re-measure the token
				last = iter.get_position();
			}
			iter.update(res.steps);
		}
		return poss;
	}

	double contents_region::_get_caret_pos_x_at_visual_line(size_t line, size_t position) const {
		size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
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

	caret_position contents_region::_hit_test_at_visual_line(size_t line, double x) const {
		size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
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

	void contents_region::_on_end_edit(buffer::end_edit_info &info) {
		// fixup view
		_fmt.fixup_after_edit(info, *_doc);
		// TODO improve performance
		_fmt.set_softbreaks(_recalculate_wrapping_region(0, _doc->get_linebreaks().num_chars()));

		// fixup carets
		_adjust_recalculate_caret_char_positions(info);

		content_modified.invoke();
		_on_content_visual_changed();
	}

	void contents_region::_custom_render() {
		performance_monitor mon(CP_STRLIT("render_contents"));

		double lh = get_line_height(), layoutw = get_layout().width();
		pair<size_t, size_t> be = get_visible_visual_lines();
		caret_set extcarets;
		const caret_set *used = &_cset;
		std::vector<caret_selection_position> tempcarets = _interaction_manager.get_temporary_carets();
		if (!tempcarets.empty()) {
			extcarets = _cset;
			for (const auto &caret : tempcarets) {
				extcarets.add(caret_set::entry(
					caret_selection(caret.caret, caret.selection), caret_data(0.0, caret.caret_at_back)
				));
			}
			used = &extcarets;
		}
		font_family::baseline_info bi = get_font().get_baseline_info();

		rectd client = get_client_region();
		get_manager().get_renderer().push_matrix_mult(matd3x3::translate(vec2d(
			round(client.xmin),
			round(
				client.ymin - editor::get_encapsulating(*this)->get_vertical_position() +
				static_cast<double>(be.first) * lh
			)
		)));
		{ // matrix pushed
			// parameters
			auto flineinfo = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(be.first)
			);
			size_t
				firstchar = flineinfo.first,
				plastchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
					_fmt.get_folding().folded_to_unfolded_line_number(be.second)
				).first,
				curvisline = be.first;
			const font_family &fnt = get_font();

			// rendering facilities
			rendering_token_iterator<soft_linebreak_inserter, folded_region_skipper> it(
				make_tuple(cref(_fmt.get_linebreaks()), firstchar),
				make_tuple(cref(_fmt.get_folding()), firstchar),
				make_tuple(cref(*_doc), firstchar)
			);
			text_metrics_accumulator metrics(fnt, get_line_height(), _fmt.get_tab_width());
			atlas::batch_renderer brend(fnt.normal->get_manager().get_atlas());
			caret_renderer caretrend(used->carets, firstchar, flineinfo.second == linebreak_type::soft);

			// actual rendering code
			while (it.get_position() < plastchar) {
				token_generation_result tok = it.generate();
				// text gizmo measurement is deferred
				metrics.next<token_measurement_flags::defer_text_gizmo_measurement>(tok.result);
				// render the token
				if (holds_alternative<character_token>(tok.result)) {
					auto &chartok = get<character_token>(tok.result);
					if (is_graphical_char(chartok.value) && metrics.get_character().char_left() < layoutw) {
						font::character_rendering_info info = fnt.get_by_style(chartok.style)->draw_character(
							chartok.value,
							vec2d(metrics.get_character().char_left(), metrics.get_y() + bi.get(chartok.style))
						);
						brend.add_sprite(info.texture, info.placement, chartok.color);
					}
				} else if (holds_alternative<text_gizmo_token>(tok.result)) { // text gizmo
					auto &tgtok = get<text_gizmo_token>(tok.result);
					vec2d sz = text_renderer::render_plain_text(
						tgtok.contents, *(tgtok.font ? tgtok.font : fnt.normal),
						vec2d(metrics.get_character().char_right(), metrics.get_y()), tgtok.color,
						brend
					);
					metrics.get_modify_character().next_gizmo(sz.x);
				} else if (holds_alternative<image_gizmo_token>(tok.result)) {
					// TODO
				} else if (holds_alternative<linebreak_token>(tok.result)) { // update line index
					++curvisline;
				}
				// metrics are fully updated only by now
				caretrend.on_update(it.get_base(), metrics, tok);
				/*if (metrics.get_character().char_right() > get_layout().width() - get_padding().left) {
					// skip to the next line
					++curvisline;
					auto pos = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().folded_to_unfolded_line_number(curvisline)
					);
					it.reposition(pos.first);
					metrics.next_line();
					// TODO update carets
				} else {
				}*/
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
