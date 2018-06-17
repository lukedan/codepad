#pragma once

/// \file
/// Classes related to ``window''s.

#include <chrono>
#include <functional>

#include "renderer.h"
#include "../ui/panel.h"
#include "../ui/commands.h"
#include "../ui/element_classes.h"
#include "../core/event.h"
#include "../core/encodings.h"

namespace codepad::os {
	/// Contains information about the resizing of windows.
	struct size_changed_info {
		/// Initializes the struct with the given new size.
		explicit size_changed_info(vec2i v) : new_size(v) {
		}
		const vec2i new_size; ///< The new size of the window.
	};
	/// Base class of all windows. Defines basic interfaces that all windows should implement.
	class window_base : public ui::panel {
		friend class ui::element_collection;
		friend class ui::decoration;
	public:
		/// Sets the caption of the window.
		virtual void set_caption(const str_t&) = 0;
		/// Retrieves the position of the top-left corner of the window's client region relative to the desktop.
		virtual vec2i get_position() const = 0;
		/// Moves the window so that the top-left corner of the window's client region is at the given position.
		virtual void set_position(vec2i) = 0;
		/// Returns the size of the window's client region.
		virtual vec2i get_client_size() const = 0;
		/// Sets the size the window's client region.
		virtual void set_client_size(vec2i) = 0;

		/// Brings the window to foreground and activates it.
		virtual void activate() = 0;
		/// Indicate to the user that the window needs attention, but without changing its state.
		virtual void prompt_ready() = 0;

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

		/// Tests if the given point is in the window, including its title bar and borders.
		virtual bool hit_test_full_client(vec2i) const = 0;

		/// Obtains the corresponding position in client coordinates.
		virtual vec2i screen_to_client(vec2i) const = 0;
		/// Obtains the corresponding position in screen coordinates.
		virtual vec2i client_to_screen(vec2i) const = 0;

		/// Called to set the position of the currently active caret. This position
		/// is used by input methods to display components such as the candidate window.
		virtual void set_active_caret_position(rectd) = 0;
		/// Called when the input is interrupted by, e.g., a change of focus or caret position.
		virtual void interrupt_input_method() = 0;

		/// Captures the mouse to a given ui::element, so that wherever the mouse moves to, the ui::element
		/// always receives notifications as if the mouse were over it, until \ref release_mouse_capture is
		/// called. Derived classes should override this function to notify the desktop environment.
		virtual void set_mouse_capture(ui::element &elem) {
			logger::get().log_verbose(
				CP_HERE, "set mouse capture 0x", &elem,
				" <", demangle(typeid(elem).name()), ">"
			);
			assert_true_usage(_capture == nullptr, "mouse already captured");
			_capture = &elem;
		}
		/// Returns the ui::element that currently captures the mouse, or \p nullptr.
		virtual ui::element *get_mouse_capture() const {
			return _capture;
		}
		/// Releases the mouse capture. Derived classes should override this function to notify the system.
		virtual void release_mouse_capture() {
			logger::get().log_verbose(CP_HERE, "release mouse capture");
			assert_true_usage(_capture != nullptr, "mouse not captured");
			_capture = nullptr;
		}
		/// If the mouse is captured, returns the mouse cursor of \ref _capture; otherwise falls back to
		/// the default behavior.
		cursor get_current_display_cursor() const override {
			if (_capture) {
				return _capture->get_current_display_cursor();
			}
			return panel::get_current_display_cursor();
		}

		/// Registers a ui::decoration to this window. When the window is disposed, all decorations registered to
		/// it will also automatically be disposed.
		void register_decoration(ui::decoration &dec) {
			assert_true_usage(dec._wnd == nullptr, "the decoration has already been registered to another window");
			dec._wnd = this;
			_decos.push_back(&dec);
			dec._tok = --_decos.end();
			invalidate_layout();
		}
		/// Unregisters a ui::decoration from this window.
		void unregister_decoration(ui::decoration &dec) {
			assert_true_usage(dec._wnd == this, "the decoration is not registered to this window");
			_decos.erase(dec._tok);
			dec._wnd = nullptr;
			invalidate_layout();
		}

		/// Returns the \ref ui::element that's focused within this window. The focused element in a window is
		/// preserved even if the window loses focus.
		ui::element *get_window_focused_element() const {
			return _focus;
		}
		/// Called to set the focused element within this window. This function invokes appropriate handlers and
		/// refreshes the list of \ref ui::element_hotkey_group "element_hotkey_groups"
		void set_window_focused_element(ui::element &e) {
			assert_true_logical(e.get_window() == this, "corrupted element tree");
			if (&e != _focus) {
				ui::element *oldfocus = _focus;
				_focus = &e;
				std::vector<ui::element_hotkey_group_data> gps;
				for (ui::element *cur = _focus; cur != nullptr; cur = cur->parent()) {
					const ui::element_hotkey_group
						*gp = ui::class_manager::get().hotkeys.find(cur->get_class());
					gps.push_back(ui::element_hotkey_group_data(gp, cur));
				}
				hotkey_manager.reset_groups(gps);
				oldfocus->_on_lost_focus();
				e._on_got_focus();
			}
		}

		event<void>
			close_request, ///< Invoked when the user clicks the `close' button.
			got_window_focus, ///< Invoked when the window gets keyboard focus.
			lost_window_focus; ///< Invoked when the window loses keyboard focus.
		event<size_changed_info> size_changed; ///< Invoked when the window's size has changed.

		ui::window_hotkey_manager hotkey_manager; ///< Manages hotkeys of the window.
	protected:
		/// The list of decorations associated with this window. Since decorations are automatically
		/// unregistered when disposed, special care must be taken when iterating through the list
		/// while deleting its entries.
		std::list<ui::decoration*> _decos;
		ui::element
			*_focus{this}, ///< The focused element within this window.
			*_capture = nullptr; ///< The element that captures the mouse.

