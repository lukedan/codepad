// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/ui/elements/text_edit.h"

/// \file
/// Implementation of \ref codepad::ui::text_edit.

#include "codepad/ui/manager.h"

namespace codepad::ui {
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
			_caret = hit.rear ? hit.character + 1 : hit.character;
			_on_caret_changed();
		}
	}

	void text_edit::_on_mouse_down(mouse_button_info &info) {
		label::_on_mouse_down(info);
		if (info.button == mouse_button::primary) {
			assert_true_logical(!_selecting, "mouse pressed again when selecting");
			auto hit = _hit_test_for_caret(info.position.get(*this));
			_caret = _selection_end = hit.rear ? hit.character + 1 : hit.character;
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

		// find range to erase first
		auto [sel_beg, sel_end] = std::minmax(_caret, _selection_end);
		auto it_beg = _text.begin();
		std::size_t i = 0;
		bool all_valid = false; // all codepoints are valid
		for (; i < sel_beg && it_beg != _text.end(); ++i) {
			all_valid = encodings::utf8::next_codepoint(it_beg, _text.end()) && all_valid;
		}
		auto it_end = it_beg;
		for (; i < sel_end && it_end != _text.end(); ++i) {
			all_valid = encodings::utf8::next_codepoint(it_end, _text.end()) && all_valid;
		}
		// erase & insert
		if (it_beg != it_end) {
			it_beg = _text.erase(it_beg, it_end);
		}
		it_beg = _text.insert(it_beg, info.content.begin(), info.content.end());
		std::size_t new_caret = sel_beg;
		// compute new caret position
		auto it_target = it_beg + info.content.size();
		if (!all_valid) {
			it_beg = _text.begin();
			new_caret = 0;
		}
		while (it_beg < it_target) {
			encodings::utf8::next_codepoint(it_beg, _text.end());
			++new_caret;
		}
		_caret = _selection_end = new_caret;

		_on_text_changed();
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
				_cached_fmt->get_character_placement(_caret).translated(get_client_region().xmin_ymin())
			);
		}
	}

	void text_edit::_custom_render() const {
		label::_custom_render();
		// label::_on_prerender() calls _check_cache_format(), so no need to call again here
		rectd cursor = _cached_fmt->get_character_placement(_caret);
		_caret_visuals.render(cursor, get_manager().get_renderer());
		if (_caret != _selection_end) { // render selection
			auto [sel_min, sel_max] = std::minmax(_caret, _selection_end);
			std::vector<rectd> rs = _cached_fmt->get_character_range_placement(sel_min, sel_max - sel_min);
			for (rectd r : rs) {
				_selection_visuals.render(r, get_manager().get_renderer());
			}
		}
	}
}
