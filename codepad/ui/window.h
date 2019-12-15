// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes related to `window's.

#include <chrono>
#include <functional>

#include "../core/encodings.h"
#include "../core/event.h"
#include "renderer.h"
#include "panel.h"
#include "commands.h"
#include "element_classes.h"

namespace codepad::ui {
	class scheduler;

	/// Base class of all windows. Defines basic abstract interfaces that all windows should implement. Note that
	/// \ref show_and_activate() needs to be called manually after its construction for this window to be displayed.
	class window_base : public panel {
		friend scheduler;
		friend element_collection;
		friend renderer_base;
	public:
		/// Contains information about the resizing of a window.
		using size_changed_info = value_update_info<vec2d, value_update_info_contents::new_value>;
		/// Contains the new DPI scaling factor of a window.
		using scaling_factor_changed_info = value_update_info<vec2d, value_update_info_contents::new_value>;

		/// Sets the caption of the window.
		virtual void set_caption(const str_t&) = 0;
		/// Retrieves the physical position of the top-left corner of the window's client region in screen
		/// coordinates.
		virtual vec2d get_position() const = 0;
		/// Moves the window so that the top-left corner of the window's client region is at the given physical
		/// position. The position may not be exact due to rounding errors.
		virtual void set_position(vec2d) = 0;
		/// Returns the logical size of the window's client region.
		virtual vec2d get_client_size() const = 0;
		/// Sets the logical size the window's client region. The size may not be exact due to rounding errors.
		virtual void set_client_size(vec2d) = 0;
		/// Returns the DPI scaling factor of this window.
		virtual vec2d get_scaling_factor() const = 0;

		/// Brings the window to foreground and activates it.
		virtual void activate() = 0;
		/// Indicate to the user that the window needs attention, but without changing its state.
		virtual void prompt_ready() = 0;

		/// Shows the window without activating it.
		virtual void show() = 0;
		/// Shows and activates the window.
		virtual void show_and_activate() {
			show();
			activate();
		}
		/// Hides the window without closing it.
		virtual void hide() = 0;

		/// Sets whether the `maximize' button is displayed.
		virtual void set_display_maximize_button(bool) = 0;
		/// Sets whether the `minimize' button is displayed.
		virtual void set_display_minimize_button(bool) = 0;
		/// Sets whether the title bar is displayed.
		virtual void set_display_caption_bar(bool) = 0;
		/// Sets whether the window border is displayed.
		virtual void set_display_border(bool) = 0;
		/// Sets whether the user can resize the window.
		virtual void set_sizable(bool) = 0;
		/// Sets whether this window is displayed about all other normal windows.
		virtual void set_topmost(bool) = 0;
		/// Sets if this window is shown in the taskbar or the task list. This may have other side effects.
		virtual void set_show_icon(bool) = 0;

		/// Tests if the given physical position in screen coordinates is in the window, including its title bar and
		/// borders.
		virtual bool hit_test_full_client(vec2d) const = 0;

		/// Obtains the corresponding logical position in client coordinates from the given physical position in
		/// screen coordinates.
		virtual vec2d screen_to_client(vec2d) const = 0;
		/// Obtains the corresponding physical position in screen coordinates from the given logical position in
		/// client coordinates.
		virtual vec2d client_to_screen(vec2d) const = 0;

		/// Called to set the position of the currently active caret. This position
		/// is used by input methods to display components such as the candidate window.
		virtual void set_active_caret_position(rectd) = 0;
		/// Called when the input is interrupted by, e.g., a change of focus or caret position.
		virtual void interrupt_input_method() = 0;

		/// Captures the mouse to a given element, so that wherever the mouse moves to, the element
		/// always receives notifications as if the mouse were over it, until \ref release_mouse_capture is
		/// called. Derived classes should override this function to notify the desktop environment.
		virtual void set_mouse_capture(element &elem) {
			logger::get().log_debug(CP_HERE) <<
				"set mouse capture 0x" << &elem << " <" << demangle(typeid(elem).name()) << ">";
			assert_true_usage(_capture == nullptr, "mouse already captured");
			_capture = &elem;
			// send appropriate mouse leave / enter messages
			// TODO this may cause stack overflow
			/*if (!_capture->is_mouse_over()) {
				std::vector<element*> enters; // the list of elements where mouse_enter should be sent to
				for (panel *e = elem.parent(); e; e = e->parent()) { // send mouse_leave messages
					if (e->is_mouse_over()) {
						for (element *cur : e->children().z_ordered()) {
							if (cur->is_mouse_over()) {
								// if the mouse is over an element then
								// it must not contain _capture
								cur->_on_mouse_leave();
								// there should only be one element that the mouse is over,
								// but we'll iterate over all of them just because
							}
						}
						break;
						// TODO if the mouse is not in this window then mouse_leave messages will *not* be sent
						//      not sure if anything should be done, or if the system set_capture function will
						//      handle it
					}
					enters.emplace_back(e);
				}
				// send mouse_enter messages
				for (auto it = enters.rbegin(); it != enters.rend(); ++it) {
					(*it)->_on_mouse_enter();
				}
				_capture->_on_mouse_enter();
			}*/
		}
		/// Returns the element that currently captures the mouse, or \p nullptr.
		virtual element *get_mouse_capture() const {
			return _capture;
		}
		/// Releases the mouse capture. Derived classes should override this function to notify the system.
		virtual void release_mouse_capture() {
			logger::get().log_debug(CP_HERE) << "release mouse capture";
			assert_true_usage(_capture != nullptr, "mouse not captured");
			_capture = nullptr;
			// TODO send a mouse_move message to correct mouse over information?
		}
		/// If the mouse is captured, returns the mouse cursor of \ref _capture; otherwise falls back to
		/// the default behavior.
		cursor get_current_display_cursor() const override {
			if (_capture) {
				return _capture->get_current_display_cursor();
			}
			return panel::get_current_display_cursor();
		}

