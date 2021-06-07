// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/fragment_generation.h"

/// \file
/// Implementation of certain fragment generation functionalities.

#include "codepad/editors/code/contents_region.h"

namespace codepad::editors::code {
	text_fragment text_fragment::gizmo_from_utf8(std::u8string_view txt, colord c, std::shared_ptr<ui::font> f) {
		std::basic_string<codepoint> u32;
		for (auto it = txt.begin(); it != txt.end(); ) {
			codepoint cp;
			if (!encodings::utf8::next_codepoint(it, txt.end(), cp)) {
				cp = encodings::replacement_character;
			}
			u32.push_back(cp);
		}
		return text_fragment(std::move(u32), c, std::move(f), true);
	}


	fragment_generation_result soft_linebreak_inserter::generate(std::size_t position) {
		if (_cur_softbreak != _reg.end()) {
			std::size_t nextpos = _prev_chars + _cur_softbreak->length;
			if (position == nextpos) {
				_prev_chars += _cur_softbreak->length;
				++_cur_softbreak;
				return fragment_generation_result(linebreak_fragment(line_ending::none), 0);
			}
			return fragment_generation_result(no_fragment(), nextpos - position);
		}
		return fragment_generation_result::exhausted();
	}

	void soft_linebreak_inserter::update(std::size_t oldpos, std::size_t steps) {
		std::size_t newpos = oldpos + steps;
		if (steps > 0 && _cur_softbreak != _reg.end()) { // no update needed if not moved
			if (newpos > _prev_chars + _cur_softbreak->length) {
				// reset once the iterator to the next soft linebreak is invalid
				reposition(newpos);
			}
		}
	}


	fragment_generation_result folded_region_skipper::generate(std::size_t position) {
		if (_cur_region != _reg.end()) {
			if (position >= _region_start) { // jump
				return fragment_generation_result(
					_frag_func(_cur_region), _cur_region->range - (position - _region_start)
				);
			}
			return fragment_generation_result(no_fragment(), _region_start - position);
		}
		return fragment_generation_result::exhausted();
	}

	void folded_region_skipper::update(std::size_t oldpos, std::size_t steps) {
		if (_cur_region != _reg.end()) {
			std::size_t newpos = oldpos + steps, regionend = _region_start + _cur_region->range;
			if (newpos >= regionend) { // advance to the next region and check again
				++_cur_region;
				if (_cur_region != _reg.end()) {
					_region_start = regionend + _cur_region->gap;
					if (_region_start + _cur_region->range <= newpos) { // nope, still ahead
						reposition(newpos);
					}
				}
			}
		}
	}


	fragment_assembler::fragment_assembler(const contents_region &rgn) : fragment_assembler(
		rgn.get_manager().get_renderer(), *rgn.get_font_families()[0], rgn.get_font_size(),
		rgn.get_line_height(), rgn.get_baseline(), rgn.get_tab_width()
	) {
	}
}
