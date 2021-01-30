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


	fragment_assembler::fragment_assembler(const contents_region &rgn) : fragment_assembler(
		rgn.get_manager().get_renderer(), *rgn.get_font_families()[0], rgn.get_font_size(),
		rgn.get_line_height(), rgn.get_baseline(), rgn.get_tab_width()
	) {
	}


	caret_gatherer::caret_gatherer(
		const caret_set::container &set, std::size_t pos, const fragment_assembler &ass, bool stall
	) : _carets(set), _assembler(&ass) {
		// find the first candidate caret to render
		auto first = _carets.lower_bound(ui::caret_selection(pos, 0));
		if (first != _carets.begin()) {
			auto prev = first;
			--prev;
			if (prev->first.selection >= pos) {
				first = prev;
			}
		}
		while (first != _carets.end() && _queued.size() < maximum_num_lookahead_carets) {
			std::pair<std::size_t, std::size_t> range = std::minmax(first->first.caret, first->first.selection);
			if (range.second > pos || !_should_end_before_stall(first->first, first->second, stall)) {
				// not too late
				if ( // not too early to start
					range.first < pos ||
					(range.first == pos && _should_start_before_stall(first->first, first->second, stall))
					) { // should jumpstart
					_active.emplace_back(_single_caret_renderer::jumpstart(*_assembler, first));
				} else { // queue for later
					_queued.emplace_back(first);
				}
			}
			++first;
		}
	}

	void caret_gatherer::skip_line(bool stall, std::size_t posafter) {
		// first update active renderers
		for (auto it = _active.begin(); it != _active.end(); ) {
			if (!it->handle_line_skip(
				posafter, stall,
				_assembler->get_horizontal_position(), _assembler->get_vertical_position(),
				_assembler->get_vertical_position() + _assembler->get_line_height(),
				*this
			)) {
				it = _active.erase(it);
			} else {
				++it;
			}
		}
		// check & jumpstart or discard pending carets
		for (auto it = _queued.begin(); it != _queued.end(); ) {
			std::pair<std::size_t, std::size_t> range = std::minmax((*it)->first.caret, (*it)->first.selection);
			if ( // too early
				range.first > posafter ||
				(range.first == posafter && !_should_start_before_stall((*it)->first, (*it)->second, stall))
				) {
				++it; // next
			} else { // either start or discard
				if ( // should start
					range.second > posafter ||
					(range.second == posafter && !_should_end_before_stall((*it)->first, (*it)->second, stall))
					) {
					_active.emplace_back(_single_caret_renderer::jumpstart_at_skip_line(*_assembler, *it));
				} // otherwise should only discard
				// queue another & remove entry
				if (caret_set::const_iterator next = _queued.back(); (++next) != _carets.end()) {
					_queued.emplace_back(next);
				}
				it = _queued.erase(it);
			}
		}
	}


	std::optional<caret_gatherer::_single_caret_renderer> caret_gatherer::_single_caret_renderer::start_at_fragment(
		const text_fragment&, const fragment_assembler::text_rendering &r,
		std::size_t steps, std::size_t posafter, caret_gatherer &rend, caret_set::const_iterator iter
	) {
		_single_caret_renderer res(iter, r.topleft.x);
		if (res._range.second + steps < posafter) { // too late
			return std::nullopt;
		}
		if (res._range.first < posafter) { // start
			rectd caret = r.text->get_character_placement(res._range.first - (posafter - steps));
			res._region_left += caret.xmin;
			if (res._range.first == res._caret->first.caret) { // add caret
				rend._caret_rects.emplace_back(rectd::from_xywh(
					res._region_left, r.topleft.y,
					caret.width(), rend.get_fragment_assembler().get_line_height()
				));
			}
			return res;
		}
		return std::nullopt;
	}

	caret_gatherer::_single_caret_renderer caret_gatherer::_single_caret_renderer::jumpstart_at_skip_line(
		const fragment_assembler &ass, caret_set::const_iterator iter
	) {
		vec2d pos = ass.get_position();
		_single_caret_renderer res(iter, pos.x);
		res._append_line_selection(pos.x, pos.y, pos.y + ass.get_line_height());
		res._region_left = 0.0;
		return res;
	}

	bool caret_gatherer::_single_caret_renderer::handle_fragment(
		const text_fragment&, const fragment_assembler::text_rendering &r,
		std::size_t steps, std::size_t posafter, caret_gatherer &rend
	) {
		assert_true_logical(steps > 0, "invalid text fragment");
		if (posafter > _range.second) { // terminate here
			rectd pos = r.text->get_character_placement(_range.second - (posafter - steps));
			_terminate_with_caret(_range.second, rectd::from_xywh(
				pos.xmin + r.topleft.x, rend.get_fragment_assembler().get_vertical_position(),
				pos.width(), rend.get_fragment_assembler().get_line_height()
			), rend);
			return false;
		}
		return true;
	}

	bool caret_gatherer::_single_caret_renderer::handle_fragment(
		const linebreak_fragment &frag, const fragment_assembler::basic_rendering &r,
		std::size_t steps, std::size_t posafter, caret_gatherer &rend
	) {
		rectd pos = rectd::from_xywh(
			r.topleft.x, r.topleft.y, 10.0, rend.get_fragment_assembler().get_line_height()
		);
		if (_handle_solid_fragment(pos, steps, posafter, rend)) { // not over, to the next line
			// for soft linebreaks, do not add space after the line
			_append_line_selection(frag.type == ui::line_ending::none ? pos.xmin : pos.xmax, pos.ymin, pos.ymax);
			_region_left = 0.0;
			return true;
		}
		return false;
	}

	bool caret_gatherer::_single_caret_renderer::handle_line_skip(
		std::size_t posafter, bool stall, double x, double ymin, double ymax, caret_gatherer &rend
	) {
		if (
			_range.second < posafter || // end before this position
			(_range.second == posafter && _should_end_before_stall(_caret->first, _caret->second, stall))
		) { // stop here
			_terminate(x, ymin, ymax, rend);
			return false;
		} // else jump to the next line
		_append_line_selection(x, ymin, ymax);
		_region_left = 0.0;
		return true;
	}

	std::optional<
		caret_gatherer::_single_caret_renderer
	> caret_gatherer::_single_caret_renderer::_start_at_solid_fragment(
		rectd caret, std::size_t steps, std::size_t posafter,
		caret_gatherer &rend, caret_set::const_iterator c
	) {
		_single_caret_renderer res(c, caret.xmin);
		if (res._range.second + steps < posafter) { // too late
			return std::nullopt;
		}
		if (steps == 0) { // stall
			if (res._range.first == posafter && _should_start_before_stall(c->first, c->second, true)) {
				rend._caret_rects.emplace_back(caret);
				return res;
			}
		} else { // not stall
			if (posafter > res._range.first) { // should start
				if (res._caret->first.caret + steps == posafter) { // should also generate caret
					rend._caret_rects.emplace_back(caret);
				}
				return res;
			}
		}
		return std::nullopt;
	}

	bool caret_gatherer::_single_caret_renderer::_handle_solid_fragment(
		rectd caret, std::size_t steps, std::size_t posafter, caret_gatherer &rend
	) {
		if (steps == 0) { // stall
			if (posafter == _range.second && _should_end_before_stall(_caret->first, _caret->second, true)) {
				_terminate_with_caret(posafter, caret, rend);
				return false;
			}
		} else { // not stall
			if (posafter > _range.second) { // stop here & this fragment is completely covered
				_terminate_with_caret(posafter - steps, caret, rend);
				return false;
			}
		}
		return true;
	}
}
