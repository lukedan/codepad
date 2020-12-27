// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/binary/contents_region.h"

/// \file
/// Implementation of the contents region of the binary editor.

#include "codepad/editors/manager.h"
#include "../details.h"

namespace codepad::editors::binary {
	void contents_region::set_buffer(std::shared_ptr<buffer> buf) {
		_unbind_buffer_events();
		_buf = std::move(buf);
		_carets.reset();
		if (_buf) {
			_mod_tok = (_buf->end_edit += [this](buffer::end_edit_info &info) {
				_on_end_edit(info);
				});
		}
		_on_content_modified();
	}

	void contents_region::on_text_input(std::u8string_view text) {
		constexpr unsigned char _spacebar = std::numeric_limits<unsigned char>::max();

		// first gather the input string
		std::basic_string<unsigned char> chars;
		std::size_t num_input_bytes = 1;
		for (auto it = text.begin(); it != text.end(); ) {
			codepoint current;
			if (encodings::utf8::next_codepoint(it, text.end(), current)) {
				if (current == u8' ') { // next codepoint
					chars.push_back(_spacebar);
					++num_input_bytes;
				} else { // possible value
					unsigned char val = get_character_value(current);
					if (val < static_cast<unsigned char>(get_radix())) { // is this character valid?
						chars.push_back(val);
					}
				}
			}
		}
		if (chars.empty()) {
			return;
		}

		// insert mode:
		//   spaces insert new bytes after the current one
		//   with selection: remove selected bytes, then add a new byte at that position
		// overwrite mode:
		//   spaces simply move the caret to the next byte
		//   with selection: the same as in insert mode
		{
			buffer::scoped_normal_modifier mod(*get_buffer(), this);
			for (auto &[caret, data] : get_carets().carets) {
				std::size_t mod_begin_pos = 0, mod_remove_len = 1;
				std::basic_string<std::byte> overwritten_bytes;
				if (caret.caret != caret.selection) {
					auto [min, max] = caret.get_range();
					mod_begin_pos = min + mod.get_modifier().get_fixup_offset();
					mod_remove_len = max - min;
				} else {
					mod_begin_pos = caret.caret + mod.get_modifier().get_fixup_offset();
					auto it = get_buffer()->at(mod_begin_pos);
					if (is_insert_mode()) {
						if (it.get_position() < get_buffer()->length()) {
							overwritten_bytes.push_back(*it);
						}
					} else {
						overwritten_bytes.reserve(num_input_bytes);
						for (std::size_t i = 0; i < num_input_bytes && it != get_buffer()->end(); ++i, ++it) {
							overwritten_bytes.push_back(*it);
						}
					}
					mod_remove_len = overwritten_bytes.size();
				}
				// update overwritten_bytes
				auto it = chars.begin();
				overwritten_bytes.resize(num_input_bytes);
				for (std::byte &b : overwritten_bytes) {
					for (; it != chars.end() && *it != _spacebar; ++it) {
						b = (b << 4) | static_cast<std::byte>(*it);
					}
					if (it != chars.end()) {
						++it;
					}
				}
				// modify
				mod.get_modifier().modify_nofixup(mod_begin_pos, mod_remove_len, std::move(overwritten_bytes));
			}
		}
	}

	std::size_t contents_region::hit_test_for_caret_document(vec2d pos) const {
		if (_buf) {
			pos.x = std::max(pos.x - get_padding().left, 0.0);
			pos.y = std::max(pos.y - get_padding().top, 0.0);
			std::size_t
				line = std::min(_get_line_at_position(pos.y), get_num_lines() - 1),
				col = std::min(static_cast<std::size_t>(
					(pos.x + 0.5 * get_blank_width()) / (_cached_max_byte_width + get_blank_width())
					), get_bytes_per_row()),
				res = line * get_bytes_per_row() + col;
			if (res >= _buf->length()) {
				return _buf->length();
			}
			return res;
		}
		return 0;
	}

	std::size_t contents_region::hit_test_for_caret(vec2d pos) const {
		if (auto *edt = editor::get_encapsulating(*this)) {
			return hit_test_for_caret_document(vec2d(
				pos.x + edt->get_horizontal_position() + get_padding().left,
				pos.y + edt->get_vertical_position() + get_padding().top
			));
		}
		return 0;
	}

