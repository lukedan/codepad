// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of tabs.

#include "../../../core/misc.h"
#include "../../element.h"
#include "../../panel.h"
#include "../label.h"
#include "../button.h"

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
		/// The default padding.
		///
		/// \todo Make this customizable.
		constexpr static thickness content_padding = thickness(5.0);

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

		info_event<>
			/// Invoked when the ``close'' button is clicked, or when the user presses the tertiary mouse button on the
			/// \ref tab_button.
			request_close,
			tab_selected, ///< Invoked when the associated tab is selected.
			tab_unselected; ///< Invoked when the associated tab is unselected.
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

		/// Registers \ref tab_selected and \ref tab_unselected events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_event(name, u8"tab_selected", tab_selected, callback) ||
				_event_helpers::try_register_event(name, u8"tab_unselected", tab_unselected, callback) ||
				panel::_register_event(name, std::move(callback));
		}

		/// Called when the associated tab is selected. Invokes \ref tab_selected.
		virtual void _on_tab_selected() {
			tab_selected.invoke();
		}
		/// Called when the associated tab is unselected. Invokes \ref tab_unselected.
		virtual void _on_tab_unselected() {
			tab_unselected.invoke();
		}

		/// Adds \ref _label and \ref _close_btn to the mapping.
		class_arrangements::notify_mapping _get_child_notify_mapping() override;

		/// Initializes \ref _close_btn.
		void _initialize(std::u8string_view cls) override;
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

		/// Requests that this tab be closed. Derived classes should override \ref _on_close_requested to add
		/// additional behavior.
		void request_close() {
			_on_close_requested();
		}

		/// Returns the associated \ref tab_button.
		tab_button &get_button() const {
			return *_btn;
		}
		/// Returns the \ref host that this tab is currently in, which should be its logical parent.
		host *get_host() const;
		/// Returns the manager of this tab.
		tab_manager &get_tab_manager() const {
			return *_tab_manager;
		}

		info_event<>
			selected, ///< Invoked when this tab is selected.
			unselected; ///< Invoked when this tab is unselected.

		/// Returns the default class of elements of type \ref tab.
		inline static std::u8string_view get_default_class() {
			return u8"tab";
		}
	protected:
		tab_button *_btn = nullptr; ///< The \ref tab_button associated with tab.

		/// Called when \ref request_close is called to handle the user's request of closing this tab. By default,
		/// this function removes this tab from the host, then marks this for disposal.
		virtual void _on_close_requested();

		/// Initializes \ref _btn.
		void _initialize(std::u8string_view) override;
		/// Marks \ref _btn for disposal.
		void _dispose() override;

		/// Registers \ref selected and \ref unselected events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_event(name, u8"selected", selected, callback) ||
				_event_helpers::try_register_event(name, u8"unselected", unselected, callback) ||
				panel::_register_event(name, std::move(callback));
		}

		/// Called when this tab is selected. Invokes \ref tab_button::_on_tab_selected() and \ref selected.
		virtual void _on_selected() {
			_btn->_on_tab_selected();
			selected.invoke();
		}
		/// Called when this tab is unselected. Invokes \ref tab_button::_on_tab_unselected() and \ref unselected.
		virtual void _on_unselected() {
			_btn->_on_tab_unselected();
			unselected.invoke();
		}
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};
}
