// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to add animations to tab buttons.

#include "codepad/ui/element.h"
#include "codepad/ui/panel.h"
#include "codepad/ui/elements/stack_panel.h"
#include "host.h"
#include "manager.h"

namespace codepad::ui::tabs {
	/// A panel that adds animations to \ref tab_button "tab buttons" when they're moved around. This only works when
	/// the tab buttons have fixed sizes and margins in the direction they're laid out.
	class animated_tab_buttons_panel : public stack_panel {
	public:
		/// Returns the duration of tab button animations.
		double get_animation_duration() const {
			return _animation_duration;
		}

		/// Returns the transition function used for the animation.
		const std::function<double(double)> &get_transition_function() const {
			return _transition;
		}

		/// Returns the role of \ref _host.
		inline static std::u8string_view get_host_role() {
			return u8"host";
		}
		/// Returns the default class of elements of type \ref tab.
		inline static std::u8string_view get_default_class() {
			return u8"animated_tab_buttons_panel";
		}
	protected:
		/// Returns whether the given tab button is being dragged.
		[[nodiscard]] inline static bool _is_tab_button_being_dragged(tab_manager &tabman, element &e) {
			if (tabman.is_dragging_any_tab()) {
				for (tab *t : tabman.get_dragging_tabs()) {
					if (&t->get_button() == &e) {
						return true;
					}
				}
			}
			return false;
		}

		/// The data associated with a \ref tab_button.
		struct _child_data {
			/// Default constructor.
			_child_data() = default;
			/// Initializes all fields of this struct.
			explicit _child_data(info_event<tab_button::drag_start_info>::token tok) : token(tok) {
			}

			/// Starts the animation for this specific tab button. Since this struct does not store any reference to
			/// the element, it must be passed in as the argument.
			void set_offset(animated_tab_buttons_panel&, element&, double offset);

			/// The token used to listen to \ref tab_button::start_drag.
			info_event<tab_button::drag_start_info>::token token;
			scheduler::sync_task_token task; ///< The task used to update the animation.
			scheduler::clock_t::time_point start; ///< The start of the current animation.
			double
				starting_offset = 0.0, ///< The starting offset of the tab button from its original position.
				current_offset = 0.0; ///< Current offset of the tab button.
		};

		info_event<tab_drag_ended_info>::token _droptok; ///< Used to handle \ref tab_manager::drag_ended.
		/// Used to handle \ref tab_manager::drag_move_tab_button.
		info_event<tab_drag_update_info>::token _updatetok;
		/// The transition function.
		std::function<double(double)> _transition = transition_functions::convex_quadratic;
		double _animation_duration = 0.1; ///< Duration of tab button animations.
		host *_host; ///< The \ref host that owns this panel.

		/// Returns the \ref tab_manager of the \ref host that owns this panel.
		[[nodiscard]] tab_manager &_get_tab_manager() const {
			return _host->get_tab_manager();
		}

		/// Returns the \ref _child_data corresponding to the given \ref element.
		[[nodiscard]] _child_data *_get_data(element &elem) {
			return std::any_cast<_child_data>(&_child_get_parent_data(elem));
		}
		/// Returns a \ref stack_layout_helper object that can be used for computing tab button positions. The
		/// returned object will have data from all children (except \p exclude) accumulated.
		[[nodiscard]] stack_layout_helper _get_children_layout_helper(element *exclude = nullptr) const {
			rectd client = get_client_region();
			stack_layout_helper result;
			if (get_orientation() == orientation::horizontal) {
				result = stack_layout_helper(client.xmin, client.width(), get_orientation());
			} else {
				result = stack_layout_helper(client.ymin, client.height(), get_orientation());
			}
			for (element *child : _children.items()) {
				if (child != exclude) {
					result.accumulate(*child);
				}
			}
			return result;
		}

