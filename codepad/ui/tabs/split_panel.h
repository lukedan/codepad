// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the split panel.

#include "../element.h"
#include "../panel.h"
#include "../manager.h"

namespace codepad::ui::tabs {
	/// A panel with two regions separated by a draggable separator.
	class split_panel : public ui::panel {
	public:
		constexpr static double minimum_panel_size = 30.0; ///< The minimum size that a region can have.

		/// Sets the child that will be placed above or to the left of the separator.
		void set_child1(ui::element *elem) {
			_change_child(_c1, elem);
		}
		/// Returns the child that's currently above or to the left of the separator.
		ui::element *get_child1() const {
			return _c1;
		}
		/// Sets the child that will be placed below or to the right of the separator.
		void set_child2(ui::element *elem) {
			_change_child(_c2, elem);
		}
		/// Returns the child that's currently below or to the right of the separator.
		ui::element *get_child2() const {
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
					sp->_maintain_separator_position<false>(totw, oldpos, get_separator_position());
				}
				sp = dynamic_cast<split_panel*>(_c2);
				if (sp && sp->get_orientation() == get_orientation()) {
					sp->_maintain_separator_position<true>(totw, oldpos, get_separator_position());
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

		/// Returns the default class of all elements of type \ref split_panel.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("split_panel");
		}

		/// Returns the role identifier of the separator.
		inline static str_view_t get_separator_role() {
			return CP_STRLIT("separator");
		}
	protected:
		ui::element
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
		/// \tparam MinChanged \p true if the position of the left/top boundary has changed, and \p false if that of
		///                    the right/bottom boundary has changed.
		/// \param ptotw The total width of the parent, not including that of its separator. This value is obtained
		///             before the layouts are updated.
		/// \param poldv The original position of the parent's separator.
		/// \param pnewv The updated position of the parent's separator.
		template <bool MinChanged> void _maintain_separator_position(double ptotw, double poldv, double pnewv) {
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
			if constexpr (MinChanged) {
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
			auto *sp = dynamic_cast<split_panel*>(MinChanged ? _c1 : _c2);
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
				if constexpr (MinChanged) {
					sp->_maintain_separator_position<true>(neww, ptotw * poldv / neww, ptotw * pnewv / neww);
				} else {
					sp->_maintain_separator_position<false>(
						neww, omytotw * (1.0 - oldpos) / neww, nmytotw * (1.0 - newpos) / neww
						);
				}
			}
			// update position
			_maintainpos = true;
			set_separator_position(newpos);
			_maintainpos = false;
		}

		/// Changes the given child to the specified value.
		void _change_child(ui::element * &e, ui::element * newv) {
			if (e) {
				ui::element *old = e; // because e may be changed in _on_child_removed
				_children.remove(*old);
			}
			e = newv;
			if (e) {
				_child_set_logical_parent(*e, this);
				_children.add(*e);
			}
		}
		/// Sets the corresponding pointer to \p nullptr.
		void _on_child_removed(element & e) override {
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
			// TODO notify _sep of this change
			_invalidate_children_layout();
		}

		/// Renders all children with additional clip regions.
		void _custom_render() const override {
			panel::_custom_render();
			_child_on_render(*_sep);
			ui::renderer_base &r = get_manager().get_renderer();
			/*if (_c1) {
				r.push_clip(get_region1().fit_grid_enlarge<int>());
				_child_on_render(*_c1);
				r.pop_clip();
			}
			if (_c2) {
				r.push_clip(get_region2().fit_grid_enlarge<int>());
				_child_on_render(*_c2);
				r.pop_clip();
			}*/
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

		/// Initializes \ref _sep and adds handlers for certain events.
		void _initialize(str_view_t cls, const element_configuration & config) override {
			ui::panel::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(*this, {
				{get_separator_role(), _role_cast(_sep)}
				});

			_sep->mouse_down += [this](ui::mouse_button_info & p) {
				if (p.button == ui::mouse_button::primary) {
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
			_sep->mouse_up += [this](ui::mouse_button_info & p) {
				if (_sep_dragging && p.button == ui::mouse_button::primary) {
					_sep_dragging = false;
					get_window()->release_mouse_capture();
				}
			};
			_sep->mouse_move += [this](ui::mouse_move_info & p) {
				if (_sep_dragging) {
					rectd client = get_client_region();
					double position =
						get_orientation() == orientation::vertical ? // TODO FIXME
						(p.new_position.get(*_sep).y - _sep_offset) /
						(client.height() - _sep->get_layout().height()) :
						(p.new_position.get(*_sep).x - _sep_offset) /
						(client.width() - _sep->get_layout().width());
					set_separator_position(position);
				}
			};
		}
	};
}
