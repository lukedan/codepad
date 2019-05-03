// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of \ref codepad::editors::binary::contents_region.

#include <algorithm>
#include <string_view>

#include "../../ui/element.h"
#include "../buffer.h"
#include "../caret_set.h"
#include "../interaction_modes.h"
#include "../editor.h"

namespace codepad::editors::binary {
	/// The data associated with a caret.
	struct caret_data {
		/// Default constructor.
		caret_data() = default;
		/// Initializes \ref next_line.
		explicit caret_data(bool nl) : next_line(nl) {
		}

		/// Indicates if this caret should be put at the beginning of the next line if it's at the end of a line.
		bool next_line = false;
	};

	/// A set of carets.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	};

	/// The \ref ui::element that displays the contents of the \ref buffer and handles user interactions.
	class contents_region : public interactive_contents_region_base<caret_set> {
	public:
		/// How this display should be wrapped.
		enum class wrap_mode : unsigned char {
			fixed, ///< Wrap at \ref _target_bytes_per_row.
			auto_fill, ///< Wrap when out of the boundary at any place.
			auto_power2 ///< Wrap when out of the boundary, but only at line lengths that are powers of two.
		};

		/// Returns the \ref buffer that's currently being edited.
		const std::shared_ptr<buffer> &get_buffer() const {
			return _buf;
		}
		/// Sets the \ref buffer that's being edited.
		void set_buffer(std::shared_ptr<buffer> buf) {
			_unbind_buffer_events();
			_buf = std::move(buf);
			_carets.reset();
			if (_buf) {
				_mod_tok = (_buf->end_edit += [this](buffer::end_edit_info&) {
					_on_content_modified();
				});
			}
			_on_content_modified();
		}

		/// Returns the current set of carets.
		const caret_set &get_carets() const override {
			return _carets;
		}
		/// Sets the current set of carets. This function scrolls the viewport so that the first caret is visible.
		void set_carets(caret_set set) {
			assert_true_usage(!set.carets.empty(), "must have at least one caret");
			_carets = std::move(set);
			/*_make_caret_visible(caret_position(*_carets.carets.rbegin()));*/ // TODO
			_on_carets_changed();
		}

		/// Adds the given caret to the \ref contents_region.
		void add_caret(caret_selection_position caret) override {
			_carets.add(caret_set::entry(
				caret_selection(caret.caret, caret.selection), caret_data(caret.caret_at_back)
			));
			_on_carets_changed();
		}
		/// Removes the given caret.
		void remove_caret(caret_set::const_iterator it) override {
			_carets.carets.erase(it);
			_on_carets_changed();
		}
		/// Clears all carets from the \ref contents_region.
		void clear_carets() override {
			_carets.carets.clear();
			_on_carets_changed();
		}
		/// Extracts a \ref caret_selection_position.
		caret_selection_position extract_caret_selection_position(const caret_set::entry &et) const override {
			return caret_selection_position(et.first.first, et.first.second, et.second.next_line);
		}

		/// Returns whether the \ref contents_region is currently in insert mode.
		bool is_insert_mode() const {
			return _insert;
		}
		/// Sets whether the \ref contents_region is currently in insert mode.
		void set_insert_mode(bool v) {
			_insert = v;
			_reset_caret_animation();
		}
		/// Toggles insert mode.
		void toggle_insert_mode() {
			set_insert_mode(!is_insert_mode());
		}

		/// Returns the height of a line.
		///
		/// \todo Add customizable line height.
		double get_line_height() const {
			/*return _font ? _font->height() : 0.0;*/
			return 0.0;
		}
		/// Returns the number of lines.
		size_t get_num_lines() const {
			if (_buf) {
				size_t bytes = get_bytes_per_row();
				return (std::max<size_t>(_buf->length(), 1) + bytes - 1) / bytes;
			}
			return 0;
		}

		/// Returns the length of scrolling by one tick.
		double get_vertical_scroll_delta() const override {
			return get_line_height() * _lines_per_scroll;
		}
		/// Returns the length of scrolling by one tick. Currently the same value as that returned by
		/// \ref get_vertical_scroll_delta().
		double get_horizontal_scroll_delta() const override {
			return get_vertical_scroll_delta();
		}
		/// Returns the vertical viewport range.
		double get_vertical_scroll_range() const override {
			return
				get_line_height() * static_cast<double>(get_num_lines() - 1) +
				get_layout().height() + get_padding().top;
		}
		/// Returns the horizontal viewport range.
		double get_horizontal_scroll_range() const override {
			return
				get_bytes_per_row() * (_cached_max_byte_width + get_blank_width()) - get_blank_width() +
				get_padding().width();
		}

