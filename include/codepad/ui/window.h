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

namespace codepad::os {
	class window_impl;
}
namespace codepad::ui {
	class scheduler;
	class window;

	namespace _details {
		/// Pimpl for windows.
		class window_impl {
		public:
			/// Initializes \ref _window.
			explicit window_impl(window &w) : _window(w) {
			}
			/// Default virtual destructor.
			virtual ~window_impl() = default;

			/// Sets the parent of this window.
			virtual void set_parent(window*) = 0;

			/// Sets the caption of this window.
			virtual void set_caption(const std::u8string&) = 0;

			/// Returns the position of the top-left corner of the client area of this window in screen coordinates.
			[[nodiscard]] virtual vec2d get_position() const = 0;
			/// Adjusts the position of the top-left corner of the client area of this window in screen coordinates
			/// to the given position by moving this window.
			virtual void set_position(vec2d) = 0;

			/// Returns the size of the client area, *in logical pixels* (meaning that it has been adjusted to
			/// counter the effects of device scaling, returned by \ref get_scaling_factor() ).
			[[nodiscard]] virtual vec2d get_client_size() const = 0;
			/// Sets the size of the client area, *in logical pixels*.
			virtual void set_client_size(vec2d) = 0;

			/// Returns the scaling factor currently applied to this window.
			[[nodiscard]] virtual vec2d get_scaling_factor() const = 0;

			/// Activates this window, bringing it to the top level and focuses on the window.
			virtual void activate() = 0;
			/// Prompts the user that this window is ready, without activating the window.
			virtual void prompt_ready() = 0;

			/// Shows the window without activating it.
			virtual void show() = 0;
			/// Shows and activates the window. By default this function calls \ref show() and \ref activate();
			/// implementaitons can override this to provide a simpler way to achieve this.
			virtual void show_and_activate() {
				show();
				activate();
			}
			/// Hides the window without closing it.
			virtual void hide() = 0;

			/// Invoked when this winodw needs redrawing.
			virtual void invalidate_window_visuals() = 0;

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

			/// Obtains the corresponding logical position in client coordinates from the given physical position in
			/// screen coordinates. The result may not be exact due to rounding, and round-trip is not guaranteed.
			[[nodiscard]] virtual vec2d screen_to_client(vec2d) const = 0;
			/// Obtains the corresponding physical position in screen coordinates from the given logical position in
			/// client coordinates. The result may not be exact due to rounding, and round-trip is not guaranteed.
			[[nodiscard]] virtual vec2d client_to_screen(vec2d) const = 0;

			/// Called to set the position of the currently active caret. This position
			/// is used by input methods to display components such as the candidate window.
			virtual void set_active_caret_position(rectd) = 0;
			/// Called when the input is interrupted by, e.g., a change of focus or caret position.
			virtual void interrupt_input_method() = 0;

			/// Notifies the system that this window now captures the mouse.
			virtual void set_mouse_capture() = 0;
			/// Notifies the system that this window has released the capture on the mouse.
			virtual void release_mouse_capture() = 0;
		protected:
			window &_window; ///< The associated \ref window.
		};
	}

	/// Base class of all windows. Defines basic abstract interfaces that all windows should implement. Note that
	/// \ref show() or \ref show_and_activate() needs to be called manually after its construction for this window to
	/// be displayed.
	class window : public panel {
		friend scheduler;
		friend element_collection;
		friend renderer_base;
		friend os::window_impl;
	public:
		/// Contains information about the resizing of a window.
		using size_changed_info = value_update_info<vec2d, value_update_info_contents::new_value>;
		/// Contains the new DPI scaling factor of a window.
		using scaling_factor_changed_info = value_update_info<vec2d, value_update_info_contents::new_value>;

		/// Sets the parent of this window.
		void set_parent(window *other) {
			_impl->set_parent(other);
		}

		/// Sets the caption of the window.
		void set_caption(const std::u8string &s) {
			_impl->set_caption(s);
		}

		/// Retrieves the physical position of the top-left corner of the window's client region in screen
		/// coordinates.
		[[nodiscard]] vec2d get_position() const {
			return _impl->get_position();
		}
		/// Moves the window so that the top-left corner of the window's client region is at the given physical
		/// position. The position may not be exact due to rounding.
		void set_position(vec2d pos) {
			_impl->set_position(pos);
		}
		/// Returns the logical size of the window's client region.
		[[nodiscard]] vec2d get_client_size() const {
			return _impl->get_client_size();
		}
		/// Sets the logical size the window's client region. The size may not be exact due to rounding.
		void set_client_size(vec2d size) {
			_impl->set_client_size(size);
		}

		/// Returns the DPI scaling factor of this window.
		[[nodiscard]] vec2d get_scaling_factor() const {
			return _impl->get_scaling_factor();
		}

		/// Activates this window, bringing it to the top level and focuses on the window.
		void activate() {
			_impl->activate();
		}
		/// Prompts the user that this window is ready, without activating the window.
		void prompt_ready() {
			_impl->prompt_ready();
		}

