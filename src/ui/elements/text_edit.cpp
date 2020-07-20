// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/text_edit.h"

/// \file
/// Implementation of \ref codepad::ui::text_edit.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.h"

namespace codepad::ui {
	std::pair<std::u8string::iterator, bool> text_edit::modify(
		std::size_t del_begin, std::size_t del_len, std::u8string_view add
	) {
		// TODO this is no doubt going to be *really* buggy and would require lots of testing
		//      mostly because the character indices calculated here may not match the character indices of the
		//      formatted_text implementation
		auto it_beg = _text.begin();
		std::size_t i = 0;
		bool all_valid = false; // all codepoints are valid
		for (; i < del_begin && it_beg != _text.end(); ++i) {
			all_valid = encodings::utf8::next_codepoint(it_beg, _text.end()) && all_valid;
		}
		std::size_t del_end = del_begin + del_len;
		auto it_end = it_beg;
		for (; i < del_end && it_end != _text.end(); ++i) {
			all_valid = encodings::utf8::next_codepoint(it_end, _text.end()) && all_valid;
		}
		// erase & insert
		if (it_beg != it_end) {
			it_beg = _text.erase(it_beg, it_end);
		}
		it_beg = _text.insert(it_beg, add.begin(), add.end());
		_on_text_changed();
		return { it_beg, all_valid };
	}

	void text_edit::move_caret_left(bool continue_sel) {
		move_caret_raw(
			[this]() {
				return _get_previous_caret_position(_caret.caret);
			},
			[this]() {
				return std::min(_caret.caret, _caret.selection);
			},
				continue_sel
				);
	}

	void text_edit::move_caret_right(bool continue_sel) {
		move_caret_raw(
			[this]() {
				return _get_next_caret_position(_caret.caret);
			},
			[this]() {
				return std::max(_caret.caret, _caret.selection);
			},
				continue_sel
				);
	}

