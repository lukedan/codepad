// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the split panel.

#include "../../element.h"
#include "../../panel.h"
#include "../../manager.h"
#include "../stack_panel.h"

namespace codepad::ui::tabs {
	/// A panel with two regions separated by a draggable separator.
	class split_panel : public panel {
	public:
		constexpr static double minimum_panel_size = 30.0; ///< The minimum size that a region can have.

		/// Sets the child that will be placed above or to the left of the separator.
		void set_child1(element *elem) {
			_change_child(_c1, elem);
		}
		/// Returns the child that's currently above or to the left of the separator.
		element *get_child1() const {
			return _c1;
		}
		/// Sets the child that will be placed below or to the right of the separator.
		void set_child2(element *elem) {
			_change_child(_c2, elem);
		}
		/// Returns the child that's currently below or to the right of the separator.
		element *get_child2() const {
			return _c2;
		}

		/// Returns the boundaries of the left/top region.
		rectd get_region1() const {
			rectd cln = get_client_region();
			return
				get_orientation() == orientation::vertical ?
				rectd(cln.xmin, cln.xmax, cln.ymin, _sep->get_layout().ymin) :
				rectd(cln.xmin, _sep->get_layout().xmin, cln.ymin, cln.ymax);
		}
		/// Returns the boundaries of the bottom/right region.
		rectd get_region2() const {
			rectd cln = get_client_region();
			return
				get_orientation() == orientation::vertical ?
				rectd(cln.xmin, cln.xmax, _sep->get_layout().ymax, cln.ymax) :
				rectd(_sep->get_layout().xmax, cln.xmax, cln.ymin, cln.ymax);
		}

		/// Returns the position of the separator, a number in the range of [0, 1].
		double get_separator_position() const {
			return _sep_position;
		}
		/// Sets the position of the separator.
		void set_separator_position(double pos) {
			double oldpos = _sep_position;
			_sep_position = std::clamp(pos, 0.0, 1.0);
			if (!_maintainpos) {
				double totw =
					get_orientation() == orientation::vertical ?
					get_client_region().height() - _sep->get_layout().height() :
					get_client_region().width() - _sep->get_layout().width();
				auto *sp = dynamic_cast<split_panel*>(_c1);
				if (sp && sp->get_orientation() == get_orientation()) {
					sp->_maintain_separator_position(totw, oldpos, get_separator_position(), false);
				}
				sp = dynamic_cast<split_panel*>(_c2);
				if (sp && sp->get_orientation() == get_orientation()) {
					sp->_maintain_separator_position(totw, oldpos, get_separator_position(), true);
				}
			}
			_invalidate_children_layout();
		}

		/// Returns the current orientation.
		orientation get_orientation() const {
			return _orientation;
		}
		/// Sets the current orientation.
		void set_orientation(orientation o) {
			if (_orientation != o) {
				_orientation = o;
				_on_orientation_changed();
			}
		}

		info_event<> orientation_changed; ///< Invoked whenever the orientation has been changed.

		/// Returns the default class of all elements of type \ref split_panel.
		inline static std::u8string_view get_default_class() {
			return u8"split_panel";
		}

		/// Returns the name identifier of the separator.
		inline static std::u8string_view get_separator_name() {
			return u8"separator";
		}
	protected:
		element
			/// The first child, displayed above or to the left of the separator.
			*_c1 = nullptr,
			/// The second child, displayed below or to the right of the separator.
			*_c2 = nullptr,
			/// The draggable separator.
			*_sep = nullptr;
		double
			/// The position of \ref _sep in this panel. This value should be between 0 and 1.
			_sep_position = 0.5,
			/// The offset to the mouse when the user drags the separator.
			_sep_offset = 0.0;
		/// The orientation in which the two children are laid out.
		orientation _orientation = orientation::horizontal;
		bool
			/// Indicates when the position of \ref _sep is being set in \ref _maintain_separator_position to avoid
			/// infinite recursion.
			_maintainpos = false,
			/// Whether the user's dragging the separator.
			_sep_dragging = false;

