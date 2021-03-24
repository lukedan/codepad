// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/contents_region.h"

/// \file
/// Implementation of certain methods of \ref codepad::editors::code::contents_region.

#include <array>

#include "codepad/editors/manager.h"
#include "codepad/editors/code/fragment_generation.h"
#include "codepad/editors/code/caret_gatherer.h"
#include "codepad/editors/code/decoration_gatherer.h"
#include "codepad/editors/code/whitespace_gatherer.h"
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

	/// \todo Word wrapping not implemented.
	std::vector<std::size_t> contents_region::_recalculate_wrapping_region(std::size_t, std::size_t) const {
		/*
		vector<std::size_t> poss;
		std::size_t last = 0;

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

	double contents_region::_get_caret_pos_x_at_visual_line(std::size_t line, std::size_t position) const {
		std::size_t
			linebeg = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
				_fmt.get_folding().folded_to_unfolded_line_number(line)
			).first;
		fragment_generator<fragment_generator_component_hub<soft_linebreak_inserter, folded_region_skipper>> iter(
			get_document(), get_invalid_codepoint_fragment_func(),
			get_font_families(), get_text_theme(), linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), get_folded_fragment_function(), linebeg)
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
			get_document(), get_invalid_codepoint_fragment_func(),
			get_font_families(), get_text_theme(), linebeg,
			soft_linebreak_inserter(_fmt.get_linebreaks(), linebeg),
			folded_region_skipper(_fmt.get_folding(), get_folded_fragment_function(), linebeg)
		);
		fragment_assembler ass(*this);
		while (iter.get_position() < _doc->get_linebreaks().num_chars()) {
			std::size_t oldpos = iter.get_position();
			fragment_generation_result res = iter.generate_and_update();
			if (holds_alternative<linebreak_fragment>(res.result)) { // end of the line
				// explicitly require that it's at the end of this line, rather than at the beginning of the next
				return caret_position(oldpos, false);
			}
			caret_position respos;
			bool has_result = visit([this, &iter, &ass, &res, &respos, x](auto &&frag) -> bool {
				auto &&rendering = ass.append(frag);
				if (ass.get_horizontal_position() > x) {
					if constexpr (std::is_same_v<std::decay_t<decltype(frag)>, text_fragment>) {
						if (!frag.is_gizmo) {
							ui::caret_hit_test_result htres = rendering.text->hit_test(x - rendering.topleft.x);
							respos.position = iter.get_position() - res.steps;
							respos.position += htres.rear ? htres.character + 1 : htres.character;
							respos.at_back = true;
							return true;
						}
					}
					if (x < 0.5 * (rendering.topleft.x + ass.get_horizontal_position())) {
						respos.position = iter.get_position() - res.steps;
						respos.at_back = true;
						return true;
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
				get_padding().top - get_editor().get_vertical_position() + static_cast<double>(be.first) * lh
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
				get_document(), get_invalid_codepoint_fragment_func(),
				get_font_families(), get_text_theme(), firstchar,
				soft_linebreak_inserter(_fmt.get_linebreaks(), firstchar),
				folded_region_skipper(_fmt.get_folding(), get_folded_fragment_function(), firstchar)
			);
			fragment_assembler ass(*this);
			caret_gatherer caretrend(used->carets, firstchar, ass, flineinfo.second == linebreak_type::soft);
			whitespace_gatherer whitespaces(*used, firstchar, ass);

			// decorations
			decoration_gatherer deco_gather(get_document().get_decoration_providers(), firstchar, ass);
			std::vector<std::pair<decoration_layout, decoration_renderer*>> decorations;
			deco_gather.render_callback = [&](decoration_layout layout, decoration_renderer *deco_renderer) {
				decorations.emplace_back(std::move(layout), deco_renderer);
			};

			// gather information for text and carets
			std::vector<fragment_assembler::rendering_storage> renderings;
			while (gen.get_position() < plastchar) {
				fragment_generation_result frag = gen.generate_and_update();

				// render the fragment
				std::visit(
					[&, this](auto &&specfrag) {
						auto rendering = ass.append(specfrag);

						caretrend.handle_fragment(specfrag, rendering, frag.steps, gen.get_position());
						deco_gather.handle_fragment(specfrag, rendering, frag.steps, gen.get_position());
						whitespaces.handle_fragment(specfrag, rendering, frag.steps, gen.get_position());

						renderings.emplace_back(std::move(rendering));
					},
					frag.result
				);

				if (std::holds_alternative<linebreak_fragment>(frag.result)) {
					++curvisline;
				} else if (ass.get_horizontal_position() + get_padding().left > get_layout().width()) {
					// skip to the next line
					++curvisline;
					auto pos = _fmt.get_linebreaks().get_beginning_char_of_visual_line(
						_fmt.get_folding().folded_to_unfolded_line_number(curvisline)
					);
					// update gatherers
					caretrend.skip_line(pos.second == linebreak_type::soft, pos.first);
					deco_gather.skip_line(pos.first);
					// reposition fragment generator
					gen.reposition(pos.first);
					// update fragment assenbler
					ass.set_horizontal_position(0.0);
					ass.advance_vertical_position(1);
				}
			}
			caretrend.finish(gen.get_position());
			deco_gather.finish();

			// render carets & selections
			vec2d unit = get_layout().size();
			if (code_selection_renderer()) {
				for (const auto &selrgn : caretrend.get_selection_rects()) {
					code_selection_renderer()->render(renderer, selrgn, unit);
				}
			}
			for (const rectd &rgn : caretrend.get_caret_rects()) {
				_caret_visuals.render(rgn, renderer);
			}

			// render decorations
			for (const auto &[layout, rend] : decorations) {
				if (rend) {
					rend->render(renderer, layout, unit);
				}
			}

			// render text
			for (auto &rendering : renderings) {
				std::visit([&](auto &concrete_rendering) {
					fragment_assembler::render(renderer, concrete_rendering);
				}, rendering);
			}

			// render whitespaces
			std::array<
				const ui::generic_visual_geometry*,
				static_cast<std::size_t>(whitespace_gatherer::whitespace::type::max_count)
			> whitespace_geometries{
				&_whitespace_geometry, &_tab_geometry,
				&_crlf_geometry, &_cr_geometry, &_lf_geometry
			};
			for (auto &ws : whitespaces.whitespaces) {
				auto *geom = whitespace_geometries[static_cast<std::size_t>(ws.whitespace_type)];
				renderer.push_matrix_mult(matd3x3::translate(ws.placement.xmin_ymin()));
				geom->draw(ws.placement.size(), renderer);
				renderer.pop_matrix();
			}

			renderer.pop_matrix();
			renderer.pop_clip();
		}
	}

	void contents_region::_initialize() {
		interactive_contents_region_base::_initialize();

		std::vector<std::u8string> profile; // TODO custom profile

		auto &renderer = get_manager().get_renderer();
		auto &set = get_manager().get_settings();
		std::vector<std::shared_ptr<ui::font_family>> families;
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

		// TODO customizability
		_fold_fragment_func = [this](const folding_registry::iterator&) {
			return text_fragment::gizmo_from_utf8(u8"...", colord(0.8, 0.8, 0.8, 1.0), get_font_families()[0]->get_matching_font(
				ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
			));
		};
		_invalid_cp_func = [this](codepoint cp) {
			return text_fragment::gizmo_from_utf8(format_invalid_codepoint(cp), colord(1.0, 0.2, 0.2, 1.0), get_font_families()[0]->get_matching_font(
				ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
			));
		};
	}

	ui::property_info contents_region::_find_property_path(const ui::property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"code_contents_region")) {
			if (path.front().property == u8"whitespace_geometry") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region::_whitespace_geometry, element
				>(path, get_manager(), ui::property_info::make_typed_modification_callback<element>([](element &e) {
					e.invalidate_visual();
				}));
			}
			if (path.front().property == u8"tab_geometry") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region::_tab_geometry, element
				>(path, get_manager(), ui::property_info::make_typed_modification_callback<element>([](element &e) {
					e.invalidate_visual();
				}));
			}
			if (path.front().property == u8"crlf_geometry") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region::_crlf_geometry, element
				>(path, get_manager(), ui::property_info::make_typed_modification_callback<element>([](element &e) {
					e.invalidate_visual();
				}));
			}
			if (path.front().property == u8"cr_geometry") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region::_cr_geometry, element
				>(path, get_manager(), ui::property_info::make_typed_modification_callback<element>([](element &e) {
					e.invalidate_visual();
				}));
			}
			if (path.front().property == u8"lf_geometry") {
				return ui::property_info::find_member_pointer_property_info_managed<
					&contents_region::_lf_geometry, element
				>(path, get_manager(), ui::property_info::make_typed_modification_callback<element>([](element &e) {
					e.invalidate_visual();
				}));
			}
		}
		return interactive_contents_region_base::_find_property_path(path);
	}
}
