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
		auto it = text.begin();
		while (it != text.end()) {
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
					caret_rects.emplace_back(_get_caret_rect(it->first.caret));
					selection_rects.emplace_back(_get_selection_rects(it->first, clampmin, clampmax));
				}
				// TODO proper visuals
				for (const rectd &r : caret_rects) {
					renderer.draw_rectangle(
						r.translated(-edtpos),
						ui::generic_brush_parameters(),
						ui::generic_pen_parameters(ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord())))
					);
				}
				for (const auto &v : selection_rects) {
					for (const rectd &r : v) {
						renderer.draw_rectangle(
							r.translated(-edtpos),
							ui::generic_brush_parameters(ui::brush_parameters::solid_color(colord(1.0, 1.0, 1.0, 0.2))),
							ui::generic_pen_parameters()
						);
					}
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

	void contents_region::_initialize(std::u8string_view cls, const ui::element_configuration &config) {
		element::_initialize(cls, config);

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