		/// Returns the current \ref wrap_mode.
		wrap_mode get_wrap_mode() const {
			return _wrap;
		}
		/// Sets the wrap mode.
		void set_wrap_mode(wrap_mode w) {
			if (_wrap != w) {
				_wrap = w;
				_update_bytes_per_row();
			}
		}

		/// Returns the actual current number of bytes per row.
		size_t get_bytes_per_row() const {
			return _cached_bytes_per_row;
		}
		/// Sets the desired bytes per row. This value is only used when \ref _wrap is \ref wrap_mode::fixed.
		void set_bytes_per_row(size_t val) {
			_target_bytes_per_row = val;
			_update_bytes_per_row();
		}

		/// Returns the current minimum width of blanks between bytes.
		double get_blank_width() const {
			return _blank_width;
		}
		/// Sets the minimum width of blanks between bytes.
		void set_blank_width(double w) {
			_blank_width = w;
			if (!_update_bytes_per_row()) {
				_on_content_visual_changed();
			}
		}

		/*/// Returns the \ref ui::font for rendering characters.
		const std::shared_ptr<const ui::font> &get_font() const {
			return _font;
		}
		/// Sets the \ref ui::font for rendering characters.
		void set_font(std::shared_ptr<const ui::font> f) {
			_font = f;
			_cached_max_byte_width = _font->get_max_width_charset(U"0123456789ABCDEF") * 2;
			if (!_update_bytes_per_row()) {
				_on_content_visual_changed();
			}
		}*/

		/// Sets the number of lines to scroll per `tick'.
		void set_num_lines_per_scroll(double v) {
			_lines_per_scroll = v;
		}
		/// Sets the number of lines to scroll per `tick'.
		double get_num_lines_per_scroll() const {
			return _lines_per_scroll;
		}

		/// Returns the overriden cursor if there is one, otherwise returns the `I'-beam cursor.
		ui::cursor get_current_display_cursor() const override {
			if (ui::cursor c = _interaction_manager.get_override_cursor(); c != ui::cursor::not_specified) {
				return c;
			}
			return ui::cursor::text_beam;
		}

		/// Catches all characters that are valid in hexadecimal, and modifies the contents of the \ref buffer.
		void on_text_input(str_view_t t) override {
			// TODO
		}

		/// Returns the \ref caret_position correponding to the given mouse position. The mouse position is relative
		/// to the top left corner of the document.
		caret_position hit_test_for_caret_document(vec2d pos) const {
			if (_buf) {
				pos.x = std::max(pos.x - get_padding().left, 0.0);
				pos.y = std::max(pos.y - get_padding().top, 0.0);
				size_t
					line = std::min(_get_line_at_position(pos.y), get_num_lines() - 1),
					col = std::min(static_cast<size_t>(
						0.5 + (pos.x + 0.5 * get_blank_width()) / (_cached_max_byte_width + get_blank_width())
						), get_bytes_per_row()),
					res = line * get_bytes_per_row() + col;
				if (res >= _buf->length()) {
					return caret_position(_buf->length(), false);
				}
				return caret_position(res, col == 0);
			}
			return caret_position();
		}
		/// Similar to \ref hit_test_for_caret_document(), only that the position is relative to the top left corner
		/// of this \ref ui::element.
		caret_position hit_test_for_caret(vec2d pos) const override {
			if (auto *edt = editor::get_encapsulating(*this)) {
				return hit_test_for_caret_document(pos + edt->get_position());
			}
			return caret_position();
		}

		info_event<>
			content_modified, ///< Invoked when the \ref buffer is modified or changed by \ref set_buffer.
			/// Invoked when the set of carets is changed. Note that this does not necessarily mean that the result
			/// of \ref get_carets will change.
			carets_changed;

		/// Converts a character into the corresponding hexadecimal value.
		inline static int get_hex_value(codepoint c1) {
			if (c1 >= '0' && c1 <= '9') {
				return c1 - '0';
			}
			if (c1 >= 'a' && c1 <= 'f') {
				return c1 - 'a' + 10;
			}
			if (c1 >= 'A' && c1 <= 'F') {
				return c1 - 'A' + 10;
			}
			return 0;
		}

		/// Casts the obtained \ref components_region_base to the correct type.
		inline static contents_region *get_from_editor(editor &edt) {
			return dynamic_cast<contents_region*>(edt.get_contents_region());
		}