	unsigned char contents_region::get_character_value(codepoint c1) {
		if (c1 >= u8'0' && c1 <= u8'9') {
			return static_cast<unsigned char>(c1 - u8'0');
		}
		if (c1 >= u8'a' && c1 <= u8'f') {
			return static_cast<unsigned char>(c1 - u8'a' + 10);
		}
		if (c1 >= u8'A' && c1 <= u8'F') {
			return static_cast<unsigned char>(c1 - u8'A' + 10);
		}
		return std::numeric_limits<unsigned char>::max();
	}

	std::basic_string_view<codepoint> contents_region::_get_hex_byte(std::byte b) {
		static const char32_t _lut[256][3]{
			U"00", U"01", U"02", U"03", U"04", U"05", U"06", U"07", U"08", U"09", U"0A", U"0B", U"0C", U"0D", U"0E", U"0F",
			U"10", U"11", U"12", U"13", U"14", U"15", U"16", U"17", U"18", U"19", U"1A", U"1B", U"1C", U"1D", U"1E", U"1F",
			U"20", U"21", U"22", U"23", U"24", U"25", U"26", U"27", U"28", U"29", U"2A", U"2B", U"2C", U"2D", U"2E", U"2F",
			U"30", U"31", U"32", U"33", U"34", U"35", U"36", U"37", U"38", U"39", U"3A", U"3B", U"3C", U"3D", U"3E", U"3F",
			U"40", U"41", U"42", U"43", U"44", U"45", U"46", U"47", U"48", U"49", U"4A", U"4B", U"4C", U"4D", U"4E", U"4F",
			U"50", U"51", U"52", U"53", U"54", U"55", U"56", U"57", U"58", U"59", U"5A", U"5B", U"5C", U"5D", U"5E", U"5F",
			U"60", U"61", U"62", U"63", U"64", U"65", U"66", U"67", U"68", U"69", U"6A", U"6B", U"6C", U"6D", U"6E", U"6F",
			U"70", U"71", U"72", U"73", U"74", U"75", U"76", U"77", U"78", U"79", U"7A", U"7B", U"7C", U"7D", U"7E", U"7F",
			U"80", U"81", U"82", U"83", U"84", U"85", U"86", U"87", U"88", U"89", U"8A", U"8B", U"8C", U"8D", U"8E", U"8F",
			U"90", U"91", U"92", U"93", U"94", U"95", U"96", U"97", U"98", U"99", U"9A", U"9B", U"9C", U"9D", U"9E", U"9F",
			U"A0", U"A1", U"A2", U"A3", U"A4", U"A5", U"A6", U"A7", U"A8", U"A9", U"AA", U"AB", U"AC", U"AD", U"AE", U"AF",
			U"B0", U"B1", U"B2", U"B3", U"B4", U"B5", U"B6", U"B7", U"B8", U"B9", U"BA", U"BB", U"BC", U"BD", U"BE", U"BF",
			U"C0", U"C1", U"C2", U"C3", U"C4", U"C5", U"C6", U"C7", U"C8", U"C9", U"CA", U"CB", U"CC", U"CD", U"CE", U"CF",
			U"D0", U"D1", U"D2", U"D3", U"D4", U"D5", U"D6", U"D7", U"D8", U"D9", U"DA", U"DB", U"DC", U"DD", U"DE", U"DF",
			U"E0", U"E1", U"E2", U"E3", U"E4", U"E5", U"E6", U"E7", U"E8", U"E9", U"EA", U"EB", U"EC", U"ED", U"EE", U"EF",
			U"F0", U"F1", U"F2", U"F3", U"F4", U"F5", U"F6", U"F7", U"F8", U"F9", U"FA", U"FB", U"FC", U"FD", U"FE", U"FF"
		};
		static_assert(sizeof(codepoint) == sizeof(char32_t));
		return std::basic_string_view<codepoint>(
			reinterpret_cast<const codepoint*>(_lut[static_cast<std::size_t>(b)]), 2
			);
	}

	rectd contents_region::_get_caret_rect(std::size_t cpos) const {
		if (cpos == _buf->length()) { // caret is at the end of the file
			// in this case, make the caret a vertical line
			if (cpos == 0) { // empty document
				return rectd::from_xywh(_get_column_offset(0), _get_line_offset(0), 0.0f, get_line_height());
			}
			// otherwise put caret after the last byte
			auto [line, col] = _get_line_and_column_of_byte(cpos - 1);
			return rectd::from_xywh(
				_get_column_offset(col) + _cached_max_byte_width, _get_line_offset(line), 0.0f, get_line_height()
			);
		}
		// otherwise, make the caret cover the entire byte
		auto [line, col] = _get_line_and_column_of_byte(cpos);
		return rectd::from_xywh(
			_get_column_offset(col),
			_get_line_offset(line),
			_cached_max_byte_width,
			get_line_height()
		);
	}

