// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Virtualized listboxes.

#include <deque>

#include "label.h"
#include "scroll_viewport.h"
#include "reference_container.h"

namespace codepad::ui {
	/// A virtualized viewport of a listbox. The children of this viewport should not be accessed directly; use a
	/// \ref item_source instead. Element sizes on the stacking orientation are determined by the first element.
	class virtual_list_viewport : public scroll_viewport {
	public:
		/// Source of \ref listbox items. This is a virtual class that needs to be implemented by users.
		class item_source {
			friend virtual_list_viewport;
		public:
			/// Default virtual destructor.
			virtual ~item_source() = default;

			/// Returns the number of items in this item source.
			[[nodiscard]] virtual std::size_t get_item_count() const = 0;
			/// Updates the given \ref reference_container with the item at the specified index.
			virtual void set_item(std::size_t, reference_container&) const = 0;
		protected:
			/// Derived classes should call this when the list of items has changed. Calls
			/// \ref virtual_list_viewport::_on_items_changed().
			void _on_items_changed() {
				if (_list) {
					_list->_on_items_changed();
				}
			}
		private:
			virtual_list_viewport *_list = nullptr; ///< The list associated with this item source.
		};
		/// An item source that simply stores a list of strings. Each item's \p text reference will be set to the
		/// corresponding string.
		class simple_text_item_source : public item_source {
		public:
			/// Returns the number of items in \ref items.
			std::size_t get_item_count() const override {
				return items.size();
			}
			/// Sets the \p text reference of the given container.
			void set_item(std::size_t i, reference_container &con) const override {
				if (auto text = con.get_reference<label>(u8"text")) {
					text->set_text(items[i]);
				}
			}

			/// This function should be called when the contents of \ref items has been changed.
			void on_items_changed() {
				_on_items_changed();
			}

			std::vector<std::u8string> items; ///< The list of items.
		};

		/// Returns the size of the virtual panel on the stacking orientation. The returned
		/// value does not include padding.
		[[nodiscard]] double get_stacking_virtual_panel_size() {
			_initialize_visible_elements(get_client_region().size());
			if (!_source || _children.items().empty()) {
				return 0.0;
			}
			auto [before, size, after] = _get_item_size();
			return (before + size + after) * _source->get_item_count();
		}
		/// Returns the width or height of the virtual panel, depending on the orientation of this list. The returned
		/// value does not include padding.
		vec2d get_virtual_panel_size() override {
			double stacking_size = get_stacking_virtual_panel_size();
			panel_desired_size_accumulator independent_accum(
				std::numeric_limits<double>::infinity(),
				get_orientation() == orientation::horizontal ? orientation::vertical : orientation::horizontal
			);
			for (element *child : _children.items()) {
				independent_accum.accumulate(*child);
			}
			if (get_orientation() == orientation::horizontal) {
				return vec2d(stacking_size, independent_accum.maximum_size);
			} else {
				return vec2d(independent_accum.maximum_size, stacking_size);
			}
		}

		/// Returns \ref _source.
		[[nodiscard]] item_source *get_source() const {
			return _source.get();
		}
		/// Replaces the item source with a new one.
		std::unique_ptr<item_source> replace_source(std::unique_ptr<item_source> src) {
			if (_source) {
				_source->_list = nullptr;
			}
			std::unique_ptr<item_source> result = std::exchange(_source, std::move(src));
			if (_source) {
				_source->_list = this;
			}
			_on_items_changed();
			return result;
		}

		/// Returns the element class used by items.
		[[nodiscard]] const std::u8string &get_item_class() const {
			return _item_class;
		}
		/// Sets \ref _item_class and calls \ref _reset_items() so that items can spawn with the new class.
		void set_item_class(std::u8string cls) {
			_item_class = std::move(cls);
			_reset_items();
		}

		/// Returns the orientation of this viewport.
		[[nodiscard]] orientation get_orientation() const {
			return _orient;
		}
		/// Sets the orientation of this panel.
		void set_orientation(orientation orient) {
			if (_orient != orient) {
				_orient = orient;
				_on_orientation_changed();
			}
		}

		/// Called when the items of this list view changes. This includes when the contents of the \ref item_source
		/// is changed, or when the source itself is replaced.
		info_event<> items_changed;

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"virtual_list_viewport";
		}
	protected:
		std::unique_ptr<item_source> _source; ///< Item source.
		std::u8string _item_class; ///< Element class of list items.
		orientation _orient = orientation::vertical; ///< The orientation of this panel.

		/// Index of the second element of \ref _children in \ref _source - the first element is always going to be
		/// the element at index 0.
		std::size_t _second_element_index = 0;

