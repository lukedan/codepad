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
	vector<size_t> contents_region::_recalculate_wrapping_region(size_t, size_t) const {
		/*
		vector<size_t> poss;
		size_t last = 0;

		fragment_generator<folded_region_skipper> iter(
			make_tuple(cref(_fmt.get_folding()), beg),
			make_tuple(cref(*_doc), beg)
		);
		text_metrics_accumulator metrics(get_font(), get_line_height(), _fmt.get_tab_width());

		while (iter.get_position() < end) {
			fragment_generation_result res = iter.generate();
			metrics.next<>(res.result);
			if (iter.get_position() > last && metrics.get_character().char_right() > _view_width) { // wrap
				poss.emplace_back(iter.get_position()); // TODO stalls not taken into account
				metrics.next_line();
				metrics.next<>(res.result); // TODO better way to re-measure the fragment
				last = iter.get_position();
			}
			iter.update(res.steps);
		}
		return poss;
		*/
		return {};
	}

	double contents_region::_get_caret_pos_x_at_visual_line(size_t line, size_t position) const {
		size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
		fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> iter(
			*_doc, linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), linebeg)
		);
		fragment_assembler ass(*this);
		while (iter.get_position() < position) {
			fragment_generation_result res = iter.generate_and_update();
			if (iter.get_position() >= position) {
				if (holds_alternative<text_fragment>(res.result)) {
					fragment_assembler::text_rendering rendering = ass.append(get<text_fragment>(res.result));
					return rendering.topleft.x + rendering.text->get_character_placement(
						position - (iter.get_position() - res.steps)
					).xmin;
				}
				return
					ass.get_horizontal_position();
			}
			visit([&ass](auto && frag) {
				ass.append(frag);
				}, res.result);
		}
		return ass.get_horizontal_position();
	}

	settings::retriever_parser<double> &contents_region::_get_font_size_setting() {
		static settings::retriever_parser<double> _setting = settings::get().create_retriever_parser<double>(
			{u8"editor", u8"font_size"}, settings::basic_parsers::basic_type_with_default<double>(12.0)
		);
		return _setting;
	}

	caret_position contents_region::_hit_test_at_visual_line(size_t line, double x) const {
		size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
		fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> iter(
			*_doc, linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), linebeg)
		);
		fragment_assembler ass(*this);
		while (iter.get_position() < _doc->get_linebreaks().num_chars()) {
			size_t oldpos = iter.get_position();
			fragment_generation_result res = iter.generate_and_update();
			if (holds_alternative<linebreak_fragment>(res.result)) { // end of the line
				// explicitly require that it's at the end of the line, rather than at the beginning of the next
				return caret_position(oldpos, false);
			}
			caret_position respos;
			bool has_result = visit([this, &iter, &ass, &res, &respos, x](auto && frag) -> bool {
				auto &&rendering = ass.append(frag);
				if (ass.get_horizontal_position() > x) {
					if constexpr (is_same_v<decay_t<decltype(frag)>, text_fragment>) {
						formatted_text::hit_test_result htres = rendering.text->hit_test(vec2d(
							x - rendering.topleft.x, 0.5 * get_line_height()
						));
						respos.position = iter.get_position() - res.steps;
						respos.position += htres.rear ? htres.character + 1 : htres.character;
						respos.at_back = true;
						return true;
					} else {
						if (x < 0.5 * (rendering.topleft.x + ass.get_horizontal_position())) {
							respos.position = iter.get_position() - res.steps;
							respos.at_back = true;
							return true;
						}
					}
				}
				return false;
				}, res.result);
			if (has_result) {
				return respos;
			}
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

	void contents_region::_custom_render() const {
		interactive_contents_region_base::_custom_render();

		performance_monitor mon(CP_STRLIT("render_contents"));
		double lh = get_line_height();
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

		get_manager().get_renderer().push_rectangle_clip(
			rectd::from_xywh(0.0, 0.0, get_layout().width(), get_layout().height())
		);
		get_manager().get_renderer().push_matrix_mult(matd3x3::translate(vec2d(
			get_padding().left,
			get_padding().top - editor::get_encapsulating(*this)->get_vertical_position() +
			static_cast<double>(be.first) * lh
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

			// rendering facilities
			fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> gen(
				*_doc, firstchar,
				soft_linebreak_inserter(_fmt.get_linebreaks(), firstchar),
				folded_region_skipper(_fmt.get_folding(), firstchar)
			);
			fragment_assembler ass(*this);
			caret_gatherer caretrend(used->carets, firstchar, ass, flineinfo.second == linebreak_type::soft);
			renderer_base & rend = get_manager().get_renderer();

			// actual rendering code
			while (gen.get_position() < plastchar) {
				fragment_generation_result frag = gen.generate_and_update();
				// render the fragment
				std::visit([this, &frag, &gen, &ass, &caretrend](auto && specfrag) {
					auto &&rendering = ass.append(specfrag);
					ass.render(get_manager().get_renderer(), rendering);
					caretrend.handle_fragment(specfrag, rendering, frag.steps, gen.get_position());
					}, frag.result);
				if (std::holds_alternative<linebreak_fragment>(frag.result)) {
					++curvisline;
				} else if (ass.get_horizontal_position() + get_padding().left > get_layout().width()) {
					// skip to the next line
					++curvisline;
					auto pos = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().folded_to_unfolded_line_number(curvisline)
					);
					// update caret renderer
					caretrend.skip_line(pos.second == linebreak_type::soft, pos.first);
					gen.reposition(pos.first); // reposition fragment generator
					// update fragment assenbler
					ass.set_horizontal_position(0.0);
					ass.advance_vertical_position(1);
				}
			}

			// render carets
			// TODO
			caretrend.finish(gen.get_position());
			for (const rectd &rgn : caretrend.get_caret_rects()) {
				rend.draw_rectangle(
					rgn,
					ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord(1.0, 1.0, 1.0, 0.3))),
					ui::generic_pen_parameters(ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord(1.0, 1.0, 1.0, 1.0))))
				);
			}
			rounded_selection_renderer rcrend;
			for (const auto &selrgn : caretrend.get_selection_rects()) {
				rcrend.render(
					rend, selrgn,
					ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord(0.2, 0.2, 1.0, 0.3))),
					ui::generic_pen_parameters(ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord(0.0, 0.0, 0.0, 1.0))))
				);
			}
		}
		get_manager().get_renderer().pop_matrix();
		get_manager().get_renderer().pop_clip();
	}
}
