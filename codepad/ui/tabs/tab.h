// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of tabs.

#include "../../core/misc.h"
#include "../element.h"
#include "../panel.h"
#include "../common_elements.h"

namespace codepad::ui::tabs {
	class host;
	class tab_manager;
	class tab;

	/// A button representing a \ref tab in a \ref host.
	class tab_button : public panel {
		friend tab_manager;
		friend host;
	public:
		/// The minimum distance the mouse cursor have to move before dragging starts.
		///
		/// \todo Combine different declarations and use system default.
		constexpr static double drag_pivot = 5.0;
		/// The default padding.
		///
		/// \todo Make this customizable.
		constexpr static thickness content_padding = thickness(5.0);

		/// Contains information about the user starting to drag a \ref tab_button.
		struct drag_start_info {
			/// Initializes all fields of the struct.
			explicit drag_start_info(vec2d df) : drag_diff(df) {
			}
			const vec2d drag_diff; ///< The offset of the mouse cursor from the top left corner of the \ref tab_button.
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
		void set_label(str_t str) {
			_label->set_text(std::move(str));
		}
		/// Returns the label curretly displayed on the button.
		const str_t &get_label() const {
			return _label->get_text();
		}

		/// Invoked when the ``close'' button is clicked, or when the user presses the tertiary mouse button on the
		/// \ref tab_button.
		info_event<> request_close;
		info_event<drag_start_info> start_drag; ///< Invoked when the user starts dragging the \ref tab_button.
		info_event<click_info> click; ///< Invoked when the user clicks the \ref tab_button.

		/// Returns the default class of elements of type \ref tab_button.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("tab_button");
		}

		/// Returns the name identifier of the label.
		inline static str_view_t get_label_name() {
			return CP_STRLIT("label");
		}
		/// Returns the name identifier of the `close' button.
		inline static str_view_t get_close_button_name() {
			return CP_STRLIT("close_button");
		}
	protected:
		label *_label; ///< Used to display the tab's label.
		button *_close_btn; ///< The `close' button.
		vec2d _mdpos; ///< The positon where the user presses the primary mouse button.
		/// Indicates whether the user has pressed the primary mouse button when hovering over this element and may
		/// or may not start dragging.
		bool _predrag = false;

		/// Handles mouse button interactions.
		///
		/// \todo Make actions customizable.
		void _on_mouse_down(mouse_button_info &p) override {
			if (
				p.button == mouse_button::primary &&
				!_close_btn->is_mouse_over()
				) {
				_mdpos = p.position.get(*this);
				_predrag = true;
				get_manager().get_scheduler().schedule_element_update(*this);
				click.invoke_noret(p);
			} else if (p.button == mouse_button::tertiary) {
				request_close.invoke();
			}
			panel::_on_mouse_down(p);
		}

		/// Checks and starts dragging.
		void _on_update() override {
			panel::_on_update();
			if (_predrag) {
				if (os::is_mouse_button_down(mouse_button::primary)) {
					vec2d diff =
						get_window()->screen_to_client(os::get_mouse_position()).convert<double>() - _mdpos;
					if (diff.length_sqr() > drag_pivot * drag_pivot) {
						_predrag = false;
						start_drag.invoke_noret(_mdpos - get_layout().xmin_ymin());
					} else {
						get_manager().get_scheduler().schedule_element_update(*this);
					}
				} else {
					_predrag = false;
				}
			}
		}

		/// Initializes \ref _close_btn.
		void _initialize(str_view_t cls, const element_configuration &config) override {
			panel::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_label_name(), _name_cast(_label)},
				{get_close_button_name(), _name_cast(_close_btn)}
				});

			_close_btn->click += [this]() {
				request_close.invoke();
			};
		}
	};

	/// A tab that contains other elements.
	class tab : public panel {
		friend host;
		friend tab_manager;
	public:
		/// Sets the text displayed on the \ref tab_button.
		void set_label(str_t s) {
			_btn->set_label(std::move(s));
		}
		/// Returns the currently displayed text on the \ref tab_button.
		const str_t &get_label() const {
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

		/// Returns the default class of elements of type \ref tab.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("tab");
		}
	protected:
		tab_button *_btn; ///< The \ref tab_button associated with tab.

		/// Called when \ref request_close is called to handle the user's request of closing this tab. By default,
		/// this function removes this tab from the host, then marks this for disposal.
		virtual void _on_close_requested();

		// TODO notify the button when this element is selected

		/// Initializes \ref _btn.
		void _initialize(str_view_t, const element_configuration&) override;
		/// Marks \ref _btn for disposal.
		void _dispose() override {
			get_manager().get_scheduler().mark_for_disposal(*_btn);
			panel::_dispose();
		}
	private:
		tab_manager *_tab_manager = nullptr; ///< The manager of this tab.
	};
}