		/// Calls renderer_base::begin to start rendering to this window.
		void _on_prerender() override {
			os::renderer_base::get().begin(*this);
			panel::_on_prerender();
		}
		/// Calls renderer_base::end to finish rendering to this window.
		void _on_postrender() override {
			panel::_on_postrender();
			os::renderer_base::get().end();
		}

		/// Called when the user clicks the `close' button.
		virtual void _on_close_request() {
			close_request();
		}
		/// Called when the window's size has changed.
		virtual void _on_size_changed(size_changed_info &p) {
			invalidate_layout();
			size_changed(p);
		}

		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_down(ui::key_info &p) override {
			if (_focus != this) {
				_focus->_on_key_down(p);
			} else {
				panel::_on_key_down(p);
			}
		}
		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_up(ui::key_info &p) override {
			if (_focus != this) {
				_focus->_on_key_up(p);
			} else {
				panel::_on_key_up(p);
			}
		}
		/// Forwards the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_keyboard_text(ui::text_info &p) override {
			if (_focus != this) {
				_focus->_on_keyboard_text(p);
			} else {
				panel::_on_keyboard_text(p);
			}
		}
		/// Forwards the composition event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_composition(ui::composition_info &p) override {
			if (_focus != this) {
				_focus->_on_composition(p);
			} else {
				panel::_on_composition(p);
			}
		}
		/// Forwards the composition event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_composition_finished() override {
			if (_focus != this) {
				_focus->_on_composition_finished();
			} else {
				panel::_on_composition_finished();
			}
		}


		/// Does nothing. The layout is automatically updated when the window is resized.
		void _recalc_layout(rectd) override {
		}


		/// Called when an element is being removed from this window. Resets the focus if the
		/// removed element has it.
		virtual void _on_removing_window_element(ui::element *e) {
			ui::element *ef = _focus;
			for (; ef != nullptr && e != ef; ef = ef->parent()) {
			}
			if (ef != nullptr) {
				set_window_focused_element(*this);
			}
		}
		/// Called when the window gets the focus. Calls \ref ui::manager::_on_window_got_focus, and invokes
		/// \ref ui::element::_on_got_focus on \ref _focus.
		virtual void _on_got_window_focus();
		/// Called when the window loses the focus. Calls \ref ui::manager::_on_window_lost_focus, and invokes
		/// \ref ui::element::_on_lost_focus on \ref _focus.
		virtual void _on_lost_window_focus();

		/// Called when mouse capture is broken by the user's actions.
		virtual void _on_lost_window_capture() {
			if (_capture != nullptr) {
				_capture->_on_capture_lost();
				_capture = nullptr;
			}
		}


		/// Renders all the window's children, then renders all decorations on top of the rendered result.
		void _custom_render() override {
			panel::_custom_render();
			// render decorations
			bool has_active = false;
			for (auto i = _decos.begin(); i != _decos.end(); ) {
				if ((*i)->_st.update_and_render((*i)->_layout)) {
					has_active = true;
				} else {
					if (test_bit_all(
						(*i)->get_state(), ui::visual::get_predefined_states().corpse
					)) { // a dead corpse
						auto j = i;
						++j;
						delete *i; // the entry in _decos will be automatically removed here
						i = j;
						continue;
					}
				}
				++i;
			}
			if (has_active) {
				invalidate_visual();
			}
		}

		/// Called in ui::decoration::~decoration() to automatically unregister the decoration.
		virtual void _on_decoration_destroyed(ui::decoration &d) {
			assert_true_logical(d._wnd == this, "calling the wrong window");
			_decos.erase(d._tok);
			invalidate_layout();
		}


		/// Registers the window to \ref renderer_base.
		void _initialize() override {
			panel::_initialize();
			renderer_base::get()._new_window(*this);
		}
		/// Deletes all decorations, releases the focus, and unregisters the window from \ref renderer_base.
		void _dispose() override;

		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_enter() override {
			if (_capture != nullptr) {
				_capture->_on_mouse_enter();
				ui::element::_on_mouse_enter();
			} else {
				panel::_on_mouse_enter();
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_leave() override {
			if (_capture != nullptr) {
				_capture->_on_mouse_leave();
				ui::element::_on_mouse_leave();
			} else {
				panel::_on_mouse_leave();
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_move(ui::mouse_move_info &p) override {
			if (_capture != nullptr) {
				if (!_capture->is_mouse_over()) {
					_capture->_on_mouse_enter();
				}
				_capture->_on_mouse_move(p);
				ui::element::_on_mouse_move(p);
			} else {
				panel::_on_mouse_move(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_down(ui::mouse_button_info &p) override {
			if (_capture != nullptr) {
				_capture->_on_mouse_down(p);
				mouse_down(p);
			} else {
				panel::_on_mouse_down(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_up(ui::mouse_button_info &p) override {
			if (_capture != nullptr) {
				_capture->_on_mouse_up(p);
				ui::element::_on_mouse_up(p);
			} else {
				panel::_on_mouse_up(p);
			}
		}
		/// If the mouse is captured by an element, forwards the event to the element. Otherwise falls back
		/// to the default behavior.
		void _on_mouse_scroll(ui::mouse_scroll_info &p) override {
			if (_capture != nullptr) {
				for (element *e = _capture; !p.handled() && e != this; e = e->parent()) {
					assert_true_logical(e, "corrupted element tree");
					e->_on_mouse_scroll(p);
				}
				ui::element::_on_mouse_scroll(p);
			} else {
				panel::_on_mouse_scroll(p);
			}
		}
	};
}