		/// Shows the window without activating it.
		void show() {
			_impl->show();
		}
		/// Shows and activates the window.
		void show_and_activate() {
			_impl->show_and_activate();
		}
		/// Hides the window without closing it.
		void hide() {
			_impl->hide();
		}

		/// Invoked when any element in this winodw needs redrawing.
		void invalidate_window_visuals() {
			_impl->invalidate_window_visuals();
		}

		/// Sets whether the `maximize' button is displayed.
		void set_display_maximize_button(bool b) {
			_impl->set_display_maximize_button(b);
		}
		/// Sets whether the `minimize' button is displayed.
		void set_display_minimize_button(bool b) {
			_impl->set_display_minimize_button(b);
		}
		/// Sets whether the title bar is displayed.
		void set_display_caption_bar(bool b) {
			_impl->set_display_caption_bar(b);
		}
		/// Sets whether the window border is displayed.
		void set_display_border(bool b) {
			_impl->set_display_border(b);
		}
		/// Sets whether the user can resize the window.
		void set_sizable(bool b) {
			_impl->set_sizable(b);
		}
		/// Sets whether this window is displayed about all other normal windows.
		void set_topmost(bool b) {
			_impl->set_topmost(b);
		}
		/// Sets if this window is shown in the taskbar or the task list. This may have other side effects.
		void set_show_icon(bool b) {
			_impl->set_show_icon(b);
		}

		/// Obtains the corresponding logical position in client coordinates from the given physical position in
		/// screen coordinates.
		[[nodiscard]] vec2d screen_to_client(vec2d p) const {
			return _impl->screen_to_client(p);
		}
		/// Obtains the corresponding physical position in screen coordinates from the given logical position in
		/// client coordinates.
		[[nodiscard]] vec2d client_to_screen(vec2d p) const {
			return _impl->client_to_screen(p);
		}

		/// Called to set the position of the currently active caret. This position
		/// is used by input methods to display components such as the candidate window.
		void set_active_caret_position(rectd r) {
			_impl->set_active_caret_position(r);
		}
		/// Called when the input is interrupted by, e.g., a change of focus or caret position.
		void interrupt_input_method() {
			_impl->interrupt_input_method();
		}

		/// Captures the mouse to a given element, so that wherever the mouse moves to, the element always receives
		/// mouse events as if the mouse were over it, until \ref release_mouse_capture is called or if the
		/// capture's broken by the system. Derived classes should override this function to notify the desktop
		/// environment.
		void set_mouse_capture(element&);
		/// Returns the element that currently captures the mouse, or \p nullptr.
		[[nodiscard]] virtual element *get_mouse_capture() const {
			return _capture;
		}
		/// Releases the mouse capture. Derived classes should override this function to notify the system.
		void release_mouse_capture() {
			logger::get().log_debug(CP_HERE) << "release mouse capture";
			assert_true_usage(_capture != nullptr, "mouse not captured");
			_capture = nullptr;
			// TODO send a mouse_move message to correct mouse over information?
			// notify the system
			_impl->release_mouse_capture();
		}
		/// If the mouse is captured, returns the mouse cursor of \ref _capture; otherwise falls back to
		/// the default behavior.
		[[nodiscard]] cursor get_current_display_cursor() const override;

		/// Returns the underlying implementation of this window.
		[[nodiscard]] _details::window_impl &get_impl() const {
			return *_impl;
		}

		info_event<>
			close_request, ///< Invoked when the user clicks the `close' button.
			got_window_focus, ///< Invoked when the window gets keyboard focus.
			lost_window_focus; ///< Invoked when the window loses keyboard focus.
		info_event<size_changed_info> size_changed; ///< Invoked when the window's size has changed.
		/// Invoked when the window's scaling factor has been changed.
		info_event<scaling_factor_changed_info> scaling_factor_changed;

		/// Default class name for \ref window.
		inline static std::u8string_view get_default_class() {
			return u8"window";
		}
	protected:
		std::any _renderer_data; ///< Renderer-specific data associated with this window.
		std::unique_ptr<_details::window_impl> _impl; ///< The implementation of this window.
		element *_capture = nullptr; ///< The element that captures the mouse.

		/// Updates \ref _cached_mouse_position and \ref _cached_mouse_position_timestamp, and returns a
		/// corresponding \ref mouse_position object. Note that the input position is in device independent units.
		mouse_position _update_mouse_position(vec2d);

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
		virtual void _on_lost_window_capture();


		/// Registers the window to \ref renderer_base.
		void _initialize(std::u8string_view) override;
		/// Deletes all decorations, releases the focus, and unregisters the window from \ref renderer_base.
		void _dispose() override;

		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_enter() override;
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_leave() override;
		/// If the mouse is captured by an element, forwards the event to it. Otherwise falls back to the default
		/// behavior.
		void _on_mouse_move(mouse_move_info&) override;
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_down(mouse_button_info&) override;
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_up(mouse_button_info&) override;
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_scroll(mouse_scroll_info&) override;

		/// Creates a \ref _details::window_impl for this window. This function will be called in
		/// \ref _initiaslize() before everything else, and is platform-dependent.
		[[nodiscard]] static std::unique_ptr<_details::window_impl> _create_impl(window&);
	};
}
