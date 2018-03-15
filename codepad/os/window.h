#pragma once

/// \file
/// Classes related to `window's.

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
		friend class ui::manager;
		friend class ui::element_collection;
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

		/// Captures the mouse to a given ui::element, so that wherever the mouse moves to, the ui::element
		/// always receives notifications as if the mouse were over it, until \ref release_mouse_capture is
		/// called. Derived classes should override this function to notify the system.
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

		/// Creates a ui::decoration bound to this window.
		ui::decoration *create_decoration() {
			ui::decoration *d = new ui::decoration();
			d->_wnd = this;
			_decos.push_back(d);
			d->_tok = --_decos.end();
			return d;
		}
		/// Disposes the given ui::decoration. The ui::decoration must have been created by this window.
		void delete_decoration(ui::decoration *d) {
			_decos.erase(d->_tok);
			delete d;
			invalidate_visual();
		}

		event<void>
			close_request, ///< Invoked when the user clicks the `close' button.
			got_window_focus, ///< Invoked when the window gets keyboard focus.
			lost_window_focus; ///< Invoked when the window loses keyboard focus.
		event<size_changed_info> size_changed; ///< Invoked when the window's size has changed.

		ui::window_hotkey_manager hotkey_manager; ///< Manages hotkeys of the window.
	protected:
		std::list<ui::decoration*> _decos; ///< The list of decorations associated with this window.
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

		/// Sets the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_down(ui::key_info &p) override {
			if (_focus && _focus != this) {
				_focus->_on_key_down(p);
			} else {
				panel::_on_key_down(p);
			}
		}
		/// Sets the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_key_up(ui::key_info &p) override {
			if (_focus && _focus != this) {
				_focus->_on_key_up(p);
			} else {
				panel::_on_key_up(p);
			}
		}
		/// Sets the keyboard event to the focused element, or, if the window itself is focused,
		/// falls back to the default behavior.
		void _on_keyboard_text(ui::text_info &p) override {
			if (_focus && _focus != this) {
				_focus->_on_keyboard_text(p);
			} else {
				panel::_on_keyboard_text(p);
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
				if (ui::manager::get().get_focused() == _focus) {
					ui::manager::get().set_focus(this);
				} else {
					_set_window_focus_element(this);
				}
			}
		}
		/// Called to set the focused element within this window. That element is automatically focused
		/// when the window gets the focus.
		virtual void _set_window_focus_element(ui::element *e) {
			assert_true_logical(e && e->get_window() == this, "corrupted element tree");
			if (e != _focus) {
				_focus = e;
				std::vector<ui::element_hotkey_group_data> gps;
				for (ui::element *cur = _focus; cur != nullptr; cur = cur->parent()) {
					const ui::element_hotkey_group
						*gp = ui::class_manager::get().hotkeys.find(cur->get_class());
					gps.push_back(ui::element_hotkey_group_data(gp, cur));
				}
				hotkey_manager.reset_groups(gps);
			}
		}

		/// Called when the window gets the focus.
		virtual void _on_got_window_focus() {
			ui::manager::get().set_focus(_focus);
			got_window_focus.invoke();
		}
		/// Called when the window loses the focus.
		virtual void _on_lost_window_focus() {
			if (ui::manager::get().get_focused() == _focus) { // in case the focus has already shifted
				ui::manager::get().set_focus(nullptr);
			}
			lost_window_focus.invoke();
		}
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
					)) {
						// a dead corpse
						delete *i;
						i = _decos.erase(i);
						continue;
					}
				}
				++i;
			}
			if (has_active) {
				invalidate_visual();
			}
		}

		/// Registers the window to \ref renderer_base.
		void _initialize() override {
			panel::_initialize();
			renderer_base::get()._new_window(*this);
		}
		/// Deletes all decorations, releases the focus, and unregisters the window from \ref renderer_base.
		void _dispose() override {
			for (ui::decoration *dec : _decos) {
				delete dec;
			}
			if (ui::manager::get().get_focused() == _focus) {
				ui::manager::get().set_focus(nullptr);
			}
			renderer_base::get()._delete_window(*this);
			panel::_dispose();
		}

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
