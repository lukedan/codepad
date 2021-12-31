// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of tabs.

#include "codepad/core/misc.h"
#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/elements/label.h"
#include "codepad/ui/elements/button.h"

namespace codepad::ui::tabs {
	class host;
	class tab_manager;
	class tab;

	/// A button representing a \ref tab in a \ref host.
	class tab_button : public panel {
		friend tab;
		friend tab_manager;
		friend host;
	public:
		/// Contains information about the user starting to drag a \ref tab_button.
		struct drag_start_info {
			/// Initializes all fields of the struct.
			explicit drag_start_info(vec2d df) : reference(df) {
			}
			/// The offset of the mouse cursor from the top left corner of the \ref tab_button.
			const vec2d reference;
		};

		/// Contains information about the user clicking a \ref tab_button.
		struct click_info {
			/// Initializes all fields of the struct.
			explicit click_info(mouse_button_info &i) : button_info(i) {
			}
			/// The \ref mouse_button_info of the \ref element::mouse_down event.
			mouse_button_info &button_info;
		};

		/// Sets the label displayed on the button.
		void set_label(std::u8string str) {
			_label->set_text(std::move(str));
		}
		/// Returns the label curretly displayed on the button.
		const std::u8string &get_label() const {
			return _label->get_text();
		}

		/// Invoked when the ``close'' button is clicked, or when the user presses the tertiary mouse button on the
		/// \ref tab_button.
		info_event<> request_close;
		info_event<> tab_activated; ///< Invoked when the associated tab is activated.
		info_event<> tab_deactivated; ///< Invoked when the associated tab is deactivated.
		info_event<> tab_selected; ///< Invoked when the associated tab is selected.
		info_event<> tab_deselected; ///< Invoked when the associated tab is deselected.
		info_event<drag_start_info> start_drag; ///< Invoked when the user starts dragging the \ref tab_button.
		info_event<click_info> click; ///< Invoked when the user clicks the \ref tab_button.

		/// Returns the default class of elements of type \ref tab_button.
		inline static std::u8string_view get_default_class() {
			return u8"tab_button";
		}

		/// Returns the name identifier of the label.
		inline static std::u8string_view get_label_name() {
			return u8"label";
		}
		/// Returns the name identifier of the `close' button.
		inline static std::u8string_view get_close_button_name() {
			return u8"close_button";
		}
	protected:
		drag_deadzone _drag; ///< Used when starting dragging.
		/// The reference point for dragging. This is the position of the mouse relative to this \ref tab_button
		/// without transformations (i.e., if no transformations were applied to this element).
		vec2d _drag_pos;
		label *_label = nullptr; ///< Used to display the tab's label.
		button *_close_btn = nullptr; ///< The `close' button.

		/// Handles mouse button interactions.
		///
		/// \todo Make actions customizable.
		void _on_mouse_down(mouse_button_info&) override;
		/// Updates \ref _drag, and invokes \ref start_drag if necessary.
		void _on_mouse_move(mouse_move_info &p) override {
			if (_drag.is_active()) {
				if (_drag.update(p.new_position, *this)) {
					start_drag.invoke_noret(_drag_pos);
				}
			}
			panel::_on_mouse_move(p);
		}
		/// Cancels \ref _drag if necessary.
		void _on_mouse_up(mouse_button_info &p) override {
			if (_drag.is_active()) {
				_drag.on_cancel(*this);
			}
			panel::_on_mouse_up(p);
		}
		/// Cancels \ref _drag if necessary.
		void _on_capture_lost() override {
			_drag.on_capture_lost();
			panel::_on_capture_lost();
		}

		/// Registers \ref tab_activated and \ref tab_deactivated events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_event(name, u8"tab_activated", tab_activated, callback) ||
				_event_helpers::try_register_event(name, u8"tab_deactivated", tab_deactivated, callback) ||
				_event_helpers::try_register_event(name, u8"tab_selected", tab_selected, callback) ||
				_event_helpers::try_register_event(name, u8"tab_deselected", tab_deselected, callback) ||
				panel::_register_event(name, std::move(callback));
		}
		/// Handles \ref _label and \ref _close_btn, and registers for events.
		bool _handle_reference(std::u8string_view, element*) override;