	std::vector<rectd> contents_region::_get_selection_rects(
		ui::caret_selection sel, std::size_t clampmin, std::size_t clampmax
	) const {
		if (sel.caret == sel.selection) { // no selection for single byte
			return {};
		}
		auto range = sel.get_range();
		std::size_t
			beg = std::max(range.first, clampmin),
			end = std::min(range.second, clampmax);
		if (beg >= end) { // not visible
			return {};
		}
		std::vector<rectd> res;
		auto [bline, bcol] = _get_line_and_column_of_byte(beg);
		auto [eline, ecol] = _get_line_and_column_of_byte(end);
		if (ecol == 0 && eline != 0) { // move end to the end of the last line if it's at the beginning
			--eline;
			ecol = get_bytes_per_row();
		}
		double
			y = _get_line_offset(bline),
			lh = get_line_height();
		if (bline == eline) { // on the same line
			res.emplace_back(_get_column_offset(bcol), _get_column_offset(ecol) - get_blank_width(), y, y + lh);
		} else {
			double
				colbeg = _get_column_offset(0),
				colend = _get_column_offset(get_bytes_per_row()) - get_blank_width();
			res.emplace_back(_get_column_offset(bcol), colend, y, y + lh);
			y += lh;
			for (std::size_t i = bline + 1; i < eline; ++i, y += lh) { // whole lines
				res.emplace_back(colbeg, colend, y, y + lh);
			}
			res.emplace_back(colbeg, _get_column_offset(ecol) - get_blank_width(), y, y + lh);
		}
		return res;
	}

	void contents_region::_custom_render() const {
		interactive_contents_region_base::_custom_render();

		if (auto *edt = editor::get_encapsulating(*this)) {
			std::size_t
				firstline = _get_line_at_position(edt->get_vertical_position()),
				// first byte in a line
				firstbyte = _get_column_at_position(edt->get_horizontal_position()),
				// past last byte in a line
				lastbyte = std::min(
					get_bytes_per_row(),
					static_cast<std::size_t>(
						(edt->get_horizontal_position() + get_layout().width()) /
						(_cached_max_byte_width + get_blank_width())
						) + 1
				);
			double
				lineh = get_line_height(),
				bottom = get_layout().height();
			vec2d
				edtpos = edt->get_position(),
				topleft = vec2d(_get_column_offset(firstbyte), _get_line_offset(firstline)) - edtpos;
			auto &renderer = get_manager().get_renderer();

			{
				renderer.push_rectangle_clip(rectd::from_corners(vec2d(), get_layout().size()));

				// render bytes
				for (std::size_t line = firstline; topleft.y < bottom; topleft.y += lineh, ++line) {
					// render a single line
					std::size_t pos = line * get_bytes_per_row() + firstbyte;
					if (pos >= _buf->length()) {
						break;
					}
					auto it = _buf->at(pos);
					double x = topleft.x;
					for (
						std::size_t i = firstbyte;
						i < lastbyte && it != _buf->end();
						++i, ++it, x += _cached_max_byte_width + _blank_width
						) {
						auto text = renderer.create_plain_text_fast(_get_hex_byte(*it), *_font, _font_size);
						// TODO customizable color
						renderer.draw_plain_text(
							*text,
							vec2d(x + 0.5 * (_cached_max_byte_width - text->get_width()), topleft.y),
							colord()
						);
					}
				}
				// render carets
				caret_set extcarets;
				const caret_set *cset = &_carets;
				std::vector<ui::caret_selection> tmpcarets = _interaction_manager.get_temporary_carets();
				if (!tmpcarets.empty()) { // merge & use temporary caret set
					extcarets = _carets;
					for (const auto &caret : tmpcarets) {
						extcarets.add(caret_set::entry(caret, caret_data()));
					}
					cset = &extcarets;
				}
				auto it = cset->carets.lower_bound({ firstline * get_bytes_per_row() + firstbyte, 0 });
				if (it != cset->carets.begin()) {
					--it;
				}
				std::size_t
					clampmin = firstline * get_bytes_per_row(),
					clampmax = (
						_get_line_at_position(edt->get_vertical_position() + get_layout().height()) + 1
						) * get_bytes_per_row();
				std::vector<rectd> caret_rects;
				std::vector<std::vector<rectd>> selection_rects;
				for (; it != cset->carets.end(); ++it) {
					if (it->first.caret > clampmax && it->first.selection > clampmax) { // out of visible scope
						break;
					}
					caret_rects.emplace_back(_get_caret_rect(it->first.caret).translated(-edtpos));
					selection_rects.emplace_back(_get_selection_rects(it->first, clampmin, clampmax));
					for (rectd &part : selection_rects.back()) {
						part = part.translated(-edtpos);
					}
				}
				for (const rectd &r : caret_rects) {
					_caret_visuals.render(r, renderer);
				}
				vec2d unit = get_layout().size();
				for (auto &v : selection_rects) {
					code_selection_renderer()->render(
						renderer, v, _selection_brush.get_parameters(unit), _selection_pen.get_parameters(unit)
					);
				}

				renderer.pop_clip();
			}
		}
	}