		/// When this element is itself a child of a \ref split_panel with the same orientation, and the separator's
		/// position of the parent has changed, this function is called to make the element behave as if it is
		/// independent of its parent, i.e., to keep the global position of the draggable separator uncahnged.
		///
		/// \param ptotw The total width of the parent, not including that of its separator. This value is obtained
		///             before the layouts are updated.
		/// \param poldv The original position of the parent's separator.
		/// \param pnewv The updated position of the parent's separator.
		/// \param min_changed \p true if the position of the left/top boundary has changed, and \p false if that of
		///                    the right/bottom boundary has changed.
		void _maintain_separator_position(double ptotw, double poldv, double pnewv, bool min_changed) {
			vec2d sepsz = _sep->get_layout().size();
			double newpos, oldpos = get_separator_position(), padding =
				get_orientation() == orientation::vertical ?
				sepsz.y + get_padding().height() :
				sepsz.x + get_padding().width();
			double
				// the total width of the two regions, before and after the change
				omytotw, nmytotw,
				// the width of this region whose width won't change
				myfixw;
			// recalculate separator position
			if (min_changed) {
				// (ptotw * (1 - poldv) - padding) * (1 - oldmv) = (ptotw * (1 - pnewv) - padding) * (1 - newmv)
				omytotw = ptotw * (1.0 - poldv) - padding;
				nmytotw = ptotw * (1.0 - pnewv) - padding;
				myfixw = omytotw * (1.0 - oldpos);
				newpos = 1.0 - myfixw / nmytotw;
			} else {
				// (ptotw * poldv - padding) * oldmv = (ptotw * pnewv - padding) * newmv
				omytotw = ptotw * poldv - padding;
				nmytotw = ptotw * pnewv - padding;
				myfixw = omytotw * oldpos;
				newpos = myfixw / nmytotw;
			}
			// the possibly affected child
			auto *sp = dynamic_cast<split_panel*>(min_changed ? _c1 : _c2);
			if (sp && sp->get_orientation() == get_orientation()) {
				// must also be a split_panel with the same orientation
				// here we transform the positions so that it's as if this split_panel doesn't exist
				// for example, if MinChanged is true, we transform the positions from
				//   +------+--------------+
				//   |      |##############|
				//   |      |#+--+--+#    #|
				//   |      |#|  |  |#    #|
				//   |      |#+--+--+#    #|
				//   |      |##############|
				//   +------+--------------+,
				//   |----- ^ -------------|
				// where the frame marked by # is this element, to as if
				//   +------+--------+
				//   |      | +--+--+|
				//   |      | |  |  ||
				//   |      | +--+--+|
				//   +------+--------+,
				//   |----- ^ -------|
				// note how the space on the right is removed
				double neww = ptotw - padding - myfixw; // the width of the transformed region to be adjusted
				if (min_changed) {
					sp->_maintain_separator_position(neww, ptotw * poldv / neww, ptotw * pnewv / neww, true);
				} else {
					sp->_maintain_separator_position(
						neww, omytotw * (1.0 - oldpos) / neww, nmytotw * (1.0 - newpos) / neww, false
					);
				}
			}
			// update position
			_maintainpos = true;
			set_separator_position(newpos);
			_maintainpos = false;
		}

		/// Changes the given child to the specified value.
		void _change_child(element *&e, element *newv) {
			if (e) {
				element *old = e; // because e may be changed in _on_child_removed
				_children.remove(*old);
			}
			e = newv;
			if (e) {
				_child_set_logical_parent(*e, this);
				_children.add(*e);
			}
		}
		/// Sets the corresponding pointer to \p nullptr.
		void _on_child_removed(element &e) override {
			if (&e == _c1 || &e == _c2) {
				if (&e == _c1) {
					_c1 = nullptr;
				} else {
					_c2 = nullptr;
				}
			}
			panel::_on_child_removed(e);
		}

		/// Called after the current orientation has been changed. Calls \ref invalidate_layout, and notifies
		/// \ref _sep of this change.
		virtual void _on_orientation_changed() {
			_invalidate_children_layout();
			orientation_changed.invoke();
		}

		/// Updates the layout of all children.
		void _on_update_children_layout() override {
			rectd client = get_client_region();
			if (get_orientation() == orientation::vertical) {
				panel::layout_child_horizontal(*_sep, client.xmin, client.xmax);
				auto metrics = _sep->get_layout_height();
				double top = (client.height() - metrics.value) * _sep_position + client.ymin;
				_child_set_vertical_layout(*_sep, top, top + metrics.value);
			} else {
				panel::layout_child_vertical(*_sep, client.ymin, client.ymax);
				auto metrics = _sep->get_layout_width();
				double left = (client.width() - metrics.value) * _sep_position + client.xmin;
				_child_set_horizontal_layout(*_sep, left, left + metrics.value);
			}
			if (_c1) {
				panel::layout_child(*_c1, get_region1());
			}
			if (_c2) {
				panel::layout_child(*_c2, get_region2());
			}
		}

		/// Handles the \p set_horizontal and \p set_vertical events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_orientation_events(
					name, orientation_changed, [this]() {
						return get_orientation();
					}, callback
				) ||
				panel::_register_event(name, std::move(callback));
		}

		/// Adds \ref _sep to the mapping.
		class_arrangements::notify_mapping _get_child_notify_mapping() override {
			auto mapping = panel::_get_child_notify_mapping();
			mapping.emplace(get_separator_name(), _name_cast(_sep));
			return mapping;
		}

		/// Initializes \ref _sep and adds handlers for certain events.
		void _initialize(std::u8string_view cls) override {
			panel::_initialize(cls);

			_sep->mouse_down += [this](mouse_button_info &p) {
				if (p.button == mouse_button::primary) {
					_sep_dragging = true;
					_sep_offset =
						get_orientation() == orientation::vertical ?
						p.position.get(*_sep).y :
						p.position.get(*_sep).x;
					get_window()->set_mouse_capture(*_sep);
				}
			};
			_sep->lost_capture += [this]() {
				_sep_dragging = false;
			};
			_sep->mouse_up += [this](mouse_button_info &p) {
				if (_sep_dragging && p.button == mouse_button::primary) {
					_sep_dragging = false;
					get_window()->release_mouse_capture();
				}
			};
			_sep->mouse_move += [this](mouse_move_info &p) {
				if (_sep_dragging) {
					rectd client = get_client_region();
					double position =
						get_orientation() == orientation::vertical ? // TODO what is this todo for?
						(p.new_position.get(*this).y - _sep_offset) /
						(client.height() - _sep->get_layout().height()) :
						(p.new_position.get(*this).x - _sep_offset) /
						(client.width() - _sep->get_layout().width());
					set_separator_position(position);
				}
			};
		}
	};
}
