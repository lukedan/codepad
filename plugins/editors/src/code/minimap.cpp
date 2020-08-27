// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/minimap.h"
#include "codepad/ui/json_parsers.h"

/// \file
/// Implementation of the minimap.

namespace codepad::editors::code {
	double editors::code::minimap::_target_height = 2.0; // TODO turn this into a setting

	void minimap::_page_cache::restart() {
		pages.clear();
		if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
			std::pair<std::size_t, std::size_t> be = _parent->_get_visible_visual_lines();
			double slh = edt->get_line_height() * _parent->get_scale();
			std::size_t
				numlines = edt->get_num_visual_lines(),
				pgsize = std::max(
					be.second - be.first, static_cast<std::size_t>(minimum_page_size / slh) + 1
				),
				page_beg = 0;
			_page_end = numlines;
			if (pgsize < numlines) { // the viewport is smaller than one page
				if (be.first + be.second < pgsize) { // at the top
					_page_end = pgsize;
				} else if (be.first + be.second + pgsize > numlines * 2) { // at the bottom
					page_beg = numlines - pgsize;
				} else { // middle
					page_beg = (be.first + be.second - pgsize) / 2;
					_page_end = page_beg + pgsize;
				}
			}
			_render_page(page_beg, _page_end); // render the visible page
		}
	}

	void minimap::_page_cache::prepare() {
		if (_ready) {
			return;
		}
		if (pages.empty()) {
			restart();
		} else {
			if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
				std::pair<std::size_t, std::size_t> be = _parent->_get_visible_visual_lines();
				std::size_t page_beg = pages.begin()->first;
				if (be.first >= page_beg && be.second <= _page_end) { // all are visible
					return;
				}
				std::size_t min_page_lines = static_cast<std::size_t>(
					minimum_page_size / (edt->get_line_height() * _parent->get_scale())
					) + 1,
					// the number of lines in the page about to be rendered
					page_lines = std::max(be.second - be.first, min_page_lines);
				if (be.first + page_lines < page_beg || _page_end + page_lines < be.second) {
					// too far away from already rendered region, reset cache
					restart();
				} else {
					if (be.first < page_beg) { // render one page before the first one
						// if the page before is not large enough, make it as large as min_page_lines
						std::size_t frontline = std::max(page_beg, min_page_lines) - min_page_lines;
						// at least the first visible line is rendered
						_render_page(std::min(be.first, frontline), page_beg);
					}
					if (be.second > _page_end) { // render one page after the last one
						// if not large enough, make it as large as min_page_lines
						std::size_t backline = std::min(edt->get_num_visual_lines(), _page_end + min_page_lines);
						// at least the last visible line is rendered
						backline = std::max(be.second, backline);
						_render_page(_page_end, backline);
						_page_end = backline; // set _page_end
					}
				}
			}
		}
		_ready = true;
	}

	void minimap::_page_cache::_render_page(std::size_t s, std::size_t pe) {
		ui::window_base *wnd = _parent->get_window();
		if (wnd == nullptr) { // we need the scale factor from the window
			return;
		}

		performance_monitor mon(u8"render_minimap_page", page_rendering_time_redline);
		if (contents_region *edt = component_helper::get_contents_region(*_parent)) {
			double lh = edt->get_line_height(), scale = _parent->get_scale();

			ui::renderer_base &r = _parent->get_manager().get_renderer();
			ui::render_target_data rt = r.create_render_target(
				vec2d( // add 1 because the starting position was floored instead of rounded
					_width, std::ceil(lh * scale * static_cast<double>(pe - s)) + 1
				),
				wnd->get_scaling_factor(),
				colord(1.0, 1.0, 1.0, 0.0)
			);

			const view_formatting &fmt = edt->get_formatting();
			std::size_t
				curvisline = s,
				firstchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
					fmt.get_folding().folded_to_unfolded_line_number(s)
				).first,
				plastchar = fmt.get_linebreaks().get_beginning_char_of_visual_line(
					fmt.get_folding().folded_to_unfolded_line_number(pe)
				).first;

			fragment_generator<fragment_generator_component_hub<
				soft_linebreak_inserter, folded_region_skipper
				>> gen(
					*edt->get_document(), edt->get_font_families(), firstchar,
					soft_linebreak_inserter(fmt.get_linebreaks(), firstchar),
					folded_region_skipper(fmt.get_folding(), firstchar)
				);
			fragment_assembler ass(*edt);

			r.begin_drawing(*rt.target);
			r.push_matrix_mult(matd3x3::scale(vec2d(0.0, 0.0), scale));
			while (gen.get_position() < plastchar) {
				fragment_generation_result tok = gen.generate_and_update();
				std::visit([&ass, &r](auto &&frag) {
					auto &&rendering = ass.append(frag);
					ass.render(r, rendering);
					}, tok.result);
				if (std::holds_alternative<linebreak_fragment>(tok.result)) {
					++curvisline;
				} else if (ass.get_horizontal_position() > _width / scale) {
					++curvisline;
					std::size_t
						pos = fmt.get_linebreaks().get_beginning_char_of_visual_line(
							fmt.get_folding().folded_to_unfolded_line_number(curvisline)
						).first;
					gen.reposition(pos);
					ass.advance_vertical_position(1);
					ass.set_horizontal_position(0.0);
				}
			}
			r.pop_matrix();
			r.end_drawing();
			pages.insert(std::make_pair(s, std::move(rt)));
		}
	}


	const ui::property_mapping &minimap::get_properties() const {
		return get_properties_static();
	}

	const ui::property_mapping &minimap::get_properties_static() {
		static ui::property_mapping mapping;
		if (mapping.empty()) {
			mapping = ui::element::get_properties_static();
			mapping.emplace(
				u8"viewport_visuals",
				std::make_shared<ui::member_pointer_property<&minimap::_viewport_visuals>>(
					[](minimap &m) {
						m.invalidate_visual();
					}
				)
			);
		}

		return mapping;
	}

	void minimap::_custom_render() const {
		element::_custom_render();
		if (contents_region *edt = component_helper::get_contents_region(*this)) {
			std::pair<std::size_t, std::size_t> vlines = _get_visible_visual_lines();
			double
				slh = edt->get_line_height() * get_scale(),
				top = std::round(get_padding().top - _get_y_offset());
			auto
				ibeg = _pgcache.pages.upper_bound(vlines.first),
				iend = _pgcache.pages.lower_bound(vlines.second);
			if (ibeg != _pgcache.pages.begin()) {
				--ibeg;
			} else {
				logger::get().log_error(CP_HERE) << "agnomaly in page range selection";
			}

			ui::renderer_base &r = get_manager().get_renderer();
			r.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));
			for (auto i = ibeg; i != iend; ++i) {
				auto &bmp = *i->second.target_bitmap;
				vec2d topleft(get_padding().left, std::floor(top + slh * static_cast<double>(i->first)));
				r.draw_rectangle(
					rectd::from_corner_and_size(topleft, bmp.get_size()),
					ui::generic_brush(
						ui::brushes::bitmap_pattern(&bmp), matd3x3::translate(topleft)
					),
					ui::generic_pen()
				);
			}
			// render visible region indicator
			_viewport_visuals.render(_get_clamped_viewport_rect(), r);
			r.pop_clip();
		}
	}
}
