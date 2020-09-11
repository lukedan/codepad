// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/contents_region.h"

/// \file
/// Implementation of certain methods of \ref codepad::editors::code::contents_region.

#include "codepad/editors/manager.h"
#include "codepad/editors/code/fragment_generation.h"
#include "../details.h"

namespace codepad::editors::code {
	settings::retriever_parser<std::vector<std::u8string>> &contents_region::get_backup_fonts_setting(settings &set) {
		static setting<std::vector<std::u8string>> _setting(
			{ u8"editor", u8"backup_fonts" },
			settings::basic_parsers::basic_type_with_default<std::vector<std::u8string>>(
				std::vector<std::u8string>(), json::array_parser<std::u8string>()
				)
		);
		return _setting.get(set);
	}

	void contents_region::on_text_input(std::u8string_view text) {
		_interaction_manager.on_edit_operation();
		// encode added content
		byte_string str;
		const std::byte
			*it = reinterpret_cast<const std::byte*>(text.data()),
			*end = it + text.size();
		while (it != end) {
			codepoint cp;
			if (encodings::utf8::next_codepoint(it, end, cp)) {
				str.append(_doc->get_encoding()->encode_codepoint(cp));
			} else {
				logger::get().log_warning(CP_HERE) << "skipped invalid byte sequence in input";
			}
		}
		_doc->on_insert(_cset, str, this);
	}

	/// \todo Also consider folded regions.
	/// \todo Word wrapping not implemented.
	std::vector<size_t> contents_region::_recalculate_wrapping_region(size_t, size_t) const {
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
			*get_document(), get_font_families(), linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), linebeg)
		);
		fragment_assembler ass(*this);
		while (iter.get_position() < position) {
			fragment_generation_result res = iter.generate_and_update();
			if (iter.get_position() > position) {
				if (holds_alternative<text_fragment>(res.result)) {
					fragment_assembler::text_rendering rendering = ass.append(get<text_fragment>(res.result));
					return rendering.topleft.x + rendering.text->get_character_placement(
						position - (iter.get_position() - res.steps)
					).xmin;
				}
				// FIXME the cursor is inside some object, for now just return the end position
				return ass.get_horizontal_position();
			}
			std::visit([&ass](auto &&frag) {
				ass.append(frag);
			}, res.result);
			if (iter.get_position() == position) {
				return ass.get_horizontal_position();
			}
		}
		return ass.get_horizontal_position();
	}

	caret_position contents_region::_hit_test_at_visual_line(std::size_t line, double x) const {
		std::size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
		fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> iter(
			*get_document(), get_font_families(), linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), linebeg)
		);
		fragment_assembler ass(*this);
		while (iter.get_position() < _doc->get_linebreaks().num_chars()) {
			std::size_t oldpos = iter.get_position();
			fragment_generation_result res = iter.generate_and_update();
			if (holds_alternative<linebreak_fragment>(res.result)) { // end of the line
				// explicitly require that it's at the end of the line, rather than at the beginning of the next
				return caret_position(oldpos, false);
			}
			caret_position respos;
			bool has_result = visit([this, &iter, &ass, &res, &respos, x](auto &&frag) -> bool {
				auto &&rendering = ass.append(frag);
				if (ass.get_horizontal_position() > x) {
					if constexpr (std::is_same_v<std::decay_t<decltype(frag)>, text_fragment>) {
						ui::caret_hit_test_result htres = rendering.text->hit_test(x - rendering.topleft.x);
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

		_on_content_modified();
	}

	void contents_region::_custom_render() const {
		interactive_contents_region_base::_custom_render();

		performance_monitor mon(u8"render_contents");

		auto &renderer = get_manager().get_renderer();
		double lh = get_line_height();
		std::pair<std::size_t, std::size_t> be = get_visible_visual_lines();

		caret_set extcarets;
		const caret_set *used = &_cset;
		std::vector<caret_selection_position> tempcarets = _interaction_manager.get_temporary_carets();
		if (!tempcarets.empty()) {
			extcarets = _cset;
			for (const auto &caret : tempcarets) {
				extcarets.add(caret_set::entry(
					caret.get_caret_selection(), caret_data(0.0, caret.caret_at_back)
				));
			}
			used = &extcarets;
		}

		{
			renderer.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));
			renderer.push_matrix_mult(matd3x3::translate(vec2d(
				get_padding().left,
				get_padding().top - editor::get_encapsulating(*this)->get_vertical_position() +
				static_cast<double>(be.first) * lh
			)));

			// parameters
			auto flineinfo = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(be.first)
			);
			std::size_t
				firstchar = flineinfo.first,
				plastchar = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
					_fmt.get_folding().folded_to_unfolded_line_number(be.second)
				).first,
				curvisline = be.first;

			// rendering facilities
			fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> gen(
				*get_document(), get_font_families(), firstchar,
				soft_linebreak_inserter(_fmt.get_linebreaks(), firstchar),
				folded_region_skipper(_fmt.get_folding(), firstchar)
			);
			fragment_assembler ass(*this);
			caret_gatherer caretrend(used->carets, firstchar, ass, flineinfo.second == linebreak_type::soft);

			// render text & gather information for carets
			while (gen.get_position() < plastchar) {
				fragment_generation_result frag = gen.generate_and_update();
				// render the fragment
				std::visit([this, &frag, &gen, &ass, &caretrend](auto &&specfrag) {
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

			caretrend.finish(gen.get_position());
			// render carets
			vec2d unit = get_layout().size();
			for (const auto &selrgn : caretrend.get_selection_rects()) {
				code_selection_renderer()->render(
					renderer, selrgn, _selection_brush.get_parameters(unit), _selection_pen.get_parameters(unit)
				);
			}
			for (const rectd &rgn : caretrend.get_caret_rects()) {
				_caret_visuals.render(rgn, renderer);
			}

			renderer.pop_matrix();
			renderer.pop_clip();
		}
	}

	void contents_region::_initialize(std::u8string_view cls) {
		_base::_initialize(cls);

		std::vector<std::u8string> profile; // TODO custom profile

		auto &renderer = get_manager().get_renderer();
		auto &set = get_manager().get_settings();
		std::vector<std::unique_ptr<ui::font_family>> families;
		families.emplace_back(renderer.find_font_family(
			editor::get_font_family_setting(set).get_profile(profile.begin(), profile.end()).get_value()
		));
		auto &backups = get_backup_fonts_setting(set).get_profile(profile.begin(), profile.end()).get_value();
		for (const auto &f : backups) {
			auto family = renderer.find_font_family(f);
			if (family) {
				families.emplace_back(std::move(family));
			} else {
				logger::get().log_info(CP_HERE) << "font family not found: " << f;
			}
		}
		set_font_families(std::move(families));

		set_font_size_and_line_height(
			editor::get_font_size_setting(set).get_profile(profile.begin(), profile.end()).get_value()
		);

		_interaction_manager.set_contents_region(*this);
		auto &modes = editor::get_interaction_modes_setting(set).get_profile(
			profile.begin(), profile.end()
		).get_value();
		for (const auto &mode_name : modes) {
			if (auto mode = editors::_details::get_manager().code_interactions.try_create(mode_name)) {
				_interaction_manager.activators().emplace_back(std::move(mode));
			}
		}
	}
}