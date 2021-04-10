// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of \ref codepad::ui::text_edit.

#include "../element.h"
#include "../panel.h"
#include "label.h"
#include "scroll_viewport.h"

namespace codepad::ui {
	/// An element derived from \ref label that enables the user to edit the text it contains.
	class text_edit : public label {
	public:
		/// Returns \ref cursor::text_beam if the mouse is not inside the selection, and the default value if it is.
		cursor get_current_display_cursor() const override {
			return cursor::text_beam;
		}

		/// Returns the caret.
		[[nodiscard]] caret_selection get_caret_selection() const {
			return _caret;
		}
		/// Sets the current caret. Calls \ref _set_caret_selection_impl().
		void set_caret_selection(caret_selection sel) {
			_check_cache_line_info();
			sel.clamp(_cached_line_beginnings.back());
			_set_caret_selection_impl(sel);
		}

	private:
		/// Updates \ref _alignment, and returns the caret position.
		std::size_t _update_alignment(std::size_t new_pos) {
			_alignment = _formatted_text->get_character_placement(new_pos).xmin;
			return new_pos;
		}
		/// Updates \ref _alignment using the supplied value, then returns the caret position.
		std::size_t _update_alignment(std::pair<std::size_t, double> pos_align) {
			_alignment = pos_align.second;
			return pos_align.first;
		}
	public:
		/// Moves the caret. The two function object parameters \p move and \p cancel_sel either return a single
		/// \p std::size_t, which is the new position, or a \p std::pair<std::size_t, double>, which is the new
		/// position and the new alignment (the virtual horizontal position of the caret that will be used when
		/// moving the caret vertically).
		///
		/// \param move A function that returns the position of the caret after it has been moved without cancelling
		///             the selection.
		/// \param cancel_sel A function that returns the new position of the caret after cancelling the selection.
		/// \param continue_selection Determines whether the user is trying to edit the selection or to cancel the
		///                           selection.
		template <typename MoveCaret, typename CancelSelection> void move_caret_raw(
			const MoveCaret &move, const CancelSelection &cancel_sel, bool continue_selection
		) {
			std::size_t new_pos;
			if (continue_selection || !_caret.has_selection()) {
				new_pos = _update_alignment(move());
			} else {
				new_pos = _update_alignment(cancel_sel());
			}
			if (!continue_selection) {
				_caret = caret_selection(new_pos);
			} else {
				_caret.move_caret(new_pos);
			}
			_on_caret_changed();
		}
		/// \ref move_caret_raw() where \p move and \p cancel_sel are the same.
		template <typename MoveCaret> void move_caret_raw(const MoveCaret &mc, bool continue_selection) {
			move_caret_raw(mc, mc, continue_selection);
		}

		/// Moves the caret one character to the left. If there's a selection and \p continue_selection is \p false,
		/// the selection is cancelled and the cursor is moved to the very end of the selected region.
		void move_caret_left(bool continue_selection);
		/// Moves the caret one character to the right. If there's a selection and \p continue_selection is \p false,
		/// the selection is cancelled and the cursor is moved to the very end of the selected region.
		void move_caret_right(bool continue_selection);
		/// Moves the caret one character to the beginning of the line. If there's a selection and
		/// \p continue_selection is \p false, the selection is cancelled and the cursor is moved to the very end of
		/// the selected region.
		void move_caret_to_line_beginning(bool continue_selection);
		/// Moves the caret one character to the end of the line. If there's a selection and \p continue_selection is
		/// \p false, the selection is cancelled and the cursor is moved to the very end of the selected region.
		void move_caret_to_line_ending(bool continue_selection);
		/// Moves the caret one line above. If there's a selection and \p continue_selection is \p false, the
		/// selection is cancelled and the cursor is moved to the very end of the selected region.
		void move_caret_up(bool continue_selection);
		/// Moves the caret one line below. If there's a selection and \p continue_selection is \p false, the
		/// selection is cancelled and the cursor is moved to the very end of the selected region.
		void move_caret_down(bool continue_selection);


		/// Modifies the text by removing the characters in the specified range and adding the given string in its
		/// place. Note that the character range does not take into account CRLF new line characters, i.e., a CRLF
		/// will be treated as two characters. This function does **not** check if \ref _readonly is \p true.
		///
		/// \return Iterator to the very beginning of the newly inserted text, and a boolean indicating whether all
		///         erased codepoints are valid.
		std::pair<std::u8string::iterator, bool> modify(
			std::size_t del_begin, std::size_t del_len, std::u8string_view add
		);

		/// Deletes the character before the caret, or the selection if there is one. This function does **not**
		/// check if \ref _readonly is \p true.
		void delete_character_before_caret();
		/// Deletes the character after the caret, or the selection if there is one. This function does **not**
		/// check if \ref _readonly is \p true.
		void delete_character_after_caret();


