// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of stack panels.

#include "codepad/ui/panel.h"

namespace codepad::ui {
	/// A panel that arranges all children sequentially in a given orientation.
	class stack_panel : public panel {
	public:
		/// Utility struct used for layout computation. The user should first use \ref accumulate() to accumulate
		/// data of all children, then use \ref compute_layout_for() to compute the layout of a child based on the
		/// accumulated data of the entire set of elements.
		struct stack_layout_helper {
		public:
			/// Default constructor.
			stack_layout_helper() {
			}
			/// Initializes this struct using the size, position, and orientation of the panel.
			stack_layout_helper(double min, double size, orientation ori) : _space(size), _offset(min), _orient(ori) {
			}

			/// Accumulates data for the given child.
			void accumulate(element &child) {
				double pixel_size = 0.0;
				switch (_orient) {
				case orientation::horizontal:
					child.get_margin_left().accumulate_to(pixel_size, _total_proportion);
					child.get_layout_width().accumulate_to(pixel_size, _total_proportion);
					child.get_margin_right().accumulate_to(pixel_size, _total_proportion);
					break;
				case orientation::vertical:
					child.get_margin_top().accumulate_to(pixel_size, _total_proportion);
					child.get_layout_height().accumulate_to(pixel_size, _total_proportion);
					child.get_margin_bottom().accumulate_to(pixel_size, _total_proportion);
					break;
				}
				_space -= pixel_size;
			}
			/// Returns the layout of the given child on the previously specified orientation. This function must be
			/// called in-order for all children.
			[[nodiscard]] std::pair<double, double> compute_and_accumulate_layout_for(element &child) {
				auto [before, size, after] = compute_detailed_span_for(child);
				double min = _offset + before;
				double max = min + size;
				_offset = max + after;
				return { min, max };
			}
			/// Computes the margin before, after, and the size of the given element.
			///
			/// \return The margin before the element, size of the element, and the margin after the element.
			[[nodiscard]] std::tuple<double, double, double> compute_detailed_span_for(element &child) const {
				size_allocation before, size, after;
				switch (_orient) {
				case orientation::horizontal:
					before = child.get_margin_left();
					size = child.get_layout_width();
					after = child.get_margin_right();
					break;
				case orientation::vertical:
					before = child.get_margin_top();
					size = child.get_layout_height();
					after = child.get_margin_bottom();
					break;
				}
				return {
					before.is_pixels ? before.value : (before.value * _space / _total_proportion),
					size.is_pixels ? size.value : (size.value * _space / _total_proportion),
					after.is_pixels ? after.value : (after.value * _space / _total_proportion)
				};
			}
			/// Computes the total span for this child, including its margins.
			[[nodiscard]] double compute_span_for(element &child) const {
				auto [before, size, after] = compute_detailed_span_for(child);
				return before + size + after;
			}
		protected:
			double _total_proportion = 0.0; ///< Total proportion.
			/// Total remaining space for all elements that should be distributed among proportion sizes.
			double _space = 0.0;
			double _offset = 0.0; ///< Total offset for all elements processed so far by \ref compute_layout_for().
			orientation _orient = orientation::horizontal; ///< Orientation of this panel.
		};


		/// Returns the current orientation.
		orientation get_orientation() const {
			return _orientation;
		}
		/// Sets the current orientation.
		void set_orientation(orientation o) {
			if (o != _orientation) {
				_orientation = o;
				_on_orientation_changed();
			}
		}

		/// Calculates the layout of a list of elements as if they were in a \ref stack_panel with the given
		/// orientation and client area. All elements must be children of the given \ref panel.
		inline static void layout_elements_in(
			rectd client, const std::list<element*> &elems, orientation orient
		) {
			_layout_elements_in_impl(client, orient, elems);
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"stack_panel";
		}
	private:
		/// The orientation used when calculating the children's layout.
		orientation _orientation = orientation::horizontal;
	protected:
		/// Implementation of \ref layout_elements_in().
		inline static void _layout_elements_in_impl(
			rectd client, orientation orient, const std::list<element*> &elems
		) {
			double offset = 0.0;
			double size = 0.0;
			if (orient == orientation::horizontal) {
				offset = client.xmin;
				size = client.width();
			} else {
				offset = client.ymin;
				size = client.height();
			}
			stack_layout_helper stack(offset, size, orient);
			// accumulate data
			for (element *e : elems) {
				if (e->is_visible(visibility::layout)) {
					stack.accumulate(*e);
					if (orient == orientation::horizontal) {
						panel::layout_child_vertical(*e, client.ymin, client.ymax);
					} else {
						panel::layout_child_horizontal(*e, client.xmin, client.xmax);
					}
				} else { // not accounted for; behave as panel
					panel::layout_child(*e, client);
				}
			}
			// compute layout
			for (element *e : elems) {
				if (e->is_visible(visibility::layout)) {
					auto [min, max] = stack.compute_and_accumulate_layout_for(*e);
					if (orient == orientation::horizontal) {
						_child_set_horizontal_layout(*e, min, max);
					} else {
						_child_set_vertical_layout(*e, min, max);
					}
				}
			}
		}