		/// Initializes additional information of the newly added element, and moves existing tab buttons.
		void _on_child_added(element &elem, element *before) override {
			stack_panel::_on_child_added(elem, before);
			if (auto *btn = dynamic_cast<tab_button*>(&elem)) { // initialize info
				tab_manager &man = _get_tab_manager();
				// only respond to the first element - we don't want to register for the event multiple times
				if (man.is_dragging_any_tab() && &man.get_dragging_tabs()[0]->get_button() == &elem) {
					// the button is being dragged into this host
					_on_start_drag();
				}
				auto tok = btn->start_drag += [this](tab_button::drag_start_info&) {
					_on_start_drag();
				};
				_child_get_parent_data(elem).emplace<_child_data>(tok);
			}

			// update positions for all elements after elem
			// well, after testing this is actually handled in _on_drag_update() and _on_child_order_changing(),
			// but we'll keep this just in case
			auto it = _children.items().begin();
			for (; it != _children.items().end() && *it != before; ++it) {
			}
			if (it != _children.items().end()) {
				double size = _get_children_layout_helper().compute_span_for(elem);
				for (; it != _children.items().end(); ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset - size);
				}
			}
		}
		/// Unbinds from \ref tab_button::start_drag and resets the additional data of that \ref element.
		void _on_child_removing(element &elem) override {
			stack_panel::_on_child_removing(elem);

			if (auto *btn = dynamic_cast<tab_button*>(&elem)) {
				tab_manager &man = _get_tab_manager();
				// only respond to the first element - don't unregister multiple times
				if (man.is_dragging_any_tab() && &man.get_dragging_tabs()[0]->get_button() == &elem) {
					// the button is being dragged away
					_on_end_drag();
				}
				std::any &data = _child_get_parent_data(elem);
				btn->start_drag -= std::any_cast<_child_data>(&data)->token;
				data.reset();
			}

			// update positions for all elements after elem
			auto it = _children.items().begin();
			for (; it != _children.items().end() && *it != &elem; ++it) {
			}
			++it; // ok since elem is still in _children
			if (it != _children.items().end()) {
				double size = _get_children_layout_helper().compute_span_for(elem);
				for (; it != _children.items().end(); ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset + size);
				}
			}
		}

		/// Starts animation for affected elements.
		void _on_child_order_changing(element &elem, element *before) override;

		/// Updates the layout of all children like \ref stack_panel, but adds the offset to it.
		void _on_update_children_layout() override {
			stack_panel::_on_update_children_layout();

			for (element *e : _children.items()) {
				double off = _get_data(*e)->current_offset;
				rectd layout = e->get_layout();
				if (get_orientation() == orientation::vertical) {
					_child_set_vertical_layout(*e, layout.ymin + off, layout.ymax + off);
				} else {
					_child_set_horizontal_layout(*e, layout.xmin + off, layout.xmax + off);
				}
			}
		}

		/// Called when the user starts dragging a \ref tab_button in this panel, or when a \ref tab_button that's
		/// being dragged enters this panel.
		void _on_start_drag() {
			tab_manager &man = _get_tab_manager();
			_droptok = man.drag_ended += [this](tab_drag_ended_info &info) {
				// animation
				for (tab *t : info.dragging_tabs) {
					tabs::tab_button &button = t->get_button();
					auto *data = _get_data(button);
					data->set_offset(*this, button, data->current_offset);
				}
				_on_end_drag();
			};
			_updatetok = man.drag_move_tab_button += [this](tab_drag_update_info &info) {
				_on_drag_update(info);
			};
		}
		/// Called when the user stops dragging a tab or when the tab is dragged away from this panel, to start
		/// animations and unregister handlers.
		void _on_end_drag() {
			// unregister events
			tab_manager &man = _get_tab_manager();
			man.drag_ended -= _droptok;
			man.drag_move_tab_button -= _updatetok;
		}
		/// Called when \ref tab_manager::drag_move_tab_button is invoked.
		void _on_drag_update(tab_drag_update_info&);

		/// Handles \ref _host.
		bool _handle_reference(std::u8string_view role, element *elem) override {
			if (role == get_host_role()) {
				_reference_cast_to(_host, elem);
				return true;
			}
			return stack_panel::_handle_reference(role, elem);
		}
	};
}
