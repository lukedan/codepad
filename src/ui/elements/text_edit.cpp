// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/text_edit.h"

/// \file
/// Implementation of \ref codepad::ui::text_edit.

#include "codepad/ui/manager.h"
#include "codepad/ui/json_parsers.inl"

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
				return _get_previous_caret_position(_caret.get_caret_position());
			},
			[this]() {
				return _caret.selection_begin;
			},
			continue_sel
		);
	}

	void text_edit::move_caret_right(bool continue_sel) {
		move_caret_raw(
			[this]() {
				return _get_next_caret_position(_caret.get_caret_position());
			},
			[this]() {
				return _caret.get_selection_end();
			},
			continue_sel
		);
	}

	void text_edit::move_caret_to_line_beginning(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.get_caret_position());
				return std::pair<std::size_t, double>(
					_cached_line_beginnings[line], -std::numeric_limits<double>::infinity()
				);
			},
			continue_sel
		);
	}

	void text_edit::move_caret_to_line_ending(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.get_caret_position());
				return std::pair<std::size_t, double>(
					_cached_line_beginnings[line] + _cached_line_metrics[line].non_linebreak_characters,
					std::numeric_limits<double>::infinity()
				);
			},
			continue_sel
		);
	}

	void text_edit::move_caret_up(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.get_caret_position());
				if (line > 0) {
					--line;
				}
				auto hit_test_result = _formatted_text->hit_test_at_line(line, _alignment);
				return std::pair<std::size_t, double>(
					hit_test_result.rear ? hit_test_result.character + 1 : hit_test_result.character,
					_alignment
				);
			},
			continue_sel
		);
	}

	void text_edit::move_caret_down(bool continue_sel) {
		move_caret_raw(
			[this]() {
				_check_cache_line_info();
				std::size_t line = _get_line_of_character(_caret.get_caret_position());
				auto hit_test_result = _formatted_text->hit_test_at_line(line + 1, _alignment);
				return std::pair<std::size_t, double>(
					hit_test_result.rear ? hit_test_result.character + 1 : hit_test_result.character,
					_alignment
				);
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
			std::size_t prev = _get_previous_caret_position(_caret.selection_begin);
			rem_beg = prev;
			rem_end = _caret.selection_begin;
		}
		modify(rem_beg, rem_end - rem_beg, std::u8string_view());
		_set_caret_selection_impl(caret_selection(rem_beg));
	}

	void text_edit::delete_character_after_caret() {
		std::size_t rem_beg = 0, rem_end = 0;
		if (_caret.has_selection()) {
			auto [sel_beg, sel_end] = _caret.get_range();
			rem_beg = sel_beg;
			rem_end = sel_end;
		} else {
			std::size_t next = _get_next_caret_position(_caret.selection_begin);
			rem_beg = _caret.selection_begin;
			rem_end = next;
		}
		modify(rem_beg, rem_end - rem_beg, std::u8string_view());
		_set_caret_selection_impl(caret_selection(rem_beg));
	}

	void text_edit::_on_mouse_move(mouse_move_info &info) {
		label::_on_mouse_move(info);
		if (_selecting) {
			auto hit = _hit_test_for_caret(info.new_position.get(*this));
			caret_selection new_caret_selection = _caret;
			new_caret_selection.move_caret(hit.rear ? hit.character + 1 : hit.character);
			_set_caret_selection_impl(new_caret_selection);
		}
	}

	void text_edit::_on_mouse_down(mouse_button_info &info) {
		label::_on_mouse_down(info);
		if (info.button == mouse_button::primary) {
			assert_true_logical(!_selecting, "mouse pressed again when selecting");
			auto hit = _hit_test_for_caret(info.position.get(*this));
			_set_caret_selection_impl(caret_selection(hit.rear ? hit.character + 1 : hit.character));

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

		if (_readonly) { // don't do anything when readonly
			return;
		}

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
		_set_caret_selection_impl(caret_selection(new_caret));
	}

	void text_edit::_on_text_changed() {
		label::_on_text_changed();
		_cached_line_metrics.clear();
		_cached_line_beginnings.clear();
		text_changed();
	}

	void text_edit::_on_caret_changed() {
		invalidate_visual();
		_update_window_caret_position();
		caret_changed.invoke();
	}

	caret_hit_test_result text_edit::_hit_test_for_caret(vec2d pos) const {
		return get_formatted_text().hit_test(
			pos - (get_client_region().xmin_ymin() - get_layout().xmin_ymin())
		);
	}

	void text_edit::_update_window_caret_position() {
		if (get_manager().get_scheduler().get_focused_element() == this) {
			rectd caret = get_formatted_text().get_character_placement(_caret.get_caret_position());
			caret = caret.translated(get_client_region().xmin_ymin());
			get_window()->set_active_caret_position(caret);
		}
	}

	void text_edit::_check_cache_line_info() {
		if (_cached_line_metrics.empty()) {
			_cached_line_metrics = get_formatted_text().get_line_metrics();
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

	property_info text_edit::_find_property_path(const property_path::component_list &path) const {
		if (path.front().is_type_or_empty(u8"text_edit")) {
			if (path.front().property == u8"readonly") {
				return property_info::find_member_pointer_property_info<&text_edit::_readonly, element>(
					path, property_info::make_typed_modification_callback<element, text_edit>([](text_edit &edt) {
						edt._on_readonly_changed();
					})
				);
			}
			if (path.front().property == u8"caret_visuals") {
				return property_info::find_member_pointer_property_info_managed<
					&text_edit::_caret_visuals, element
				>(
					path, get_manager(),
					property_info::make_typed_modification_callback<element, text_edit>([](text_edit &edt) {
						edt.invalidate_visual();
					})
				);
			}
			if (path.front().property == u8"selection_visuals") {
				return property_info::find_member_pointer_property_info_managed<
					&text_edit::_selection_visuals, element
				>(
					path, get_manager(),
					property_info::make_typed_modification_callback<element, text_edit>([](text_edit &edt) {
						edt.invalidate_visual();
					})
				);
			}
		}
		return label::_find_property_path(path);
	}

	void text_edit::_custom_render() const {
		label::_custom_render();
		// label::_on_prerender() calls _check_cache_format(), so no need to call again here
		vec2d offset(get_padding().left, get_padding().top);
		rectd cursor = get_formatted_text().get_character_placement(_caret.get_caret_position()).translated(offset);
		_caret_visuals.render(cursor, get_manager().get_renderer());
		if (_caret.has_selection()) { // render selection
			auto [sel_min, sel_max] = _caret.get_range();
			std::vector<rectd> rs = get_formatted_text().get_character_range_placement(sel_min, sel_max - sel_min);
			for (const rectd &r : rs) {
				_selection_visuals.render(r.translated(offset), get_manager().get_renderer());
			}
		}
	}


	bool textbox::_handle_reference(std::u8string_view role, element *elem) {
		if (role == get_text_edit_name()) {
			if (_reference_cast_to(_edit, elem)) {
				_edit->caret_changed += [this]() {
					thickness edit_padding = _edit->get_padding();
					rectd caret = _edit->get_formatted_text().get_character_placement(
						_edit->get_caret_selection().get_caret_position()
					).translated(vec2d(edit_padding.left, edit_padding.top));
					make_region_visible(caret);
				};
				_edit->text_changed += [this]() {
					// this is necessary: since text layout of _edit is dependent on its element layout. without
					// this, the caret_changed event won't be able to obtain correct caret positions
					get_manager().get_scheduler().update_element_layout_immediate(*_edit);
					clamp_to_valid_range();
				};
			}
			return true;
		}
		return scroll_view::_handle_reference(role, elem);
	}

	void textbox::_initialize() {
		_is_focus_scope = true;
		scroll_view::_initialize();
	}
}
