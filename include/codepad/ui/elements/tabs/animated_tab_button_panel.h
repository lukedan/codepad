// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes used to add animations to tab buttons.

#include "../../element.h"
#include "../../panel.h"
#include "../stack_panel.h"
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

		/// Returns the default class of elements of type \ref tab.
		inline static std::u8string_view get_default_class() {
			return u8"animated_tab_buttons_panel";
		}
	protected:
		/// The data associated with a \ref tab_button.
		struct _child_data {
			/// Default constructor.
			_child_data() = default;
			/// Initializes all fields of this struct.
			explicit _child_data(info_event<tab_button::drag_start_info>::token tok) : token(tok) {
			}

			/// Starts the animation for this specific tab button. Since this struct does not store any reference to
			/// the element, it must be passed in as the argument.
			void set_offset(animated_tab_buttons_panel &pnl, element &e, double offset) {
				if (!task.empty()) {
					e.get_manager().get_scheduler().cancel_synchronous_task(task);
				}
				current_offset = offset;
				if (
					pnl._get_tab_manager().is_dragging_tab() &&
					&pnl._get_tab_manager().get_dragging_tab()->get_button() == &e
				) {
					// the tab is being dragged, do not start animation
					e.invalidate_layout();
				} else {
					starting_offset = current_offset;
					start = scheduler::clock_t::now();
					task = e.get_manager().get_scheduler().register_synchronous_task(
						scheduler::clock_t::now(), &e,
						[this, panel = &pnl](element *e) -> std::optional<scheduler::clock_t::time_point> {
							e->invalidate_layout();
							double dur = std::chrono::duration<double>(scheduler::clock_t::now() - start).count();
							if (dur > panel->get_animation_duration()) {
								current_offset = 0.0;
								task = scheduler::sync_task_token();
								return std::nullopt;
							}
							double progress = dur / panel->get_animation_duration();
							double position = panel->get_transition_function()(progress);
							current_offset = starting_offset * (1.0 - position);
							return scheduler::clock_t::now();
						}
					);
				}
			}

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

		/// Returns the \ref host that owns this panel.
		host *_get_host() const {
			return dynamic_cast<host*>(logical_parent());
		}
		/// Returns the \ref tab_manager of the \ref host that owns this panel.
		tab_manager &_get_tab_manager() const {
			return _get_host()->get_tab_manager();
		}

		/// Returns the \ref _child_data corresponding to the given \ref element.
		_child_data *_get_data(element &elem) {
			return std::any_cast<_child_data>(&_child_get_parent_data(elem));
		}
		/// Returns the absolute part of the span of the given element in the current orientation.
		double _get_absolute_span(const element &elem) const {
			auto span =
				get_orientation() == orientation::vertical ?
				_get_vertical_absolute_span(elem) :
				_get_horizontal_absolute_span(elem);
			if (span) {
				return span.value();
			}
			logger::get().log_warning(CP_HERE) << "tab buttons should have concrete pixel sizes";
			return 0.0;
		}

		/// Initializes additional information of the newly added element, and moves existing tab buttons.
		void _on_child_added(element &elem, element *before) override {
			stack_panel::_on_child_added(elem, before);
			if (auto *btn = dynamic_cast<tab_button*>(&elem)) { // initialize info
				tab_manager &man = _get_tab_manager();
				if (man.is_dragging_tab() && &man.get_dragging_tab()->get_button() == &elem) {
					// the button is already being dragged
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
				double size = _get_absolute_span(elem);
				for (; it != _children.items().end(); ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset - size);
				}
			}
		}
		/// Unbinds from \ref tab_button::start_drag and resets the additional data of that \ref element..
		void _on_child_removing(element &elem) override {
			stack_panel::_on_child_removing(elem);

			if (auto *btn = dynamic_cast<tab_button*>(&elem)) {
				tab_manager &man = _get_tab_manager();
				if (man.is_dragging_tab() && &man.get_dragging_tab()->get_button() == &elem) {
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
				double size = _get_absolute_span(elem);
				for (; it != _children.items().end(); ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset + size);
				}
			}
		}

		/// Starts animation for affected elements.
		void _on_child_order_changing(element &elem, element *before) override {
			stack_panel::_on_child_order_changing(elem, before);

			// find positions
			auto elemit = _children.items().begin(), beforeit = _children.items().begin();
			bool move_to_begin = true;
			for (; elemit != _children.items().end() && *elemit != &elem; ++elemit) {
			}
			for (; beforeit != _children.items().end() && *beforeit != before; ++beforeit) {
				if (*beforeit == &elem) {
					move_to_begin = false;
				}
			}
			if (auto next = elemit; ++next == beforeit) { // just in place
				return;
			}
			double offset = _get_absolute_span(elem), elemoffset = 0.0;
			// add offset
			if (move_to_begin) {
				for (auto it = beforeit; it != elemit; ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset - offset);
					elemoffset += _get_absolute_span(**it);
				}
			} else {
				for (auto it = ++elemit; it != beforeit; ++it) {
					auto *data = _get_data(**it);
					data->set_offset(*this, **it, data->current_offset + offset);
					elemoffset -= _get_absolute_span(**it);
				}
			}
			auto *data = _get_data(elem);
			data->set_offset(*this, elem, data->current_offset + elemoffset);
		}

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
				tabs::tab_button &button = info.dragging_tab->get_button();
				auto *data = _get_data(button);
				data->set_offset(*this, button, data->current_offset);
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
		void _on_drag_update(tab_drag_update_info &info) {
			// update position in tab list
			host *host = _get_host();
			tab_manager &man = _get_tab_manager();
			tab_button &dragbtn = man.get_dragging_tab()->get_button();
			double accu = 0.0, relpos =
				get_orientation() == orientation::vertical ?
				info.position.y :
				info.position.x;
			auto beforeit = host->get_tabs().items().begin();
			for (element *e : _children.items()) {
				if (e != &dragbtn) {
					double mysz = _get_absolute_span(*e);
					if (accu + 0.5 * mysz > relpos) { // should be here
						break;
					}
					accu += mysz;
				}
				++beforeit;
			}
			// calculate current position
			double curpos = relpos;
			if (get_orientation() == orientation::vertical) {
				if ((dragbtn.get_anchor() & anchor::top) != anchor::none) {
					curpos -= dragbtn.get_margin().top;
				}
			} else {
				if ((dragbtn.get_anchor() & anchor::left) != anchor::none) {
					curpos -= dragbtn.get_margin().left;
				}
			}
			// actually set stuff
			host->move_tab_before(
				*man.get_dragging_tab(),
				beforeit == host->get_tabs().items().end() ? nullptr : dynamic_cast<tab*>(*beforeit)
			);
			auto *data = _get_data(dragbtn);
			data->set_offset(*this, dragbtn, curpos - accu);
		}
	};
}
