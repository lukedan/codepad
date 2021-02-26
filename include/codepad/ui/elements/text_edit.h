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
		cursor get_current_display_cursor() const {
			return cursor::text_beam;
		}

		/// Returns the caret.
		caret_selection get_caret_selection() const {
			return _caret;
		}
		/// Sets the current caret. Calls \ref _set_caret_selection_impl().
		void set_caret_selection(caret_selection sel) {
			_check_cache_line_info();
			sel.caret = std::min(sel.caret, _cached_line_beginnings.back());
			sel.selection = std::min(sel.selection, _cached_line_beginnings.back());
			_set_caret_selection_impl(sel);
		}

	private:
		/// Invoked by \ref move_caret_raw() to update the position of the caret when only the position is given.
		/// This function updates the caret alignment using this new position.
		void _update_caret(std::size_t new_pos) {
			_caret.caret = new_pos;
			_alignment = _formatted_text->get_character_placement(_caret.caret).xmin;
		}
		/// Invoked by \ref move_caret_raw() to update the position and alignment of the caret.
		void _update_caret(std::pair<std::size_t, double> new_pos_align) {
			_caret.caret = new_pos_align.first;
			_alignment = new_pos_align.second;
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
		/// \param continue_selection If \p true, will keep the selection end of the caret and move the caret end as
		///                           if there's no selection.
		template <typename MoveCaret, typename CancelSelection> void move_caret_raw(
			const MoveCaret &move, const CancelSelection &cancel_sel, bool continue_selection
		) {
			if (continue_selection || !_caret.has_selection()) {
				_update_caret(move());
			} else {
				_update_caret(cancel_sel());
			}
			if (!continue_selection) {
				_caret.selection = _caret.caret;
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
		/// will be treated as two characters.
		///
		/// \return Iterator to the very beginning of the newly inserted text, and a boolean indicating whether all
		///         erased codepoints are valid.
		std::pair<std::u8string::iterator, bool> modify(
			std::size_t del_begin, std::size_t del_len, std::u8string_view add
		);

		/// Deletes the character before the caret, or the selection if there is one.
		void delete_character_before_caret();
		/// Deletes the character after the caret, or the selection if there is one.
		void delete_character_after_caret();


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
		bool _selecting = false; ///< Whether the user is dragging with the mouse to select text.

		/// Sets the caret position and recomputes \ref _alignment without checking the position.
		void _set_caret_selection_impl(caret_selection sel) {
			_caret = sel;
			_update_caret(_caret.caret); // call _update_caret() to update alignment
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

		/// Handles keyboard input.
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
		void _update_window_caret_position() const;

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

		/// Returns the mapping that contains notifications for \ref _edit.
		class_arrangements::notify_mapping _get_child_notify_mapping() override {
			auto mapping = scroll_view::_get_child_notify_mapping();
			mapping.emplace(get_text_edit_name(), _name_cast(_edit));
			return mapping;
		}

		/// Registers event handlers for edit and caret movement events of \ref _edit.
		void _initialize(std::u8string_view) override;
	};
}