	bool contents_region::_update_bytes_per_row() {
		std::size_t target = _cached_bytes_per_row;
		if (_wrap == wrap_mode::fixed) {
			target = _target_bytes_per_row;
		} else { // automatic
			std::size_t max = static_cast<std::size_t>(std::max(
				(get_client_region().width() + _blank_width) / (_cached_max_byte_width + _blank_width), 1.0
			)); // no need for std::floor
			if (_wrap == wrap_mode::auto_power2) {
				target = std::size_t(1) << high_bit_index(max);
			} else {
				target = max;
			}
		}
		if (target != _cached_bytes_per_row) {
			_cached_bytes_per_row = target;
			_on_content_visual_changed();
			return true;
		}
		return false;
	}

	void contents_region::_on_end_edit(buffer::end_edit_info &info) {
		// fixup carets
		buffer::position_patcher patcher(info.positions);
		caret_set newcarets;
		if (info.source_element == this && info.type == edit_type::normal) {
			for (const auto &pos : info.positions) {
				newcarets.add(caret_set::entry(
					pos.position + std::max<std::size_t>(pos.added_range, 1) - 1, caret_data()
				));
			}
		} else {
			for (const auto &cp : _carets.carets) {
				ui::caret_selection newpos;
				if (cp.first.caret < cp.first.selection) {
					newpos.caret = patcher.patch_next<buffer::position_patcher::strategy::back>(cp.first.caret);
					newpos.selection =
						patcher.patch_next<buffer::position_patcher::strategy::back>(cp.first.selection);
				} else {
					newpos.caret = patcher.patch_next<buffer::position_patcher::strategy::back>(cp.first.caret);
					newpos.selection =
						patcher.patch_next<buffer::position_patcher::strategy::back>(cp.first.selection);
				}
				newcarets.add(caret_set::entry(newpos, cp.second));
			}
		}
		set_carets(std::move(newcarets));

		_on_content_modified();
	}

	void contents_region::_on_logical_parent_constructed() {
		element::_on_logical_parent_constructed();

		auto *edt = editor::get_encapsulating(*this);
		edt->horizontal_viewport_changed += [this]() {
			_interaction_manager.on_viewport_changed();
		};
		edt->vertical_viewport_changed += [this]() {
			_interaction_manager.on_viewport_changed();
		};
	}

	bool contents_region::_register_event(std::u8string_view name, std::function<void()> callback) {
		return
			_event_helpers::try_register_event(name, u8"carets_changed", carets_changed, callback) ||
			interactive_contents_region_base::_register_event(name, std::move(callback));
	}

	void contents_region::_initialize(std::u8string_view cls) {
		element::_initialize(cls);

		std::vector<std::u8string> profile{ u8"binary" };

		auto &set = get_manager().get_settings();
		set_font_by_name(
			editor::get_font_family_setting(set).get_profile(profile.begin(), profile.end()).get_value()
		);
		set_font_size(
			editor::get_font_size_setting(set).get_profile(profile.begin(), profile.end()).get_value()
		);

		// initialize _interaction_manager
		_interaction_manager.set_contents_region(*this);
		auto &modes = editor::get_interaction_modes_setting(set).get_profile(
			profile.begin(), profile.end()
		).get_value();
		for (auto &&mode_name : modes) {
			if (auto mode = editors::_details::get_manager().binary_interactions.try_create(mode_name)) {
				_interaction_manager.activators().emplace_back(std::move(mode));
			}
		}
	}

	void contents_region::_dispose() {
		_unbind_buffer_events();
		element::_dispose();
	}


	namespace component_helper {
		std::pair<editor*, contents_region*> get_core_components(const ui::element &elem) {
			editor *edt = editor::get_encapsulating(elem);
			if (edt) {
				return { edt, contents_region::get_from_editor(*edt) };
			}
			return { nullptr, nullptr };
		}
	}
}