		/// Clears all materialized items and invalidates the layout and desired size of this list view.
		void _reset_items() {
			for (element *e : _children.items()) {
				get_manager().get_scheduler().mark_for_disposal(*e);
			}
			_children.clear();
			_on_desired_size_changed();
			invalidate_layout();
			_on_virtual_panel_size_changed();
		}
		/// Called when the orientation of this panel has been changed.
		virtual void _on_orientation_changed() {
			_on_desired_size_changed();
			_invalidate_children_layout();
		}
		/// Called when the items of this list has changed.
		virtual void _on_items_changed() {
			_reset_items();
			items_changed.invoke();
		}

		/// Creates a new \ref reference_container corresponding to the specified index. The caller is responsible of
		/// making sure the index is in bounds.
		[[nodiscard]] reference_container &_create_item(std::size_t index) const {
			auto *elem = dynamic_cast<reference_container*>(
				get_manager().create_element(reference_container::get_default_class(), _item_class)
			);
			_source->set_item(index, *elem);
			return *elem;
		}
		/// Returns the size and margin of the first element on the stacking orientation. The caller must make sure
		/// the first children has been materialized.
		[[nodiscard]] std::tuple<double, double, double> _get_item_size() const {
			size_allocation before, size, after;
			auto *first = _children.items().front();
			if (get_orientation() == orientation::horizontal) {
				before = first->get_margin_left();
				size = first->get_layout_width();
				after = first->get_margin_right();
			} else {
				before = first->get_margin_top();
				size = first->get_layout_height();
				after = first->get_margin_bottom();
			}
			return {
				before.is_pixels ? before.value : 0.0,
				size.is_pixels ? size.value : 0.0,
				after.is_pixels ? after.value : 0.0
			};
		}
		/// Creates and initializes all elements that should be visible. Desired size computation will always be done
		/// for the first element, but it's controlled by the \p compute_desired_size parameter for other added
		/// elements. The computatino may be necessary to obtain up-to-date layout when new elements are created
		/// during layout computation.
		void _initialize_visible_elements(vec2d available_size, bool compute_desired_size = false) {
			if (!_source || _source->get_item_count() == 0) {
				return;
			}

			panel_desired_size_accumulator accum;
			if (get_orientation() == orientation::horizontal) {
				available_size.x = std::numeric_limits<double>::max();
				accum.available_size = available_size.y;
				accum.orient = orientation::vertical;
			} else {
				available_size.y = std::numeric_limits<double>::max();
				accum.available_size = available_size.x;
				accum.orient = orientation::horizontal;
			}
			// calls element::compute_desired_size using the appropriate available size
			auto do_compute_desired_size = [&](element &e) {
				double independent_size = accum.get_available(e);
				vec2d elem_available = available_size;
				if (get_orientation() == orientation::horizontal) {
					elem_available.y = independent_size;
				} else {
					elem_available.x = independent_size;
				}
				e.compute_desired_size(elem_available);
			};

			if (_children.items().empty()) {
				auto &first_item = _create_item(0);
				_children.add(first_item);
				do_compute_desired_size(first_item);
			}

			// compute visible item indices
			auto [before, size, after] = _get_item_size();
			double start = 0.0, end = 0.0;
			double item_size = before + size + after;
			if (get_orientation() == orientation::horizontal) {
				start = get_scroll_offset().x - get_padding().left;
				end = start + get_layout().width();
			} else {
				start = get_scroll_offset().y - get_padding().top;
				end = start + get_layout().height();
			}
			auto first_item = static_cast<std::size_t>(std::max(start / item_size, 1.0));
			auto past_last_item = std::min(
				static_cast<std::size_t>(std::max(end / item_size, 0.0)) + 1, _source->get_item_count()
			);
			std::size_t past_last_existing = _second_element_index + _children.items().size() - 1;

			// remove elements that are out of view
			while (_second_element_index < first_item && _children.items().size() > 1) { // before
				element &elem = **++_children.items().begin();
				_children.remove(elem);
				get_manager().get_scheduler().mark_for_disposal(elem);
				++_second_element_index;
			}
			while (past_last_existing > _second_element_index && past_last_existing > past_last_item) {
				element &elem = **--_children.items().end();
				_children.remove(elem);
				get_manager().get_scheduler().mark_for_disposal(elem);
				--past_last_existing;
			}
			if (_children.items().size() == 1) {
				// we've removed all elements other than the first one; we should reset the range of existing
				// elements to the visible range, or the next steps may create a lot of unnecessary elements
				_second_element_index = past_last_existing = first_item;
			}

			// create new elements
			element *insert_before = nullptr;
			if (_children.items().size() > 1) {
				insert_before = *++_children.items().begin();
			}
			while (_second_element_index > first_item) { // before
				--_second_element_index;
				auto &new_elem = _create_item(_second_element_index);
				_children.insert_before(insert_before, new_elem);
				if (compute_desired_size) {
					do_compute_desired_size(new_elem);
				}
				insert_before = &new_elem;
			}
			while (past_last_existing < past_last_item) { // after
				auto &new_elem = _create_item(past_last_existing);
				_children.insert_before(nullptr, new_elem);
				if (compute_desired_size) {
					do_compute_desired_size(new_elem);
				}
				++past_last_existing;
			}
		}

