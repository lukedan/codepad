// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/line_number_display.h"

/// \file
/// Implementation of the line number display.

namespace codepad::editors::code {
	ui::size_allocation line_number_display::get_desired_width() const {
		if (contents_region *edt = component_helper::get_contents_region(*this)) {
			std::size_t ln = edt->get_document()->num_lines(), w = 0;
			for (; ln > 0; ++w, ln /= 10) {
			}
			// TODO customizable font parameters
			auto font = edt->get_font_families()[0]->get_matching_font(
				ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
			);
			double maxw = edt->get_font_size() * font->get_maximum_character_width_em(
				reinterpret_cast<const codepoint*>(U"0123456789")
			);
			return ui::size_allocation::pixels(get_padding().width() + static_cast<double>(w) * maxw);
		}
		return ui::size_allocation::pixels(0.0);
	}

	void line_number_display::_register_handlers() {
		if (!_events_registered) {
			if (contents_region *edt = component_helper::get_contents_region(*this)) {
				_events_registered = true;
				edt->editing_visual_changed += [this]() {
					// when the content is modified, it is possible that the number of digits is changed,
					// so we recalculate layout here
					_on_desired_size_changed(true, false);
				};
			}
		}
	}

	void line_number_display::_custom_render() const {
		element::_custom_render();

		if (auto &&[box, edt] = component_helper::get_core_components(*this); edt) {
			const view_formatting &fmt = edt->get_formatting();
			double
				lh = edt->get_line_height(),
				ybeg = box->get_vertical_position() - edt->get_padding().top,
				yend = ybeg + edt->get_layout().height();
			std::size_t
				fline = static_cast<std::size_t>(std::max(ybeg / lh, 0.0)),
				eline = static_cast<std::size_t>(yend / lh) + 1;
			rectd client = get_client_region();
			double cury = static_cast<double>(fline) * lh - ybeg, width = client.width() + get_padding().left;

			auto &renderer = get_manager().get_renderer();
			auto font = edt->get_font_families()[0]->get_matching_font(
				ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal
			);
			double baseline_correction = edt->get_baseline() - font->get_ascent_em() * edt->get_font_size();

			{
				renderer.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));

				for (std::size_t curi = fline; curi < eline; ++curi, cury += lh) {
					std::size_t line = fmt.get_folding().folded_to_unfolded_line_number(curi);
					auto lineinfo = fmt.get_linebreaks().get_line_info(line);
					if (lineinfo.first.entry == edt->get_document()->get_linebreaks().end()) {
						break; // when after the end of the document
					}
					if (lineinfo.first.first_char >= lineinfo.second.prev_chars) { // ignore soft linebreaks
						std::string curlbl = std::to_string(1 + line - lineinfo.second.prev_softbreaks);
						auto text = renderer.create_plain_text(
							std::u8string_view(reinterpret_cast<const char8_t*>(curlbl.data()), curlbl.size()),
							*font, edt->get_font_size()
						);
						double w = text->get_width();
						// TODO customizable color
						renderer.draw_plain_text(*text, vec2d(width - w, cury + baseline_correction), colord());
					}
				}

				renderer.pop_clip();
			}
		}
	}
}