		/// Returns the default class used by elements of type \ref contents_region.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("binary_contents_region");
		}
		/// Returns the class used by carets under `insert' mode.
		inline static str_view_t get_insert_caret_class() {
			return CP_STRLIT("binary_insert_caret");
		}
		/// Returns the class used by carets under `overwrite' mode.
		inline static str_view_t get_overwrite_caret_class() {
			return CP_STRLIT("binary_overwrite_caret");
		}
		/// Returns the class used by selected regions.
		inline static str_view_t get_contents_region_selection_class() {
			return CP_STRLIT("binary_selection");
		}
	protected:
		/// Extracts a \ref caret_position from the given \ref caret_set::entry.
		inline static caret_position _extract_position(const caret_set::entry &entry) {
			return caret_position(entry.first.first, entry.second.next_line);
		}
		/// Returns the hexadecimal representation of the given byte.
		inline static str_view_t _get_hex_byte(std::byte b) {
			static const char _lut[256][3]{
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

			return str_view_t(_lut[static_cast<unsigned char>(b)], 2);
		}

		caret_set _carets; ///< The set of carets.
		interaction_manager<caret_set> _interaction_manager; ///< Manages certain mouse and keyboard interactions.
		std::shared_ptr<buffer> _buf; ///< The buffer that's being edited.
		info_event<buffer::end_edit_info>::token _mod_tok; ///< Used to listen to \ref buffer::
		double
			_cached_max_byte_width = 0.0, ///< The width of a character.
			_blank_width = 5.0, ///< The distance between two consecutive bytes.
			_lines_per_scroll = 3.0; ///< The number of lines to scroll per `tick'.
		size_t
			/// The number of bytes per row. Only used when \ref _wrap is \ref wrap_mode::fixed.
			_target_bytes_per_row = 16,
			_cached_bytes_per_row = 16; ///< The cached actual number of bytes per row.
		wrap_mode _wrap = wrap_mode::auto_fill; ///< Indicates how the bytes should be wrapped.
		/// Indicates whether this \ref contents_region is in `insert' mode or in `overwrite' mode.
		bool _insert = true;

		// position conversion
		/// Returns the line index at the given position, relative to the top of this document, i.e., without
		/// scrolling. This function returns 0 if the position lies before the first line.
		size_t _get_line_at_position(double pos) const {
			return static_cast<size_t>(std::max((pos - get_padding().top) / get_line_height(), 0.0));
		}
		/// Returns the vertical offset of the top of the given line, relative to the top of the document.
		double _get_line_offset(size_t line) const {
			return get_padding().top + get_line_height() * line;
		}

		/// Returns the index of the first visible byte at the given horizontal position.
		size_t _get_column_at_position(double pos) const {
			return std::min(
				get_bytes_per_row(),
				static_cast<size_t>((pos + get_blank_width()) / (_cached_max_byte_width + get_blank_width()))
			);
		}
		/// Returns the horizontal offset of the left of the given column, relative to the left of the document.
		double _get_column_offset(size_t x) const {
			return get_padding().left + (_cached_max_byte_width + get_blank_width()) * x;
		}

		/// Returns the line and column that the given byte is on.
		std::pair<size_t, size_t> _get_line_and_column_of_byte(size_t byte) const {
			size_t bpr = get_bytes_per_row(), line = byte / bpr;
			return {line, byte - line * bpr};
		}

		// rendering
		/// Returns the region that a given caret occupies, relative to the top left of the document.
		rectd _get_caret_rect(caret_position cpos) const {
			auto[line, col] = _get_line_and_column_of_byte(cpos.position);
			if (col == 0) { // at "line ending"
				if (cpos.at_back || line == 0) { // at the beginning of the next line
					return rectd::from_xywh(_get_column_offset(0), _get_line_offset(line), 0.0, get_line_height());
				} // at the end of the previous line
				return rectd::from_xywh(
					_get_column_offset(get_bytes_per_row()) - get_blank_width(), _get_line_offset(line - 1),
					0.0, get_line_height()
				);
			} // occupy the blank between bytes
			return rectd::from_xywh(
				_get_column_offset(col) - get_blank_width(),
				_get_line_offset(line),
				get_blank_width(),
				get_line_height()
			);
		}
		/// Returns the rectangles that a selected region covers. The selection is clamped by the given parameters to
		/// reduce unnecessary regions. The resulting regions are placed relative to the top left of the document.
		std::vector<rectd> _get_selection_rects(caret_selection sel, size_t clampmin, size_t clampmax) const {
			auto pair = std::minmax(sel.first, sel.second);
			size_t beg = std::max(pair.first, clampmin), end = std::min(pair.second, clampmax);
			if (beg >= end) { // not visible
				return {};
			}
			std::vector<rectd> res;
			auto[bline, bcol] = _get_line_and_column_of_byte(beg);
			auto[eline, ecol] = _get_line_and_column_of_byte(end);
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
				for (size_t i = bline + 1; i < eline; ++i, y += lh) { // whole lines
					res.emplace_back(colbeg, colend, y, y + lh);
				}
				res.emplace_back(colbeg, _get_column_offset(ecol) - get_blank_width(), y, y + lh);
			}
			return res;
		}

