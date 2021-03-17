// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of \ref codepad::editors::binary::contents_region.

#include <algorithm>
#include <string_view>

#include <codepad/ui/element.h>

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
		using selection = ui::caret_selection;

		/// Wrapper around \ref caret_selection_position::set_caret_position().
		inline static void set_caret_position(selection &s, position pos) {
			s.caret = pos;
		}
		/// Wrapper around \ref caret_selection_position::get_caret_position().
		inline static position get_caret_position(selection s) {
			return s.caret;
		}
	};

	/// The radix used when displaying and editing binary data.
	enum class radix : unsigned char {
		binary = 2, ///< Binary.
		octal = 8, ///< Octal.
		decimal = 10, ///< Decimal.
		hexadecimal = 16 ///< Hexadecimal.
	};
	/// Returns the maximum number of digits required to represent a byte for the given \ref radix.
	[[nodiscard]] std::size_t get_maximum_byte_digits_for_radix(radix);
	/// Base conversion.
	[[nodiscard]] std::basic_string<codepoint> convert_base(unsigned i, radix base, std::size_t min_length = 1);

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
		buffer &get_buffer() const override {
			return *_buf;
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
		void add_caret(ui::caret_selection caret) override {
			_carets.add(caret_set::entry(
				ui::caret_selection(caret.caret, caret.selection), caret_data()
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

		/// Returns the height of a line.
		///
		/// \todo Add customizable line height.
		[[nodiscard]] double get_line_height() const {
			return _font->get_line_height_em() * get_font_size();
		}
		/// Returns the distance from the top of the line to the baseline.
		[[nodiscard]] double get_baseline() const {
			return _font->get_ascent_em() * get_font_size();
		}
		/// Returns the number of lines.
		[[nodiscard]] std::size_t get_num_lines() const {
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
		/// Returns the vertical viewport range.
		double get_vertical_scroll_range() const override {
			return
				get_line_height() * static_cast<double>(get_num_lines() - 1) +
				get_layout().height() + get_padding().top;
		}
		/// Returns the horizontal viewport range.
		double get_horizontal_scroll_range() const override {
			return
				static_cast<double>(get_bytes_per_row()) * (_cached_max_byte_width + get_blank_width()) +
				get_padding().width() - get_blank_width();
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
		[[nodiscard]] radix get_radix() const {
			return _radix;
		}
		/// Sets the radix of this editor.
		void set_radix(radix r) {
			_radix = r;
			_on_byte_width_changed();
		}

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
		[[nodiscard]] const std::shared_ptr<ui::font> &get_font() const {
			return _font;
		}
		/// Returns the name of the font family that's currently being used.
		[[nodiscard]] const std::u8string &get_font_family_name() const {
			return _font_family_name;
		}
		/// Returns the \ref ui::font_family used for rendering text.
		[[nodiscard]] const std::shared_ptr<ui::font_family> &get_font_family() const {
			return _font_family;
		}
		/// Sets the \ref ui::font_family for rendering characters given the font's name.
		void set_font_family(std::u8string name) {
			_font_family_name = std::move(name);
			_font_family = get_manager().get_renderer().find_font_family(_font_family_name);
			_update_font();
			_on_byte_width_changed();
		}

		/// Returns the current font size.
		double get_font_size() const {
			return _font_size;
		}
		/// Sets the font size.
		void set_font_size(double size) {
			_font_size = size;
			_on_byte_width_changed();
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
		caret_set _carets; ///< The set of carets.
		interaction_manager<caret_set> _interaction_manager; ///< Manages certain mouse and keyboard interactions.
		std::shared_ptr<buffer> _buf; ///< The buffer that's being edited.
		info_event<buffer::end_edit_info>::token _mod_tok; ///< Used to listen to \ref buffer::end_edit.
		radix _radix = radix::hexadecimal; ///< The radix used to display bytes.

		std::u8string _font_family_name; ///< The name of the font family.
		std::shared_ptr<ui::font_family> _font_family; ///< The font family used to display all bytes.
		std::shared_ptr<ui::font> _font; ///< The font used to display all bytes.
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

		// position conversion
		/// Returns the line index at the given position, relative to the top of this document, i.e., without
		/// scrolling. This function returns 0 if the position lies before the first line.
		std::size_t _get_line_at_position(double pos) const {
			return static_cast<std::size_t>(std::max((pos - get_padding().top) / get_line_height(), 0.0));
		}
		/// Returns the vertical offset of the top of the given line, relative to the top of the document.
		double _get_line_offset(std::size_t line) const {
			return get_padding().top + get_line_height() * static_cast<double>(line);
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
			return get_padding().left + (_cached_max_byte_width + get_blank_width()) * static_cast<double>(x);
		}

		/// Returns the line and column that the given byte is on.
		std::pair<std::size_t, std::size_t> _get_line_and_column_of_byte(std::size_t byte) const {
			std::size_t bpr = get_bytes_per_row(), line = byte / bpr;
			return { line, byte - line * bpr };
		}

		// rendering
		/// Returns the region that a given caret occupies, relative to the top left of the document.
		rectd _get_caret_rect(std::size_t) const;
		/// Returns the layout of a selected region. The selection is clamped by the given parameters to reduce
		/// unnecessary regions. The resulting regions are placed relative to the top left of the document.
		[[nodiscard]] decoration_layout _get_selection_layout(
			ui::caret_selection sel, std::size_t clampmin, std::size_t clampmax, vec2d offset
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
		/// Updates \ref _font using the font family and parameters. The caller needs to manually call
		/// \ref _on_byte_width_changed() afterwards.
		void _update_font() {
			_font = _font_family->get_matching_font(
				get_text_theme().style, get_text_theme().weight, ui::font_stretch::normal
			);
		}
		/// Additionally invokes \ref _update_font() and \ref _on_byte_width_changed().
		void _on_text_theme_changed() override {
			_update_font();
			_on_byte_width_changed();
			interactive_contents_region_base::_on_text_theme_changed();
		}
		/// Registers handlers used to forward viewport update events to \ref _interaction_manager.
		void _on_logical_parent_constructed() override;
		/// Invoked whenever the width of a byte may have changed. This includes: when the font parameters have been
		/// changed, when the radix has been changed, etc.
		void _on_byte_width_changed() {
			_cached_max_byte_width =
				get_font_size() * get_maximum_byte_digits_for_radix(_radix) * _font->get_maximum_character_width_em(
					reinterpret_cast<const codepoint*>(U"0123456789ABCDEF")
				);
			if (!_update_bytes_per_row()) {
				_on_content_visual_changed();
			}
		}

		// misc
		bool _register_event(std::u8string_view, std::function<void()>) override;

		/// Loads font and interaction settings.
		void _initialize(std::u8string_view) override;
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
