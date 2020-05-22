// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "contents_region.h"

/// \file
/// Implementation of the contents region of the binary editor.

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

	std::u8string_view contents_region::_get_hex_byte(std::byte b) {
		static const char8_t _lut[256][3]{
			u8"00", u8"01", u8"02", u8"03", u8"04", u8"05", u8"06", u8"07", u8"08", u8"09", u8"0A", u8"0B", u8"0C", u8"0D", u8"0E", u8"0F",
			u8"10", u8"11", u8"12", u8"13", u8"14", u8"15", u8"16", u8"17", u8"18", u8"19", u8"1A", u8"1B", u8"1C", u8"1D", u8"1E", u8"1F",
			u8"20", u8"21", u8"22", u8"23", u8"24", u8"25", u8"26", u8"27", u8"28", u8"29", u8"2A", u8"2B", u8"2C", u8"2D", u8"2E", u8"2F",
			u8"30", u8"31", u8"32", u8"33", u8"34", u8"35", u8"36", u8"37", u8"38", u8"39", u8"3A", u8"3B", u8"3C", u8"3D", u8"3E", u8"3F",
			u8"40", u8"41", u8"42", u8"43", u8"44", u8"45", u8"46", u8"47", u8"48", u8"49", u8"4A", u8"4B", u8"4C", u8"4D", u8"4E", u8"4F",
			u8"50", u8"51", u8"52", u8"53", u8"54", u8"55", u8"56", u8"57", u8"58", u8"59", u8"5A", u8"5B", u8"5C", u8"5D", u8"5E", u8"5F",
			u8"60", u8"61", u8"62", u8"63", u8"64", u8"65", u8"66", u8"67", u8"68", u8"69", u8"6A", u8"6B", u8"6C", u8"6D", u8"6E", u8"6F",
			u8"70", u8"71", u8"72", u8"73", u8"74", u8"75", u8"76", u8"77", u8"78", u8"79", u8"7A", u8"7B", u8"7C", u8"7D", u8"7E", u8"7F",
			u8"80", u8"81", u8"82", u8"83", u8"84", u8"85", u8"86", u8"87", u8"88", u8"89", u8"8A", u8"8B", u8"8C", u8"8D", u8"8E", u8"8F",
			u8"90", u8"91", u8"92", u8"93", u8"94", u8"95", u8"96", u8"97", u8"98", u8"99", u8"9A", u8"9B", u8"9C", u8"9D", u8"9E", u8"9F",
			u8"A0", u8"A1", u8"A2", u8"A3", u8"A4", u8"A5", u8"A6", u8"A7", u8"A8", u8"A9", u8"AA", u8"AB", u8"AC", u8"AD", u8"AE", u8"AF",
			u8"B0", u8"B1", u8"B2", u8"B3", u8"B4", u8"B5", u8"B6", u8"B7", u8"B8", u8"B9", u8"BA", u8"BB", u8"BC", u8"BD", u8"BE", u8"BF",
			u8"C0", u8"C1", u8"C2", u8"C3", u8"C4", u8"C5", u8"C6", u8"C7", u8"C8", u8"C9", u8"CA", u8"CB", u8"CC", u8"CD", u8"CE", u8"CF",
			u8"D0", u8"D1", u8"D2", u8"D3", u8"D4", u8"D5", u8"D6", u8"D7", u8"D8", u8"D9", u8"DA", u8"DB", u8"DC", u8"DD", u8"DE", u8"DF",
			u8"E0", u8"E1", u8"E2", u8"E3", u8"E4", u8"E5", u8"E6", u8"E7", u8"E8", u8"E9", u8"EA", u8"EB", u8"EC", u8"ED", u8"EE", u8"EF",
			u8"F0", u8"F1", u8"F2", u8"F3", u8"F4", u8"F5", u8"F6", u8"F7", u8"F8", u8"F9", u8"FA", u8"FB", u8"FC", u8"FD", u8"FE", u8"FF"
		};
		return std::u8string_view(_lut[static_cast<std::size_t>(b)], 2);
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
		caret_selection sel, std::size_t clampmin, std::size_t clampmax
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
				ui::pixel_snapped_render_target buffer(
					renderer,
					rectd::from_corners(vec2d(), get_layout().size()),
					get_window()->get_scaling_factor()
				);

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
						auto text = renderer.create_plain_text(_get_hex_byte(*it), *_font, _font_size);
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
				std::vector<caret_selection> tmpcarets = _interaction_manager.get_temporary_carets();
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
					_caret_visuals.render(r.translated(-edtpos), renderer);
				}
				vec2d unit = get_layout().size();
				for (auto &v : selection_rects) {
					code_selection_renderer()->render(
						renderer, v, _selection_brush.get_parameters(unit), _selection_pen.get_parameters(unit)
					);
				}
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
				caret_selection newpos;
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
			if (auto mode = get_interaction_mode_registry().try_create(mode_name)) {
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
