// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of \ref codepad::ui::text_edit.

#include "../element.h"
#include "../panel.h"
#include "scrollbar.h"
#include "label.h"

namespace codepad::ui {
	/// An element derived from \ref label that enables the user to edit the text it contains. Note that this element
	/// does not contain scrollbars.
	class text_edit : public label {
	public:
		/// Returns \ref cursor::text_beam if the mouse is not inside the selection, and the default value if it is.
		cursor get_current_display_cursor() const {
			return cursor::text_beam;
		}

		/// Returns the position of the caret.
		std::size_t get_caret() const {
			return _caret;
		}
		/// Returns the position of the non-caret end of the selection.
		std::size_t get_selection() const {
			return _selection_end;
		}

		/// Returns the list of properties.
		const property_mapping &get_properties() const override;

		info_event<> caret_changed; ///< Invoked when the caret or the selection has been changed.

		/// Adds the \p caret_visuals property.
		static const property_mapping &get_properties_static();

		/// Returns the default class used by elements of type \ref text_edit.
		inline static std::u8string_view get_default_class() {
			return u8"text_edit";
		}
	protected:
		visuals _caret_visuals; ///< The \ref visuals of the caret.
		std::size_t
			_caret = 0, ///< Caret position.
			_selection_end = 0; ///< The other end of the selection.
		bool _selecting = false; ///< Whether the user is dragging with the mouse to select text.

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
		/// Called when \ref _caret or \ref _selection_end is changed. Calls \ref invalidate_visuals(), invokes
		/// \ref carets_changed, and updates caret position if this \ref text_edit is focused.
		void _on_caret_changed();

		/// Returns the character position that the given position is over. The position is assumed to be relative to
		/// this element.
		caret_hit_test_result _hit_test_for_caret(vec2d) const;

		/// Updates the caret position used for IMEs if this element is focused.
		void _update_window_caret_position() const;

		/// Renders the caret and the selection.
		void _custom_render() const override;
	};
}