		info_event<>
			close_request, ///< Invoked when the user clicks the `close' button.
			got_window_focus, ///< Invoked when the window gets keyboard focus.
			lost_window_focus; ///< Invoked when the window loses keyboard focus.
		info_event<size_changed_info> size_changed; ///< Invoked when the window's size has changed.
		/// Invoked when the window's scaling factor has been changed.
		info_event<scaling_factor_changed_info> scaling_factor_changed;
	protected:
		std::any _renderer_data; ///< Renderer-specific data associated with this window.
		element *_capture = nullptr; ///< The element that captures the mouse.

		/// Updates \ref _cached_mouse_position and \ref _cached_mouse_position_timestamp, and returns a
		/// corresponding \ref mouse_position object. Note that the input position is a logical position, and it may
		/// be necessary to use \ref _physical_to_logical_position() to convert physical positions into logical ones.
		mouse_position _update_mouse_position(vec2d pos) {
			mouse_position::_active_window = this;
			_cached_mouse_position = get_parameters().visual_parameters.transform.inverse_transform_point(
				pos - get_layout().xmin_ymin(), get_layout().size()
			);
			++_cached_mouse_position_timestamp;
			return mouse_position(_cached_mouse_position_timestamp);
		}

		/// Calls \ref renderer_base::begin_drawing() and \ref renderer_base::clear() to start rendering to this
		/// window.
		void _on_prerender() override;
		/// Calls \ref renderer_base::end_drawing() to stop drawing.
		void _on_postrender() override;

		/// Called when the user clicks the `close' button.
		virtual void _on_close_request() {
			close_request();
		}
		/// Called when the window's size has changed.
		virtual void _on_size_changed(size_changed_info&);
		/// Called when the window's scaling factor has been changed.
		virtual void _on_scaling_factor_changed(scaling_factor_changed_info&);

		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_down(key_info&) override;
		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_up(key_info&) override;
		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_keyboard_text(text_info&) override;
		/// Forwards the composition event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_composition(composition_info&) override;
		/// Forwards the composition event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_composition_finished() override;


		/// Called when mouse capture is broken by the user's actions.
		virtual void _on_lost_window_capture() {
			if (_capture != nullptr) {
				_capture->_on_capture_lost();
				_capture = nullptr;
			}
		}


		/// Registers the window to \ref renderer_base.
		void _initialize(str_view_t, const element_configuration&) override;
		/// Deletes all decorations, releases the focus, and unregisters the window from \ref renderer_base.
		void _dispose() override;

		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_enter() override {
			if (_capture != nullptr) { // TODO technically this won't happen as the window has already captured the mouse
				_capture->_on_mouse_enter();
				element::_on_mouse_enter();
			} else {
				panel::_on_mouse_enter();
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_leave() override {
			if (_capture != nullptr) { // TODO technically this won't happen
				_capture->_on_mouse_leave();
				element::_on_mouse_leave();
			} else {
				panel::_on_mouse_leave();
			}
		}
		/// If the mouse is captured by an element, forwards the event to it. Otherwise falls back to the default
		/// behavior.
		void _on_mouse_move(mouse_move_info &p) override {
			if (_capture != nullptr) {
				_capture->_on_mouse_move(p);
				element::_on_mouse_move(p);
			} else {
				panel::_on_mouse_move(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_down(mouse_button_info &p) override {
			if (_capture != nullptr) {
				_capture->_on_mouse_down(p);
				mouse_down(p);
			} else {
				panel::_on_mouse_down(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_up(mouse_button_info &p) override {
			if (_capture != nullptr) {
				_capture->_on_mouse_up(p);
				element::_on_mouse_up(p);
			} else {
				panel::_on_mouse_up(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_scroll(mouse_scroll_info &p) override {
			if (_capture != nullptr) {
				for (element *e = _capture; !p.handled() && e != this; e = e->parent()) {
					assert_true_logical(e, "corrupted element tree");
					e->_on_mouse_scroll(p);
				}
				element::_on_mouse_scroll(p);
			} else {
				panel::_on_mouse_scroll(p);
			}
		}
	};
}