		/// Implementation of \ref _compute_desired_size_impl() for a specific orientation.
		template <
			size_allocation (element::*StackMarginMin)() const, size_allocation (element::*StackMarginMax)() const,
			size_allocation_type (element::*StackSizeAlloc)() const, double vec2d::*StackSize,
			size_allocation (element::*IndepMarginMin)() const, size_allocation (element::*IndepMarginMax)() const,
			size_allocation_type (element::*IndepSizeAlloc)() const, double vec2d::*IndepSize
		> inline static vec2d _compute_stack_panel_desired_size(
			vec2d available, vec2d padding, const std::list<element*> &children
		) {
			available -= padding;
			std::size_t num_auto_children = 0;
			double total_pixels = 0.0, total_prop = 0.0;
			for (element *child : children) {
				if (child->is_visible(visibility::layout)) {
					(child->*StackMarginMin)().accumulate_to(total_pixels, total_prop);
					(child->*StackMarginMax)().accumulate_to(total_pixels, total_prop);
					switch ((child->*StackSizeAlloc)()) {
					case size_allocation_type::automatic:
						++num_auto_children;
						break;
					case size_allocation_type::fixed:
						total_pixels += child->get_layout_parameters().size.*StackSize;
						break;
					case size_allocation_type::proportion:
						total_prop += child->get_layout_parameters().size.*StackSize;
						break;
					}
				}
			}

			_basic_desired_size_accumulator<
				IndepMarginMin, IndepMarginMax, IndepSizeAlloc, IndepSize
			> indep_accum(available.*IndepSize);
			// first allocate space to all elements with automatic or pixel size allocation
			for (element *child : children) {
				if (child->is_visible(visibility::layout)) {
					double size;
					switch ((child->*StackSizeAlloc)()) {
					case size_allocation_type::proportion:
						continue; // ignore; handled later
					case size_allocation_type::fixed:
						size = child->get_layout_parameters().size.*StackSize;
						break;
					case size_allocation_type::automatic:
						size = std::max(0.0, available.*StackSize - total_pixels);
						break;
					}
					vec2d child_available;
					child_available.*StackSize = size;
					child_available.*IndepSize = indep_accum.get_available(*child);
					child->compute_desired_size(child_available);
					if ((child->*StackSizeAlloc)() == size_allocation_type::automatic) {
						total_pixels += child->get_desired_size().*StackSize;
					}
					indep_accum.accumulate(*child);
				}
			}
			// then distribute the remaining space among proportion values
			double ratio = std::max(available.*StackSize - total_pixels, 0.0) / total_prop;
			double max_proportion_size = 0.0;
			for (element *child : children) {
				if (child->is_visible(visibility::layout)) {
					if ((child->*StackSizeAlloc)() == size_allocation_type::proportion) {
						double size_ratio = child->get_layout_parameters().size.*StackSize;
						double size_pixel = size_ratio * ratio;
						vec2d child_available;
						child_available.*StackSize = size_pixel;
						child_available.*IndepSize = indep_accum.get_available(*child);
						child->compute_desired_size(child_available);
						max_proportion_size = std::max(
							max_proportion_size, child->get_desired_size().*StackSize * total_prop / size_ratio
						);
						indep_accum.accumulate(*child);
					}
				}
			}

			vec2d result;
			result.*StackSize = max_proportion_size + total_pixels;
			result.*IndepSize = indep_accum.maximum_size;
			return result + padding;
		}
		/// Computes the desired size of this panel based on the desired size of all children.
		vec2d _compute_desired_size_impl(vec2d available) const override {
			if (get_orientation() == orientation::horizontal) {
				return _compute_stack_panel_desired_size<
					&element::get_margin_left, &element::get_margin_right,
					&element::get_width_allocation, &vec2d::x,
					&element::get_margin_top, &element::get_margin_bottom,
					&element::get_height_allocation, &vec2d::y
				>(available, get_padding().size(), _children.items());
			} else {
				return _compute_stack_panel_desired_size<
					&element::get_margin_top, &element::get_margin_bottom,
					&element::get_height_allocation, &vec2d::y,
					&element::get_margin_left, &element::get_margin_right,
					&element::get_width_allocation, &vec2d::x
				>(available, get_padding().size(), _children.items());
			}
		}

		/// Calls \ref layout_elements_in to calculate the layout of all children.
		void _on_update_children_layout() override {
			layout_elements_in(get_client_region(), _children.items(), get_orientation());
		}

		/// Invalidates the children's layout as well.
		void _on_child_added(element &elem, element *before) override {
			_invalidate_children_layout();
			panel::_on_child_added(elem, before);
		}
		/// Invalidates the children's layout as well.
		void _on_child_removed(element &elem) override {
			_invalidate_children_layout();
			panel::_on_child_removed(elem);
		}
		/// Invalidates the children's layout since it is determined by their ordering.
		void _on_child_order_changed(element &elem, element *before) override {
			_on_desired_size_changed();
			_invalidate_children_layout();
			panel::_on_child_order_changed(elem, before);
		}

		/// Called after the orientation of this element has been changed. Invalidates the layout of affected
		/// elements.
		virtual void _on_orientation_changed() {
			_on_desired_size_changed();
			_invalidate_children_layout();
		}

		/// Handles the \p orientation property.
		property_info _find_property_path(const property_path::component_list&) const override;
	};
}
