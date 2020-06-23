// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of stack panels.

#include "../panel.h"

namespace codepad::ui {
	/// A panel that arranges all children sequentially in a given orientation.
	class stack_panel : public panel {
	public:
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

		/// Returns the minimum width that can contain all elements in pixels, plus padding. More specifically, the
		/// return value is the padding plus the sum of all horizontal sizes specified in pixels, ignoring those
		/// specified as proportions, if the panel is in a horizontal state; or the padding plus the maximum
		/// horizontal size specified in pixels otherwise. If all values are in proportions, returns 1.0 in
		/// proportion.
		size_allocation get_desired_width() const override;
		/// Similar to \ref get_desired_width(), but for height.
		size_allocation get_desired_height() const override;

		// TODO change to using orientation instead of bools
		/// Calculates the layout of a list of elements as if they were in a \ref stack_panel with the given
		/// orientation and client area. All elements must be children of the given \ref panel.
		template <bool Vertical> inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems
		) {
			if constexpr (Vertical) {
				_layout_elements_in_impl<
					true, &panel::layout_child_horizontal,
					&rectd::ymin, &rectd::ymax, &rectd::xmin, &rectd::xmax
				>(client, elems);
			} else {
				_layout_elements_in_impl<
					false, &panel::layout_child_vertical,
					&rectd::xmin, &rectd::xmax, &rectd::ymin, &rectd::ymax
				>(client, elems);
			}
		}
		/// Calls the corresponding templated version of \ref layout_elements_in.
		inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems, bool vertical
		) {
			if (vertical) {
				layout_elements_in<true>(client, elems);
			} else {
				layout_elements_in<false>(client, elems);
			}
		}

		/// Returns \ref get_properties_static().
		const property_mapping &get_properties() const override;

		/// Adds the additional orientation property.
		[[nodiscard]] static const property_mapping &get_properties_static();
		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"stack_panel";
		}
	private:
		/// The orientation used when calculating the children's layout.
		orientation _orientation = orientation::horizontal;
	protected:
		/// Contains information (size, whether size is a proportion) used for layout calculation.
		struct _elem_layout_info {
			/// Default constructor.
			_elem_layout_info() = default;

			size_allocation
				margin_min, ///< The element's layout settings for its margin in the `negative' direction.
				size, ///< The element's layout settings for its size.
				margin_max; ///< The element's layout settings for the margin in the `positive' direction.

			/// Extracts the information corresponding to the given orientation from the given element.
			template <bool Vertical> inline static _elem_layout_info extract(const element &e) {
				_elem_layout_info res;
				thickness margin = e.get_margin();
				anchor anc = e.get_anchor();
				if constexpr (Vertical) {
					res.margin_min = size_allocation(margin.top, (anc & anchor::top) != anchor::none);
					res.margin_max = size_allocation(margin.bottom, (anc & anchor::bottom) != anchor::none);
					res.size = e.get_layout_height();
				} else {
					res.margin_min = size_allocation(margin.left, (anc & anchor::left) != anchor::none);
					res.margin_max = size_allocation(margin.right, (anc & anchor::right) != anchor::none);
					res.size = e.get_layout_width();
				}
				return res;
			}
		};
		/// Implementation of \ref layout_elements_in for a certain orientation.
		///
		/// \tparam Vertical Whether the elements should be laid out vertically.
		/// \tparam CalcDefaultDir Member pointer to the function used to calculate layout on the orientation
		///                        that is irrelevant to the stack panel.
		/// \tparam MainMin Pointer to the member that holds the minimum position of the client area of the
		///                 concerned orientation.
		/// \tparam MainMax Pointer to the member that holds the maximum position of the client area of the
		///                 concerned orientation.
		/// \tparam DefMin Pointer to the member that holds the minimum position of the client area of the
		///                irrelevant orientation.
		/// \tparam DefMax Pointer to the member that holds the maximum position of the client area of the
		///                irrelevant orientation.
		template <
			bool Vertical,
			void(*CalcDefaultDir)(element&, double, double),
			double rectd::*MainMin, double rectd::*MainMax,
			double rectd::*DefMin, double rectd::*DefMax
		> inline static void _layout_elements_in_impl(rectd client, const std::vector<element*> &elems) {
			std::vector<_elem_layout_info> layoutinfo;
			double total_prop = 0.0, total_px = 0.0;
			for (element *e : elems) {
				if (e->is_visible(visibility::layout)) {
					CalcDefaultDir(*e, client.*DefMin, client.*DefMax);
					_elem_layout_info info = _elem_layout_info::extract<Vertical>(*e);
					(info.margin_min.is_pixels ? total_px : total_prop) += info.margin_min.value;
					(info.size.is_pixels ? total_px : total_prop) += info.size.value;
					(info.margin_max.is_pixels ? total_px : total_prop) += info.margin_max.value;
					layoutinfo.emplace_back(info);
				} else { // not accounted for; behave as panel
					panel::layout_child(*e, client);
				}
			}
			// distribute the remaining space
			double prop_mult = (client.*MainMax - client.*MainMin - total_px) / total_prop, pos = client.*MainMin;
			auto it = layoutinfo.begin();
			for (element *e : elems) {
				if (e->is_visible(visibility::layout)) {
					const _elem_layout_info &info = *it;
					rectd nl = e->get_layout();
					nl.*MainMin = pos +=
						info.margin_min.is_pixels ?
						info.margin_min.value :
						info.margin_min.value * prop_mult;
					nl.*MainMax = pos +=
						info.size.is_pixels ?
						info.size.value :
						info.size.value * prop_mult;
					pos +=
						info.margin_max.is_pixels ?
						info.margin_max.value :
						info.margin_max.value * prop_mult;
					_child_set_layout(*e, nl);
					++it;
				}
			}
		}

		/// Calls \ref layout_elements_in to calculate the layout of all children.
		void _on_update_children_layout() override {
			layout_elements_in(
				get_client_region(),
				std::vector<element*>(_children.items().begin(), _children.items().end()),
				get_orientation() == orientation::vertical
			);
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
			bool vertical = get_orientation() == orientation::vertical;
			_on_desired_size_changed(!vertical, vertical);
			_invalidate_children_layout();
			panel::_on_child_order_changed(elem, before);
		}

		/// Called after the orientation of this element has been changed. Invalidates the layout of affected
		/// elements.
		virtual void _on_orientation_changed() {
			_on_desired_size_changed(true, true);
			_invalidate_children_layout();
		}
	};
}