		/// Called when the associated tab is activated. Invokes \ref tab_activated.
		virtual void _on_tab_activated() {
			tab_activated.invoke();
		}
		/// Called when the associated tab is deactivated. Invokes \ref tab_deactivated.
		virtual void _on_tab_deactivated() {
			tab_deactivated.invoke();
		}
		/// Called when the associated tab is selected. Invokes \ref tab_selected.
		virtual void _on_tab_selected() {
			tab_selected.invoke();
		}
		/// Called when the associated tab is de-selected. Invokes \ref tab_deselected.
		virtual void _on_tab_deselected() {
			tab_deselected.invoke();
		}
	};

	/// A tab that contains other elements.
	class tab : public panel {
		friend host;
		friend tab_manager;
	public:
		/// Sets the text displayed on the \ref tab_button.
		void set_label(std::u8string s) {
			_btn->set_label(std::move(s));
		}
		/// Returns the currently displayed text on the \ref tab_button.
		const std::u8string &get_label() const {
			return _btn->get_label();
		}

		/// Marks this tab as selected if it isn't.
		void select() {
			if (!_selected) {
				_selected = true;
				_on_selected();
			}
		}
		/// Marks this tab as unselected if it's selected.
		void deselect();
		/// Returns whether this tab is selected.
		[[nodiscard]] bool is_selected() const {
			return _selected;
		}

		/// Requests that this tab be closed. Derived classes can override this to decide whether to actually close
		/// the tab.
		///
		/// \return Whether the request has been accepted.
		virtual bool request_close() {
			_on_close();
			return true;
		}

		/// Returns the associated \ref tab_button.
		[[nodiscard]] tab_button &get_button() const {
			return *_btn;
		}
		/// Returns the \ref host that this tab is currently in, which should be its logical parent.
		[[nodiscard]] host *get_host() const {
			return _host;
		}
		/// Returns the manager of this tab.
		[[nodiscard]] tab_manager &get_tab_manager() const {
			return *_tab_manager;
		}

		info_event<> activated; ///< Invoked when this tab is activated.
		info_event<> deactivated; ///< Invoked when this tab is deactivated.
		info_event<> selected; ///< Invoked when this tab is selected (including when it's activated).
		/// Invoked when this tab is deselected (including when it's deactivated and deselected).
		info_event<> deselected;

		/// Returns the default class of elements of type \ref tab.
		inline static std::u8string_view get_default_class() {
			return u8"tab";
		}
	protected:
		/// Called when this tab is closed. By default, this function simply marks this tab for disposal. Derived
		/// classes can override this for further cleanup.
		virtual void _on_close();

		/// Initializes \ref _btn and sets \ref _is_focus_scope to \p true.
		void _initialize() override;
		/// Marks \ref _btn for disposal.
		void _dispose() override;

		/// Registers \ref activated, \ref deactivated, \ref selected, and \ref deselected events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_event(name, u8"activated", activated, callback) ||
				_event_helpers::try_register_event(name, u8"deactivated", deactivated, callback) ||
				_event_helpers::try_register_event(name, u8"selected", selected, callback) ||
				_event_helpers::try_register_event(name, u8"deselected", deselected, callback) ||
				panel::_register_event(name, std::move(callback));
		}

		/// Called when this tab is activated. Invokes \ref tab_button::_on_tab_activated() and \ref activated.
		virtual void _on_activated() {
			_btn->_on_tab_activated();
			activated.invoke();
		}
		/// Called when this tab is deactivated. Invokes \ref tab_button::_on_tab_deactivated() and \ref deactivated.
		virtual void _on_deactivated() {
			_btn->_on_tab_deactivated();
			deactivated.invoke();
		}
		/// Called when this tab is selected. This is guaranteed to be called before this tab is activated; i.e., if
		/// a tab is in `activated' state, it's guaranteed to be selected.
		virtual void _on_selected() {
			_btn->_on_tab_selected();
			selected.invoke();
		}
		/// Called when this tab is de-selected. This is guarateed to be called when this tab is not activated.
		virtual void _on_deselected() {
			_btn->_on_tab_deselected();
			deselected.invoke();
		}
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
		tab_button *_btn = nullptr; ///< The \ref tab_button associated with tab.
		host *_host = nullptr; ///< The \ref host associated with this tab.
		bool _selected = false; ///< Whether this tab has been selected.
		std::uint32_t _history_index = 0; ///< Indicates when this tab was last focused on.
	};
}
