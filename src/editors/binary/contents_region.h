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
	/// No additional data associated with a caret.
	struct caret_data {
	};

	/// A set of carets.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	public:
		using position = std::size_t; ///< The position of a caret is identified by a single \p std::size_t.
		/// The selection is a pair of positions, i.e., no additional data associated with the caret.
		using selection = caret_selection;
	};

	/// The radix used when displaying and editing binary data.
	enum class radix {
		binary = 2, ///< Binary.
		octal = 8, ///< Octal.
		hexadecimal = 16 ///< Hexadecimal.
	};

	/// The \ref ui::element that displays the contents of the \ref buffer and handles user interactions.
	///
	/// \todo Different radii.
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
		void set_buffer(std::shared_ptr<buffer>);

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
		void add_caret(caret_selection caret) override {
			_carets.add(caret_set::entry(
				caret_selection(caret.caret, caret.selection), caret_data()
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
		caret_selection extract_caret_selection(const caret_set::entry &et) const override {
			return caret_selection(et.first.caret, et.first.selection);
		}

		/// Returns whether the \ref contents_region is currently in insert mode.
		bool is_insert_mode() const {
			return _insert;
		}
		/// Sets whether the \ref contents_region is currently in insert mode.
		void set_insert_mode(bool v) {
			_insert = v;
		}
		/// Toggles insert mode.
		void toggle_insert_mode() {
			set_insert_mode(!is_insert_mode());
		}

		/// Returns the height of a line.
		///
		/// \todo Add customizable line height.
		double get_line_height() const {
			return _font->get_line_height_em() * get_font_size();
		}
		/// Returns the number of lines.
		std::size_t get_num_lines() const {
			if (_buf) {
				std::size_t bytes = get_bytes_per_row();
				return (std::max<std::size_t>(_buf->length(), 1) + bytes - 1) / bytes;
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

		/// Returns the radix used to display binary data.
		radix get_radix() const {
			return _radix;
		}
		// TODO set_radix

		/// Returns the actual current number of bytes per row.
		std::size_t get_bytes_per_row() const {
			return _cached_bytes_per_row;
		}
		/// Sets the desired bytes per row. This value is only used when \ref _wrap is \ref wrap_mode::fixed.
		void set_bytes_per_row(std::size_t val) {
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

		/// Returns the \ref ui::font for rendering characters.
		const std::unique_ptr<ui::font> &get_font() const {
			return _font;
		}
		/// Sets the \ref ui::font for rendering characters.
		void set_font(std::unique_ptr<ui::font> f) {
			_font = std::move(f);
			_on_font_parameters_changed();
		}
		/// Sets the \ref ui::font for rendering characters given the font's name.
		void set_font_by_name(std::u8string_view name) {
			auto family = get_manager().get_renderer().find_font_family(name);
			set_font(
				family->get_matching_font(ui::font_style::normal, ui::font_weight::normal, ui::font_stretch::normal)
			);
		}

		/// Returns the current font size.
		double get_font_size() const {
			return _font_size;
		}
		/// Sets the font size.
		void set_font_size(double size) {
			_font_size = size;
			_on_font_parameters_changed();
		}

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
		void on_text_input(std::u8string_view) override;

		/// Returns the \ref caret_position correponding to the given mouse position. The mouse position is relative
		/// to the top left corner of the document.
		std::size_t hit_test_for_caret_document(vec2d) const;
		/// Similar to \ref hit_test_for_caret_document(), only that the position is relative to the top left corner
		/// of this \ref ui::element.
		std::size_t hit_test_for_caret(vec2d) const override;

		info_event<>
			content_modified, ///< Invoked when the \ref buffer is modified or changed by \ref set_buffer.
			/// Invoked when the set of carets is changed. Note that this does not necessarily mean that the result
			/// of \ref get_carets will change.
			carets_changed;

		/// Converts a character into the corresponding value, i.e., A-Z are treated as 10-35. If the character lies
		/// out of the range, this function returns \p std::numeric_limits<unsigned char>::max().
		static unsigned char get_character_value(codepoint);

		/// Returns the \ref interaction_mode_registry of binary editors.
		static interaction_mode_registry<caret_set> &get_interaction_mode_registry();

		/// Returns the \ref contents_region contained by the given \ref editor.
		inline static contents_region *get_from_editor(editor &edt) {
			return dynamic_cast<contents_region*>(edt.get_contents_region());
		}

		/// Returns the default class used by elements of type \ref contents_region.
		inline static std::u8string_view get_default_class() {
			return u8"binary_contents_region";
		}
		/// Returns the class used by carets under `insert' mode.
		inline static std::u8string_view get_insert_caret_class() {
			return u8"binary_insert_caret";
		}
		/// Returns the class used by carets under `overwrite' mode.
		inline static std::u8string_view get_overwrite_caret_class() {
			return u8"binary_overwrite_caret";
		}
		/// Returns the class used by selected regions.
		inline static std::u8string_view get_contents_region_selection_class() {
			return u8"binary_selection";
		}
	protected:
		/// Returns the hexadecimal representation of the given byte.
		inline static std::u8string_view _get_hex_byte(std::byte b) {
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

			return std::u8string_view(_lut[static_cast<unsigned char>(b)], 2);
		}

		caret_set _carets; ///< The set of carets.
		interaction_manager<caret_set> _interaction_manager; ///< Manages certain mouse and keyboard interactions.
		std::shared_ptr<buffer> _buf; ///< The buffer that's being edited.
		info_event<buffer::end_edit_info>::token _mod_tok; ///< Used to listen to \ref buffer::end_edit.
		radix _radix = radix::hexadecimal; ///< The radix used to display bytes.

		std::unique_ptr<ui::font> _font; ///< The font used to display all bytes.
		double
			_cached_max_byte_width = 0.0, ///< The width of a character.
			_blank_width = 5.0, ///< The distance between two consecutive bytes.
			_font_size = 11.0, ///< Font size.
			_lines_per_scroll = 3.0; ///< The number of lines to scroll per `tick'.
		std::size_t
			/// The number of bytes per row. Only used when \ref _wrap is \ref wrap_mode::fixed.
			_target_bytes_per_row = 16,
			_cached_bytes_per_row = 16; ///< The cached actual number of bytes per row.
		wrap_mode _wrap = wrap_mode::auto_fill; ///< Indicates how the bytes should be wrapped.
		/// Indicates whether this \ref contents_region is in `insert' mode or in `overwrite' mode.
		bool _insert = true;

		// position conversion
		/// Returns the line index at the given position, relative to the top of this document, i.e., without
		/// scrolling. This function returns 0 if the position lies before the first line.
		std::size_t _get_line_at_position(double pos) const {
			return static_cast<std::size_t>(std::max((pos - get_padding().top) / get_line_height(), 0.0));
		}
		/// Returns the vertical offset of the top of the given line, relative to the top of the document.
		double _get_line_offset(std::size_t line) const {
			return get_padding().top + get_line_height() * line;
		}

		/// Returns the index of the first visible byte at the given horizontal position.
		std::size_t _get_column_at_position(double pos) const {
			return std::min(
				get_bytes_per_row(),
				static_cast<std::size_t>((pos + get_blank_width()) / (_cached_max_byte_width + get_blank_width()))
			);
		}
		/// Returns the horizontal offset of the left of the given column, relative to the left of the document.
		double _get_column_offset(std::size_t x) const {
			return get_padding().left + (_cached_max_byte_width + get_blank_width()) * x;
		}

		/// Returns the line and column that the given byte is on.
		std::pair<std::size_t, std::size_t> _get_line_and_column_of_byte(std::size_t byte) const {
			std::size_t bpr = get_bytes_per_row(), line = byte / bpr;
			return { line, byte - line * bpr };
		}

		// rendering
		/// Returns the region that a given caret occupies, relative to the top left of the document.
		rectd _get_caret_rect(std::size_t) const;
		/// Returns the rectangles that a selected region covers. The selection is clamped by the given parameters to
		/// reduce unnecessary regions. The resulting regions are placed relative to the top left of the document.
		std::vector<rectd> _get_selection_rects(
			caret_selection sel, std::size_t clampmin, std::size_t clampmax
		) const;

		/// Renders all visible bytes.
		void _custom_render() const override;

		// formatting
		/// Updates the cached value \ref _cached_bytes_per_row. This function invokes
		/// \ref _on_content_visual_changed() when necessary.
		///
		/// \return Whether the number of bytes per row has changed. This also indicates whether
		///         \ref _on_content_visual_changed() has been called.
		bool _update_bytes_per_row();

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

		/// Updates cached bytes per row, and invalidates the visuals of this element.
		void _on_font_parameters_changed() {
			_cached_max_byte_width = get_font_size() * 2.0 * _font->get_maximum_character_width_em(
				reinterpret_cast<const codepoint*>(U"0123456789ABCDEF")
			);
			if (!_update_bytes_per_row()) {
				_on_content_visual_changed();
			}
		}
		/// Called when \ref _buf has been modified, this function re-adjusts caret positions and invokes
		/// \ref _on_content_modified().
		void _on_end_edit(buffer::end_edit_info&);
		/// Called when the buffer has been modified.
		void _on_content_modified() {
			content_modified.invoke();
			_on_content_visual_changed();
		}
		/// Called when \ref content_visual_changed should be invoked.
		void _on_content_visual_changed() {
			content_visual_changed.invoke();
			invalidate_visual();
		}
		/// Called when the set of carets has changed. This can occur when the user calls \p set_caret, or when
		/// the current selection is being edited by the user. This function resets the caret animation, invokes
		/// \ref carets_changed, and calls \ref invalidate_visual.
		void _on_carets_changed() {
			carets_changed.invoke();
			invalidate_visual();
		}

		// visual
		/// Registers handlers used to forward viewport update events to \ref _interaction_manager.
		void _on_logical_parent_constructed() override;

		// misc
		/// Loads font and interaction settings.
		void _initialize(std::u8string_view, const ui::element_configuration&) override;
		/// Sets the current document to empty to unbind event listeners.
		void _dispose() override;
	};

	/// Helper functions used to obtain the \ref contents_region associated with elements.
	namespace component_helper {
		/// Returns both the \ref editor and the \ref contents_region. If the returned \ref contents_region is not
		/// \p nullptr, then the returned \ref editor also won't be \p nullptr.
		std::pair<editor*, contents_region*> get_core_components(const ui::element&);

		/// Returns the \ref contents_region that corresponds to the given \ref ui::element.
		inline contents_region *get_contents_region(const ui::element &elem) {
			return get_core_components(elem).second;
		}
	}
}
