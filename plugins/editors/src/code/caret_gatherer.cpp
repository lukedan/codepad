// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/caret_gatherer.h"

/// \file
/// Declaration of \ref codepad::editors::code::caret_gatherer.

namespace codepad::editors::code {
	caret_gatherer::caret_gatherer(
		const caret_set &set, std::size_t pos, const fragment_assembler &ass, bool stall
	) : _carets(set), _assembler(&ass) {
		// find the first candidate caret to render
		auto first = _carets.find_first_ending_at_or_after(pos);
		while (_queued.size() < maximum_num_lookahead_carets && first.get_iterator() != _carets.carets.end()) {
			auto caret_sel = first.get_caret_selection();
			if (
				caret_sel.get_selection_end() > pos ||
				!_should_end_before_stall(caret_sel, first.get_iterator()->data, stall)
			) {
				// not too late
				if (
					caret_sel.selection_begin < pos || (
						caret_sel.selection_begin == pos &&
						_should_start_before_stall(caret_sel, first.get_iterator()->data, stall
					)
				)) { // not too early to start; should jumpstart
					_active.emplace_back(_single_caret_renderer::jumpstart(*_assembler, first));
				} else { // queue for later
					_queued.emplace_back(first);
				}
			}
			first.move_next();
		}
	}

	void caret_gatherer::skip_line(bool stall, std::size_t posafter) {
		// first update active renderers
		for (auto it = _active.begin(); it != _active.end(); ) {
			if (!it->handle_line_skip(posafter, stall, _assembler->get_horizontal_position(), *this)) {
				it = _active.erase(it);
			} else {
				++it;
			}
		}
		// check & jumpstart or discard pending carets
		for (auto it = _queued.begin(); it != _queued.end(); ) {
			ui::caret_selection caret_sel = it->get_caret_selection();
			if ( // too early
				caret_sel.selection_begin > posafter || (
					caret_sel.selection_begin == posafter &&
					!_should_start_before_stall(caret_sel, it->get_iterator()->data, stall)
				)
			) {
				++it; // next
			} else { // either start or discard
				if (
					caret_sel.get_selection_end() > posafter || (
						caret_sel.get_selection_end() == posafter &&
						!_should_end_before_stall(caret_sel, it->get_iterator()->data, stall)
					)
				) { // should start
					_active.emplace_back(_single_caret_renderer::jumpstart_at_skip_line(*_assembler, *it));
				} // otherwise should only discard
				// queue another & remove entry
				auto next = _queued.back();
				next.move_next();
				if (next.get_iterator() != _carets.carets.end()) {
					_queued.emplace_back(next);
				}
				it = _queued.erase(it);
			}
		}
	}

	void caret_gatherer::finish(std::size_t position) {
		for (auto &rend : _active) {
			rend.finish(position, *this);
		}
		// draw the possibly left out last caret, which is at the very end of the document and does not have a
		// selection
		for (auto next_caret : _queued) {
			if (next_caret.get_caret_selection().get_caret_position() == position) {
				double x = _assembler->get_horizontal_position();
				_caret_rects.emplace_back(rectd::from_xywh(
					_assembler->get_horizontal_position(), _assembler->get_vertical_position(),
					10.0, _assembler->get_line_height()
				));
				auto &last_selection = _selected_regions.emplace_back();
				last_selection.top = _assembler->get_vertical_position();
				last_selection.line_height = _assembler->get_line_height();
				last_selection.baseline = _assembler->get_baseline();
				last_selection.line_bounds.emplace_back(x, x);
			}
		}
	}


	std::optional<caret_gatherer::_single_caret_renderer> caret_gatherer::_single_caret_renderer::start_at_fragment(
		const text_fragment&, const fragment_assembler::text_rendering &r,
		std::size_t steps, std::size_t posafter, caret_gatherer &rend, caret_set::iterator_position iter
	) {
		_single_caret_renderer res(
			iter, r.topleft.x, r.topleft.y,
			rend.get_fragment_assembler().get_line_height(), rend.get_fragment_assembler().get_baseline()
		);
		assert_true_logical(
			res._caret_selection.get_selection_end() + steps >= posafter,
			"single caret renderer was not started or discarded in time"
		);
		if (res._caret_selection.selection_begin < posafter) { // start
			rectd caret = r.text->get_character_placement(res._caret_selection.selection_begin - (posafter - steps));
			res._region_left += caret.xmin;
			if (res._caret_selection.caret_offset == 0) { // add caret
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
		const fragment_assembler &ass, caret_set::iterator_position iter
	) {
		vec2d pos = ass.get_position();
		_single_caret_renderer res(iter, pos.x, pos.y, ass.get_line_height(), ass.get_baseline());
		res._append_line_selection(pos.x);
		res._region_left = 0.0;
		return res;
	}

	bool caret_gatherer::_single_caret_renderer::handle_fragment(
		const text_fragment&, const fragment_assembler::text_rendering &r,
		std::size_t steps, std::size_t posafter, caret_gatherer &rend
	) {
		assert_true_logical(steps > 0, "invalid text fragment");
		std::size_t range_end = _caret_selection.get_selection_end();
		if (posafter > range_end) { // terminate here
			rectd pos = r.text->get_character_placement(range_end - (posafter - steps));
			_terminate_with_caret(range_end, rectd::from_xywh(
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
			_append_line_selection(frag.type == ui::line_ending::none ? pos.xmin : pos.xmax);
			_region_left = 0.0;
			return true;
		}
		return false;
	}

	bool caret_gatherer::_single_caret_renderer::handle_line_skip(
		std::size_t posafter, bool stall, double x, caret_gatherer &rend
	) {
		if (
			_caret_selection.get_selection_end() < posafter || ( // end before this position
				_caret_selection.get_selection_end() == posafter &&
				_should_end_before_stall(_caret_selection, _caret_iter.get_iterator()->data, stall)
			)
		) { // stop here
			_terminate(x, rend);
			return false;
		} // else jump to the next line
		_append_line_selection(x);
		_region_left = 0.0;
		return true;
	}

	std::optional<
		caret_gatherer::_single_caret_renderer
	> caret_gatherer::_single_caret_renderer::_start_at_solid_fragment(
		rectd caret, std::size_t steps, std::size_t posafter,
		caret_gatherer &rend, caret_set::iterator_position c
	) {
		_single_caret_renderer res(
			c, caret.xmin, caret.ymin,
			rend.get_fragment_assembler().get_line_height(), rend.get_fragment_assembler().get_baseline()
		);
		if (res._caret_selection.get_selection_end() + steps < posafter) { // too late
			return std::nullopt;
		}
		if (steps == 0) { // stall
			if (
				res._caret_selection.selection_begin == posafter &&
				_should_start_before_stall(res._caret_selection, res._caret_iter.get_iterator()->data, true)
			) {
				rend._caret_rects.emplace_back(caret);
				return res;
			}
		} else { // not stall
			if (posafter > res._caret_selection.selection_begin) { // should start
				if (res._caret_selection.get_caret_position() + steps == posafter) { // should also generate caret
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
			if (
				posafter == _caret_selection.get_selection_end() &&
				_should_end_before_stall(_caret_selection, _caret_iter.get_iterator()->data, true)
			) {
				_terminate_with_caret(posafter, caret, rend);
				return false;
			}
		} else { // not stall
			if (posafter > _caret_selection.get_selection_end()) { // stop here & this fragment is completely covered
				_terminate_with_caret(posafter - steps, caret, rend);
				return false;
			}
		}
		return true;
	}
}