		/// Renders all visible bytes.
		void _custom_render() const override {
			interactive_contents_region_base::_custom_render();
			/*
			if (auto *edt = editor::get_encapsulating(*this)) {
				size_t
					firstline = _get_line_at_position(edt->get_vertical_position()),
					// first byte in a line
					firstbyte = _get_column_at_position(edt->get_horizontal_position()),
					// past last byte in a line
					lastbyte = std::min(
						get_bytes_per_row(),
						static_cast<size_t>(
						(edt->get_horizontal_position() + get_layout().width()) /
							(_cached_max_byte_width + get_blank_width())
							) + 1
					);
				double
					lineh = get_line_height(),
					bottom = get_client_region().ymax;
				vec2d
					edtpos = edt->get_position(),
					topleft = get_layout().xmin_ymin() + vec2d(
						// from left of view to left of the first byte
						_get_column_offset(firstbyte),
						// from top of view to top of the first line
						_get_line_offset(firstline)
					) - edtpos;
				// render bytes
				ui::atlas::batch_renderer rend(_font->get_manager().get_atlas());
				for (size_t line = firstline; topleft.y < bottom; topleft.y += lineh, ++line) {
					// render a single line
					size_t pos = line * get_bytes_per_row() + firstbyte;
					if (pos >= _buf->length()) {
						break;
					}
					auto it = _buf->at(pos);
					double x = topleft.x;
					for (
						size_t i = firstbyte;
						i < lastbyte && it != _buf->end();
						++i, ++it, x += _cached_max_byte_width + _blank_width
						) {
						// TODO customizable color
						ui::text_renderer::render_plain_text(
							_get_hex_byte(*it), *_font, vec2d(x, topleft.y), colord(), rend
						);
					}
				}
				// render carets
				caret_set extcarets;
				caret_set *cset = &_carets;
				std::vector<caret_selection_position> tmpcarets = _interaction_manager.get_temporary_carets();
				if (!tmpcarets.empty()) { // merge & use temporary caret set
					extcarets = _carets;
					for (const auto &caret : tmpcarets) {
						extcarets.add(caret_set::entry(
							caret_selection(caret.caret, caret.selection), caret_data(caret.caret_at_back))
						);
					}
					cset = &extcarets;
				}
				auto it = cset->carets.lower_bound({firstline * get_bytes_per_row() + firstbyte, 0});
				if (it != cset->carets.begin()) {
					--it;
				}
				size_t
					clampmin = firstline * get_bytes_per_row(),
					clampmax = (
						_get_line_at_position(edt->get_vertical_position() + get_layout().height()) + 1
						) * get_bytes_per_row();
				std::vector<rectd> caret_rects;
				std::vector<std::vector<rectd>> selection_rects;
				for (; it != cset->carets.end(); ++it) {
					if (it->first.first > clampmax && it->first.second > clampmax) { // out of visible scope
						break;
					}
					caret_rects.emplace_back(_get_caret_rect(_extract_position(*it)));
					selection_rects.emplace_back(_get_selection_rects(it->first, clampmin, clampmax));
				}
				vec2d renderoffset = get_layout().xmin_ymin() - edtpos;
				for (const rectd &r : caret_rects) {
					_caret_cfg.render(get_manager().get_renderer(), r.translated(renderoffset));
				}
				for (const auto &v : selection_rects) {
					for (const rectd &r : v) {
						_selection_cfg.render(get_manager().get_renderer(), r.translated(renderoffset));
					}
				}
			}
			*/
		}

