// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Buttons.

#include "../panel.h"

namespace codepad::ui {
	/// Base class of button-like elements, that provides all interfaces only internally.
	class button : public panel {
	public:
		/// Indicates when the click event is triggered.
		enum class trigger_type {
			mouse_down, ///< The event is triggered as soon as the button is pressed.
			mouse_up ///< The event is triggered after the user presses then releases the button.
		};

		/// Returns \p true if the button is currently pressed.
		bool is_trigger_button_pressed() const {
			return _trigbtn_down;
		}

		/// Sets the mouse button used to press the button, the `trigger button'.
		void set_trigger_button(mouse_button btn) {
			_trigbtn = btn;
		}
		/// Returns the current trigger button.
		mouse_button get_trigger_button() const {
			return _trigbtn;
		}

		/// Sets when the button click is triggered.
		void set_trigger_type(trigger_type t) {
			_trigtype = t;
		}
		/// Returns the current trigger type.
		trigger_type get_trigger_type() const {
			return _trigtype;
		}

		/// Sets whether the user is allowed to cancel the click halfway.
		void set_allow_cancel(bool v) {
			_allow_cancel = v;
		}
		/// Returns \p true if the user is allowed to cancel the click.
		bool get_allow_cancel() const {
			return _allow_cancel;
		}

		info_event<> click; ///< Triggered when the button is clicked.

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"button";
		}
	protected:
		bool
			_trigbtn_down = false, ///< Indicates whether the trigger button is currently down.
			/// If \p true, the user will be able to cancel the click after pressing the trigger button,
			/// by moving the cursor out of the button and releasing the trigger button,
			/// if  \ref _trigtype is set to trigger_type::mouse_up.
			_allow_cancel = true;
		trigger_type _trigtype = trigger_type::mouse_up; ///< The trigger type of this button.
		/// The mouse button that triggers the button.
		mouse_button _trigbtn = mouse_button::primary;

		/// Checks for clicks.
		void _on_mouse_down(mouse_button_info&) override;
		/// Calls _on_trigger_button_up().
		void _on_capture_lost() override {
			_trigbtn_down = false;
			panel::_on_capture_lost();
		}
		/// Checks if this is a valid click.
		void _on_mouse_up(mouse_button_info&) override;
		/// Called when the mouse position need to be updated. If \ref _allow_cancel is \p true, checks if the mouse
		/// is still over the element, and invokes \ref _on_mouse_enter() or \ref _on_mouse_leave() accordingly.
		void _on_update_mouse_pos(vec2d);
		/// Updates the mouse position.
		void _on_mouse_move(mouse_move_info &p) override {
			_on_update_mouse_pos(p.new_position.get(*this));
			element::_on_mouse_move(p);
		}

		/// Callback that is called when the user clicks the button. Invokes \ref click by default.
		virtual void _on_click() {
			click.invoke();
		}
	};
}