		/// Returns \ref _readonly.
		[[nodiscard]] bool is_readonly() const {
			return _readonly;
		}
		/// Sets \ref _readonly and calls \ref _on_readonly_changed() if necessary.
		void set_readonly(bool value) {
			if (value != _readonly) {
				_readonly = value;
				_on_readonly_changed();
			}
		}


		info_event<>
			caret_changed, ///< Invoked when the caret or the selection has been changed.
			text_changed; ///< Invoked when the text has been changed.

		/// Returns the default class used by elements of type \ref text_edit.
		inline static std::u8string_view get_default_class() {
			return u8"text_edit";
		}
	protected:
		visuals
			_caret_visuals, ///< The \ref visuals of the caret.
			_selection_visuals; ///< Visuals for individual rectangles in the selection.
		std::vector<line_metrics> _cached_line_metrics; ///< Cached metrics of each line.
		/// Cached indices of the first characters of all lines. This contains one more element at the end which is
		/// the total number of characters.
		std::vector<std::size_t> _cached_line_beginnings;
		caret_selection _caret; ///< The caret and the associated selection.
		double _alignment = 0.0; ///< The alignment of the caret.
		bool
			_selecting = false, ///< Whether the user is dragging with the mouse to select text.
			/// Indicates whether this label is read-only. This only affects direct keyboard input - i.e., functions
			/// like \ref set_text(), \ref modify(), and \ref delete_character_before_caret() do not check this flag.
			/// Command implementations should check this flag manually via \ref is_readonly().
			_readonly = false;


		/// Sets the caret position and recomputes \ref _alignment without checking the position.
		void _set_caret_selection_impl(caret_selection sel) {
			_caret = sel;
			_update_alignment(_caret.get_caret_position());
			_on_caret_changed();
		}

		/// Updates the selection if the user is selecting text.
		void _on_mouse_move(mouse_move_info&) override;
		/// Starts selecting using the mouse.
		void _on_mouse_down(mouse_button_info&) override;
		/// Stops selecting.
		void _on_mouse_up(mouse_button_info&) override;
		/// Stops selecting.
		void _on_capture_lost() override;

		/// Handles keyboard input. Does nothing if \ref _readonly is \p true.
		void _on_keyboard_text(text_info&) override;

		/// Invokes \ref _update_window_caret_position().
		void _on_text_layout_changed() override;
		/// Invokes \ref _update_window_caret_position().
		void _on_layout_changed() override;
		/// Additionally resets \ref _cached_line_metrics and \ref _cached_line_beginnings.
		void _on_text_changed() override;
		/// Called when \ref _caret or \ref _selection_end is changed. Calls \ref invalidate_visuals(), invokes
		/// \ref carets_changed, and updates caret position if this \ref text_edit is focused.
		void _on_caret_changed();
		/// Called when \ref _readonly has been changed. Does nothing by default.
		virtual void _on_readonly_changed() {
		}

		/// TODO FOR TESTING.
		void _on_key_down(key_info &p) override {
			label::_on_key_down(p);
			if (p.key_pressed == key::control) {
				logger::get().log_debug(CP_HERE) << "set red";
				auto [smin, smax] = _caret.get_range();
				_formatted_text->set_text_color(colord(1.0, 0.0, 0.0, 1.0), smin, smax - smin);
				invalidate_visual();
			}
		}

		/// Returns the character position that the given position is over. The position is assumed to be relative to
		/// this element.
		caret_hit_test_result _hit_test_for_caret(vec2d) const;

		/// Updates the caret position used for IMEs if this element is focused.
		void _update_window_caret_position();

		/// Computes \ref _cached_line_metrics if it hasn't been cached.
		void _check_cache_line_info();

		/// Returns the line that the given character is on.
		std::size_t _get_line_of_character(std::size_t);

		/// Returns the previous caret position. This is mainly used to handle lines ending with CRLF. If the input
		/// position is 0, this function returns 0.
		std::size_t _get_previous_caret_position(std::size_t);
		/// Returns the next caret position. This is mainly used to handle lines ending with CRLF. If the input
		/// position is at the end of the text, this function returns it unchanged.
		std::size_t _get_next_caret_position(std::size_t);

		/// Handles the \p readonly, \p caret_visuals, and \p selection_visuals properties.
		property_info _find_property_path(const property_path::component_list&) const override;

		/// Renders the caret and the selection.
		void _custom_render() const override;
	};

	/// A textbox that combines a \ref scroll_view and a \ref text_edit.
	class textbox : public scroll_view {
	public:
		/// Returns \ref _edit.
		text_edit *get_text_edit() const {
			return _edit;
		}

		/// Returns the name of \ref _edit.
		inline static std::u8string_view get_text_edit_name() {
			return u8"text_edit";
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"textbox";
		}
	protected:
		text_edit *_edit = nullptr; ///< The associated \ref text_edit.

		/// Handles \ref _edit and registers for events.
		bool _handle_reference(std::u8string_view, element*) override;

		/// Sets \ref _is_focus_scope to \p false.
		void _initialize() override;
	};
}