	void text_edit::move_caret_to_line_beginning(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.caret);
				return _cached_line_beginnings[line];
			},
			continue_sel
				);
	}

	void text_edit::move_caret_to_line_ending(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.caret);
				return _cached_line_beginnings[line] + _cached_line_metrics[line].non_linebreak_characters;
			},
			continue_sel
				);
	}

	void text_edit::delete_character_before_caret() {
		std::size_t rem_beg = 0, rem_end = 0;
		if (_caret.has_selection()) {
			auto [sel_beg, sel_end] = _caret.get_range();
			rem_beg = sel_beg;
			rem_end = sel_end;
		} else {
			std::size_t prev = _get_previous_caret_position(_caret.caret);
			rem_beg = prev;
			rem_end = _caret.caret;
		}
		modify(rem_beg, rem_end - rem_beg, std::u8string_view());
		_caret = caret_selection(rem_beg);
		_on_caret_changed();
	}

	void text_edit::delete_character_after_caret() {
		std::size_t rem_beg = 0, rem_end = 0;
		if (_caret.has_selection()) {
			auto [sel_beg, sel_end] = _caret.get_range();
			rem_beg = sel_beg;
			rem_end = sel_end;
		} else {
			std::size_t next = _get_next_caret_position(_caret.caret);
			rem_beg = _caret.caret;
			rem_end = next;
		}
		modify(rem_beg, rem_end - rem_beg, std::u8string_view());
		_caret = caret_selection(rem_beg);
		_on_caret_changed();
	}

	const property_mapping &text_edit::get_properties() const {
		return get_properties_static();
	}

	const property_mapping &text_edit::get_properties_static() {
		static property_mapping mapping;
		if (mapping.empty()) {
			mapping = label::get_properties_static();
			mapping.emplace(u8"caret_visuals", std::make_shared<member_pointer_property<&text_edit::_caret_visuals>>(
				[](text_edit &edit) {
					edit.invalidate_visual();
				}
			));
			mapping.emplace(
				u8"selection_visuals",
				std::make_shared<member_pointer_property<&text_edit::_selection_visuals>>(
					[](text_edit &edit) {
						edit.invalidate_visual();
					}
					)
			);
		}

		return mapping;
	}

	void text_edit::_on_mouse_move(mouse_move_info &info) {
		label::_on_mouse_move(info);
		if (_selecting) {
			auto hit = _hit_test_for_caret(info.new_position.get(*this));
			_caret.caret = hit.rear ? hit.character + 1 : hit.character;
			_on_caret_changed();
		}
	}

	void text_edit::_on_mouse_down(mouse_button_info &info) {
		label::_on_mouse_down(info);
		if (info.button == mouse_button::primary) {
			assert_true_logical(!_selecting, "mouse pressed again when selecting");
			auto hit = _hit_test_for_caret(info.position.get(*this));
			_caret = caret_selection(hit.rear ? hit.character + 1 : hit.character);
			_on_caret_changed();

			get_window()->set_mouse_capture(*this);
			_selecting = true;
		}
	}

	void text_edit::_on_mouse_up(mouse_button_info &info) {
		label::_on_mouse_up(info);
		if (info.button == mouse_button::primary && _selecting) {
			_selecting = false;
			get_window()->release_mouse_capture();
		}
	}

	void text_edit::_on_capture_lost() {
		label::_on_capture_lost();
		assert_true_logical(_selecting, "capture set but not selecting");
		_selecting = false;
	}

	void text_edit::_on_text_layout_changed() {
		label::_on_text_layout_changed();
		_update_window_caret_position();
	}

	void text_edit::_on_layout_changed() {
		label::_on_layout_changed();
		_update_window_caret_position();
	}

	void text_edit::_on_keyboard_text(text_info &info) {
		label::_on_keyboard_text(info);

		if (_selecting) { // stop selecting
			_selecting = false;
			get_window()->release_mouse_capture();
		}

		// modify text
		auto [sel_beg, sel_end] = _caret.get_range();
		auto [it_beg, all_valid] = modify(sel_beg, sel_end - sel_beg, info.content);
		// compute new caret position
		std::size_t new_caret = sel_beg;
		auto it_target = it_beg + info.content.size();
		if (!all_valid) { // restart from the beginning
			it_beg = _text.begin();
			new_caret = 0;
		}
		while (it_beg < it_target) {
			encodings::utf8::next_codepoint(it_beg, _text.end());
			++new_caret;
		}
		_caret = caret_selection(new_caret);
		_on_caret_changed();
	}

	void text_edit::_on_text_changed() {
		label::_on_text_changed();
		_cached_line_metrics.clear();
		_cached_line_beginnings.clear();
	}

	void text_edit::_on_caret_changed() {
		invalidate_visual();
		_update_window_caret_position();
		caret_changed.invoke();
	}

	caret_hit_test_result text_edit::_hit_test_for_caret(vec2d pos) const {
		_check_cache_format();
		return _cached_fmt->hit_test(
			pos - (get_client_region().xmin_ymin() - get_layout().xmin_ymin())
		);
	}

	void text_edit::_update_window_caret_position() const {
		if (get_manager().get_scheduler().get_focused_element() == this) {
			_check_cache_format();
			get_window()->set_active_caret_position(
				_cached_fmt->get_character_placement(_caret.caret).translated(get_client_region().xmin_ymin())
			);
		}
	}

	void text_edit::_check_cache_line_info() {
		if (_cached_line_metrics.empty()) {
			_check_cache_format();
			_cached_line_metrics = _cached_fmt->get_line_metrics();
			// compute _cached_line_beginnings
			_cached_line_beginnings.clear();
			_cached_line_beginnings.reserve(_cached_line_metrics.size() + 1);
			_cached_line_beginnings.emplace_back(0);
			for (const line_metrics &entry : _cached_line_metrics) {
				std::size_t nchars = _cached_line_beginnings.back() + entry.get_total_num_characters();
				_cached_line_beginnings.emplace_back(nchars);
			}
		}
	}

	std::size_t text_edit::_get_line_of_character(std::size_t pos) {
		auto it = std::upper_bound(_cached_line_beginnings.begin(), _cached_line_beginnings.end() - 1, pos);
		return (it - _cached_line_beginnings.begin()) - 1;
	}

	std::size_t text_edit::_get_previous_caret_position(std::size_t pos) {
		if (pos == 0) {
			return 0;
		}
		_check_cache_line_info();
		std::size_t line = _get_line_of_character(pos);
		if (pos > _cached_line_beginnings[line]) {
			return pos - 1;
		}
		return pos - std::max<std::size_t>(_cached_line_metrics[line - 1].linebreak_characters, 1);
	}

	std::size_t text_edit::_get_next_caret_position(std::size_t pos) {
		_check_cache_line_info();
		if (pos >= _cached_line_beginnings.back()) {
			return pos;
		}
		std::size_t line = _get_line_of_character(pos);
		std::size_t line_end = _cached_line_beginnings[line] + _cached_line_metrics[line].non_linebreak_characters;
		if (pos < line_end) {
			return pos + 1;
		}
		return pos + std::max<std::size_t>(_cached_line_metrics[line].linebreak_characters, 1);
	}

	void text_edit::_custom_render() const {
		label::_custom_render();
		// label::_on_prerender() calls _check_cache_format(), so no need to call again here
		rectd cursor = _cached_fmt->get_character_placement(_caret.caret);
		_caret_visuals.render(cursor, get_manager().get_renderer());
		if (_caret.has_selection()) { // render selection
			auto [sel_min, sel_max] = _caret.get_range();
			std::vector<rectd> rs = _cached_fmt->get_character_range_placement(sel_min, sel_max - sel_min);
			for (rectd r : rs) {
				_selection_visuals.render(r, get_manager().get_renderer());
			}
		}
	}
}