		/// Computes the desired size for elements.
		vec2d _compute_desired_size_impl(vec2d available) override {
			available -= get_padding().size();

			_initialize_visible_elements(available);

			panel_desired_size_accumulator accum;
			if (get_orientation() == orientation::horizontal) {
				available.x = std::numeric_limits<double>::max();
				accum.available_size = available.y;
				accum.orient = orientation::vertical;
			} else {
				available.y = std::numeric_limits<double>::max();
				accum.available_size = available.x;
				accum.orient = orientation::horizontal;
			}
			
			for (element *e : _children.items()) {
				double independent_size = accum.get_available(*e);
				vec2d elem_available = available;
				if (get_orientation() == orientation::horizontal) {
					elem_available.y = independent_size;
				} else {
					elem_available.x = independent_size;
				}
				e->compute_desired_size(elem_available);
				accum.accumulate(*e);
			}

			vec2d result;
			if (get_orientation() == orientation::horizontal) {
				result.x = get_stacking_virtual_panel_size();
				result.y = accum.maximum_size;
			} else {
				result.x = accum.maximum_size;
				result.y = get_stacking_virtual_panel_size();
			}
			return result + get_padding().size();
		}

		/// Updates children layout.
		void _on_update_children_layout() override {
			rectd client = get_client_region();
			// here we need to compute new desired size so that we have layout size data for the following layout
			// computation
			_initialize_visible_elements(client.size(), true);
			if (_children.items().empty()) {
				return;
			}

			auto [before, size, after] = _get_item_size();
			double span = before + size + after;
			// layout first element
			auto it = _children.items().begin();
			double virtual_panel_start = 0.0;
			if (get_orientation() == orientation::horizontal) {
				virtual_panel_start = get_layout().xmin + before - get_scroll_offset().x;
				_child_set_horizontal_layout(**it, virtual_panel_start, virtual_panel_start + size);
				panel::layout_child_vertical(**it, client.ymin, client.ymax);
			} else {
				virtual_panel_start = get_layout().ymin + before - get_scroll_offset().y;
				_child_set_vertical_layout(**it, virtual_panel_start, virtual_panel_start + size);
				panel::layout_child_horizontal(**it, client.xmin, client.xmax);
			}
			// layout other elements
			std::size_t index = _second_element_index;
			for (++it; it != _children.items().end(); ++it, ++index) {
				double min_pos = virtual_panel_start + span * index;
				if (get_orientation() == orientation::horizontal) {
					_child_set_horizontal_layout(**it, min_pos, min_pos + size);
					panel::layout_child_vertical(**it, client.ymin, client.ymax);
				} else {
					_child_set_vertical_layout(**it, min_pos, min_pos + size);
					panel::layout_child_horizontal(**it, client.xmin, client.xmax);
				}
			}
		}


		/// Handles the \p orientation and \p item_class properties.
		property_info _find_property_path(const property_path::component_list &path) const override {
			if (path.front().is_type_or_empty(u8"virtual_list_viewport")) {
				if (path.front().property == u8"orientation") {
					return property_info::make_getter_setter_property_info<
						virtual_list_viewport, orientation, element
					>(
						[](const virtual_list_viewport &e) {
							return e.get_orientation();
						},
						[](virtual_list_viewport &e, orientation val) {
							e.set_orientation(val);
						},
						u8"virtual_list_viewport.orientation"
					);
				}
				if (path.front().property == u8"item_class") {
					return property_info::make_getter_setter_property_info<
						virtual_list_viewport, std::u8string, element
					>(
						[](const virtual_list_viewport &e) {
							return e.get_item_class();
						},
						[](virtual_list_viewport &e, std::u8string val) {
							e.set_item_class(val);
						},
						u8"virtual_list_viewport.item_class"
					);
				}
			}
			return scroll_viewport::_find_property_path(path);
		}
	};
}
