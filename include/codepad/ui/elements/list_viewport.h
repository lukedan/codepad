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
		vec2d get_virtual_panel_size() override;

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
		/// that the first children has been materialized.
		[[nodiscard]] std::tuple<double, double, double> _get_item_size() const;
		/// Creates and initializes all elements that should be visible. Desired size computation will always be done
		/// for the first element, but it's controlled by the \p compute_desired_size parameter for other added
		/// elements. The computatino may be necessary to obtain up-to-date layout when new elements are created
		/// during layout computation.
		void _initialize_visible_elements(vec2d available_size, bool compute_desired_size = false);

		/// Computes the desired size for elements.
		vec2d _compute_desired_size_impl(vec2d) override;
		/// Updates children layout. Calls \ref _initialize_visible_elements() before computing layout so that all
		/// elements that should be visible are taken into account.
		void _on_update_children_layout() override;

		/// Handles the \p orientation and \p item_class properties.
		property_info _find_property_path(const property_path::component_list&) const override;
	};
}
