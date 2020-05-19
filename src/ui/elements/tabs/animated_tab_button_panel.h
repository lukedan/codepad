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
	/// Pure virtual class that controls the animation of \ref tab_button "tab_buttons".
	class tab_button_animation_controller {
	public:
		/// Default virtual destructor.
		virtual ~tab_button_animation_controller() = default;

		/// Retrieves or calculates the offset from additional data about a \ref tab_button.
		virtual double get_offset(const std::any&) const = 0;
		/// Sets the offset for a \ref tab_button.
		virtual void set_offset(std::any&, double) const = 0;
		/// Adds the given amount to the offset of the given element.
		virtual void adjust_offset(std::any &data, double off) const {
			set_offset(data, get_offset(data) + off);
		}

		/// Initializes additional data for a \ref tab_button.
		virtual std::any initialize_tab() const = 0;
		/// Updates a \ref tab_button, given the time since last update.
		///
		/// \return Whether the animation needs futher updates.
		virtual bool update_tab(std::any&, double) const = 0;
	};

	/// Tab button animation controller in which updating the position only requires the offset.
	class simple_tab_button_animation_controller : public tab_button_animation_controller {
	public:
		/// Retrieves the offset.
		double get_offset(const std::any &v) const override {
			return std::any_cast<double>(v);
		}
		/// Sets the offset.
		void set_offset(std::any &obj, double v) const override {
			obj.emplace<double>(v);
		}

		/// Returns a \p double initialized to 0.
		std::any initialize_tab() const override {
			return std::make_any<double>(0.0);
		}
		/// Updates the offset by calling \ref _update_tab_impl().
		bool update_tab(std::any &v, double dt) const override {
			return _update_tab_impl(*std::any_cast<double>(&v), dt);
		}
	protected:
		/// Updates the current value based on the delta time given.
		virtual bool _update_tab_impl(double&, double) const = 0;
	};
	/// Tab button animation controller with no animation.
	class trivial_tab_button_animation_controller : public simple_tab_button_animation_controller {
	protected:
		/// Updates the given value.
		bool _update_tab_impl(double &v, double) const override {
			v = 0.0;
			return false;
		}
	};
	/// Tab button animation controller in which the position of the element changes exponentially.
	class exponential_tab_button_animation_controller : public simple_tab_button_animation_controller {
	public:
		/// Returns a reference to \ref _time_scale.
		double &time_scale() {
			return _time_scale;
		}
		/// \overload
		double time_scale() const {
			return _time_scale;
		}
	protected:
		double _time_scale = 20.0; ///< The scale factor for time values.

		/// Updates the given value.
		bool _update_tab_impl(double &v, double dt) const override {
			if (std::abs(v) < 0.1) {
				v = 0.0;
				return false;
			}
			v *= std::exp(-dt * _time_scale);
			return true;
		}
	};
	/// Tab button animation controller in which the position of the element changes exponentially.
	class linear_tab_button_animation_controller : public simple_tab_button_animation_controller {
	public:
		/// Returns a reference to \ref _speed.
		double &speed() {
			return _speed;
		}
		/// \overload
		double speed() const {
			return _speed;
		}
	protected:
		double _speed = 1000.0; ///< The speed of movement.

		/// Updates the given value.
		bool _update_tab_impl(double &v, double dt) const override {
			double dx = _speed * dt;
			if (std::abs(v) < dx) {
				v = 0.0;
				return false;
			}
			if (v > 0.0) {
				v -= dx;
			} else {
				v += dx;
			}
			return true;
		}
	};


	/// A panel that adds animations to \ref tab_button "tab buttons" when they're moved around. This only works when
	/// the tab buttons have fixed sizes and margins in the direction they're laid out.
	class animated_tab_buttons_panel : public stack_panel {
	public:
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
			explicit _child_data(info_event<tab_button::drag_start_info>::token tok, std::any d) :
				token(tok), data(std::move(d)) {
			}

			/// The token used to listen to \ref tab_button::start_drag.
			info_event<tab_button::drag_start_info>::token token;
			std::any data; ///< Animation related data.
		};

		std::shared_ptr<tab_button_animation_controller> _animation; ///< Controls the animation of tabs.
		info_event<>::token _droptok; ///< Used to handle \ref tab_manager::end_drag.
		/// Used to handle \ref tab_manager::drag_move_tab_button.
		info_event<tab_drag_update_info>::token _updatetok;

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
			return
				get_orientation() == orientation::vertical ?
				_get_vertical_absolute_span(elem) :
				_get_horizontal_absolute_span(elem);
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
				_child_get_parent_data(elem).emplace<_child_data>(tok, _animation->initialize_tab());
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
					_animation->adjust_offset(_get_data(**it)->data, -size);
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
					_animation->adjust_offset(_get_data(**it)->data, size);
				}
			}
			get_manager().get_scheduler().schedule_element_update(*this);
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
					_animation->adjust_offset(_get_data(**it)->data, -offset);
					elemoffset += _get_absolute_span(**it);
				}
			} else {
				for (auto it = ++elemit; it != beforeit; ++it) {
					_animation->adjust_offset(_get_data(**it)->data, offset);
					elemoffset -= _get_absolute_span(**it);
				}
			}
			_animation->adjust_offset(_get_data(elem)->data, elemoffset);
			get_manager().get_scheduler().schedule_element_update(*this);
		}

		/// Updates the layout of all children like \ref stack_panel, but adds the offset to it.
		void _on_update_children_layout() override {
			stack_panel::_on_update_children_layout();

			for (element *e : _children.items()) {
				double off = _animation->get_offset(_get_data(*e)->data);
				rectd layout = e->get_layout();
				if (get_orientation() == orientation::vertical) {
					_child_set_vertical_layout(*e, layout.ymin + off, layout.ymax + off);
				} else {
					_child_set_horizontal_layout(*e, layout.xmin + off, layout.xmax + off);
				}
			}
		}

		/// Updates animations.
		void _on_update() override {
			stack_panel::_on_update();

			tab_manager &man = _get_tab_manager();
			double dt = get_manager().get_scheduler().update_delta_time();
			bool stopped = true;
			for (element *e : _children.items()) {
				if (!man.is_dragging_tab() || &man.get_dragging_tab()->get_button() != e) {
					if (_animation->update_tab(_get_data(*e)->data, dt)) {
						stopped = false;
					}
				}
			}
			_invalidate_children_layout();
			if (!stopped) {
				get_manager().get_scheduler().schedule_element_update(*this);
			}
		}

		/// Called when the user starts dragging a \ref tab_button in this panel, or when a \ref tab_button that's
		/// being dragged enters this panel.
		void _on_start_drag() {
			tab_manager &man = _get_tab_manager();
			_droptok = man.end_drag += [this]() {
				_on_end_drag();
			};
			_updatetok = man.drag_move_tab_button += [this](tab_drag_update_info &info) {
				_on_drag_update(info);
			};
		}
		/// Called when the user stops dragging a tab or when the tab is dragged away from this panel, to start
		/// animations and unregister handlers.
		void _on_end_drag() {
			get_manager().get_scheduler().schedule_element_update(*this);
			tab_manager &man = _get_tab_manager();
			man.end_drag -= _droptok;
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
			_animation->set_offset(_get_data(dragbtn)->data, curpos - accu);
			_invalidate_children_layout(); // invalid children layout no matter what
		}

		/// Initializes \ref _animation.
		///
		/// \todo Use customizable animation controller.
		void _initialize(std::u8string_view cls, const ui::element_configuration &config) override {
			stack_panel::_initialize(cls, config);

			_animation = std::make_shared<exponential_tab_button_animation_controller>();
		}
	};
}