		// formatting
		/// Updates the cached value \ref _cached_bytes_per_row. This function invokes
		/// \ref _on_content_visual_changed() when necessary.
		///
		/// \return Whether the number of bytes per row has changed. This also indicates whether
		///         \ref _on_content_visual_changed() has been called.
		bool _update_bytes_per_row() {
			size_t target = _cached_bytes_per_row;
			if (_wrap == wrap_mode::fixed) {
				target = _target_bytes_per_row;
			} else { // automatic
				size_t max = static_cast<size_t>(std::max(
					(get_client_region().width() + _blank_width) / (_cached_max_byte_width + _blank_width), 1.0
				)); // no need for std::floor
				if (_wrap == wrap_mode::auto_power2) {
					target = size_t(1) << high_bit_index(max);
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

		/// Calls \ref _update_bytes_per_row().
		void _on_layout_changed() override {
			_update_bytes_per_row();
			element::_on_layout_changed();
		}

		// user interactions
		/// Forwards the event to \ref _interaction_manager.
		void _on_mouse_down(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_down(info);
			element::_on_mouse_down(info);
		}
		/// Forwards the event to \ref _interaction_manager.
		void _on_mouse_up(ui::mouse_button_info &info) override {
			_interaction_manager.on_mouse_up(info);
			element::_on_mouse_up(info);
		}
		/// Forwards the event to \ref _interaction_manager.
		void _on_mouse_move(ui::mouse_move_info &info) override {
			_interaction_manager.on_mouse_move(info);
			element::_on_mouse_move(info);
		}
		/// Forwards the event to \ref _interaction_manager.
		void _on_capture_lost() override {
			_interaction_manager.on_capture_lost();
			element::_on_capture_lost();
		}
		/// Forwards the event to \ref _interaction_manager.
		void _on_update() override {
			_interaction_manager.on_update();
			element::_on_update();
		}

		// callbacks
		/// Unbinds all events from the \ref buffer, if necessary.
		void _unbind_buffer_events() {
			if (_buf) {
				_buf->end_edit -= _mod_tok;
			}
		}

		/// Simply calls \ref _on_carets_changed().
		void _on_temporary_carets_changed() override {
			_on_carets_changed();
		}

		/// Called when \ref _buf has been modified. Calls \ref _on_content_visual_changed().
		void _on_content_modified() {
			_on_content_visual_changed();
			content_modified.invoke();
		}
		/// Called when \ref content_visual_changed should be invoked.
		void _on_content_visual_changed() {
			invalidate_visual();
			content_visual_changed.invoke();
		}
		/// Called when the set of carets has changed. This can occur when the user calls \p set_caret, or when
		/// the current selection is being edited by the user. This function resets the caret animation, invokes
		/// \ref carets_changed, and calls \ref invalidate_visual.
		void _on_carets_changed() {
			_reset_caret_animation();
			carets_changed.invoke();
			invalidate_visual();
		}

		// visual
		/// Sets the correct class of \ref _caret_cfg, resets the animation of carets, and schedules this element for
		/// updating.
		void _reset_caret_animation() {
			// TODO
		}
		/// Updates the value of \ref _misc_region_state, and updates \ref _caret_cfg and \ref _selection_cfg
		/// accordingly.
		void _update_misc_region_state() {
			// TODO
		}

		/// Registers handlers used to update \ref _misc_region_state.
		void _on_logical_parent_constructed() override {
			element::_on_logical_parent_constructed();
			auto *edt = editor::get_encapsulating(*this);

			edt->got_focus += [this]() {
				_update_misc_region_state();
			};
			edt->lost_focus += [this]() {
				_update_misc_region_state();
			};

			edt->horizontal_viewport_changed += [this]() {
				_interaction_manager.on_viewport_changed();
			};
			edt->vertical_viewport_changed += [this]() {
				_interaction_manager.on_viewport_changed();
			};
		}

		// misc
		/// Sets the element to non-focusable, calls \ref _reset_caret_animation(), and initializes
		/// \ref _selection_cfg.
		void _initialize(str_view_t cls, const ui::element_configuration &config) override {
			element::_initialize(cls, config);

			// initialize _interaction_manager
			_interaction_manager.set_contents_region(*this);
			// TODO
			_interaction_manager.activators().emplace_back(new interaction_modes::mouse_prepare_drag_mode_activator<caret_set>());
			_interaction_manager.activators().emplace_back(new interaction_modes::mouse_single_selection_mode_activator<caret_set>());

			_reset_caret_animation();
			// TODO
		}
		/// Sets the current document to empty to unbind event listeners.
		void _dispose() override {
			_unbind_buffer_events();
			element::_dispose();
		}
	};

	/// Helper functions used to obtain the \ref contents_region associated with elements.
	namespace component_helper {
		/// Returns both the \ref editor and the \ref contents_region. If the returned \ref contents_region is not
		/// \p nullptr, then the returned \ref editor also won't be \p nullptr.
		inline std::pair<editor*, contents_region*> get_core_components(const ui::element &elem) {
			editor *edt = editor::get_encapsulating(elem);
			if (edt) {
				return {edt, contents_region::get_from_editor(*edt)};
			}
			return {nullptr, nullptr};
		}

		/// Returns the \ref contents_region that corresponds to the given \ref ui::element.
		inline contents_region *get_contents_region(const ui::element &elem) {
			return get_core_components(elem).second;
		}
	}
}
