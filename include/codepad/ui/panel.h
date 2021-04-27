// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Panels used to contain and organize elements.

#include <chrono>
#include <unordered_map>

#include "element.h"
#include "renderer.h"

namespace codepad::ui {
	class manager;
	class panel;

	/// Stores and manages a collection of elements.
	class element_collection {
	public:
		/// Contains information about changes in an \ref element_collection.
		struct change_info {
			/// Specifies the type of the change.
			enum class type {
				add, ///< Addition of an element.
				remove, ///< Removal of an element.
				set_zindex, ///< Change of an element's z-index.
				set_order ///< Change of an element's order in the container.
			};
			/// Initializes all fields of this struct.
			change_info(type t, element &c, element *ex) : change_type(t), subject(c), before(ex) {
			}

			const type change_type; ///< The type of the change.
			element &subject; ///< The \ref element that's involved.
			/// Context sensitive field that specifies the first succeeding element after this operation.
			element *const before = nullptr;
		};

		/// Constructs the collection with the given parent.
		explicit element_collection(panel &b) : _f(b) {
		}
		/// Destructor. Checks to ensure that the collection has been cleared.
		~element_collection();

		/// Adds an element to the end of the collection by calling \ref insert_before().
		void add(element &elem) {
			insert_before(nullptr, elem);
		}
		/// Inserts \p target before \p before. If \p before is \p nullptr, \p target is placed at the back of the
		/// collection. The element is added using its original z-index.
		void insert_before(element *before, element &target);
		/// Removes an element from the collection, and sets its \ref element::_logical_parent to \p nullptr. The
		/// element must be a member of this collection.
		void remove(element&);
		/// Removes all elements from the collection.
		void clear();
		/// Sets the z-index of an element. The element must be a member of this collection.
		void set_zindex(element&, int);
		/// Moves the given element before another one. This is useful in a container that takes this ordering into
		/// account (\ref stack_panel, for example). Although this can also change their ordering on the Z-axis, it
		/// is not recommended since using \ref element::_zindex directly is usually less error-prone and more
		/// flexible.
		void move_before(element&, element*);

		/// Returns all elements in this collection.
		const std::list<element*> &items() const {
			return _children;
		}
		/// Returns all children of this collection, but in descending order of their Z-indices.
		const std::list<element*> &z_ordered() const {
			return _zorder;
		}

		/// Returns the number of elements currently in the collection.
		std::size_t size() const {
			return _children.size();
		}

		info_event<change_info>
			/// Invoked before an \ref element's about to be added, removed, or before when an element's Z-index has
			/// been changed.
			changing,
			/// Invoked after an \ref element has been added, removed, or after when an element's Z-index has been
			/// changed.
			changed;
	protected:
		panel & _f; ///< The panel that this collection belongs to.
		std::list<element*>
			_children, ///< The list of children.
			_zorder; ///< Also the list of children, stored in descending order of their Z-index.
	};

	/// Base class for all elements that contains other elements.
	///
	/// \remark Before creating classes that derive from this class, read the documentation of
	///         \ref _on_child_removed first.
	class panel : public element {
		friend element;
		friend element_collection;
		friend manager;
		friend scheduler;
		friend class_arrangements;
	public:
		/// If the mouse is over any of its children, then returns the cursor of the children.
		/// Otherwise just returns element::get_current_display_cursor().
		cursor get_current_display_cursor() const override;

		/// Sets whether it should mark all its children for disposal when it's being disposed.
		void set_dispose_children(bool v) {
			_dispose_children = v;
		}
		/// Returns whether it would dispose its children upon disposal.
		[[nodiscard]] bool get_dispose_children() const {
			return _dispose_children;
		}

		/// Returns whether this \ref panel is a focus scope.
		[[nodiscard]] bool is_focus_scope() const {
			return _is_focus_scope;
		}
		/// Returns the focused element in this focus scope. If this panel gets the focus, this element will get the
		/// focus instead.
		[[nodiscard]] element *get_focused_element_in_scope() const {
			return _scope_focus;
		}

		/// Returns all of its children.
		[[nodiscard]] element_collection &children() {
			return _children;
		}


		/// Calculates the layout of an \ref element on a direction (horizontal or vertical) in a \ref panel
		/// with the given parameters. If all of \p anchormin, \p pixelsize, and \p anchormax are \p true, all sizes
		/// are taken into account and the extra space is distributed evenly before and after the element.
		///
		/// \param clientmin Passes the minimum (left or top) boundary of the client region, and will contain the
		///                  minimum boundary of the element's layout as a return value.
		/// \param clientmax Passes the maximum (right or bottom) boundary of the client region, and will contain the
		///                  maximum boundary of the element's layout as a return value.
		/// \param margin_min Size allocation before this element.
		/// \param size Size allocation of this element.
		/// \param margin_max Size allocation after this element.
		static void layout_on_direction(
			double &clientmin, double &clientmax,
			size_allocation margin_min, size_allocation size, size_allocation margin_max
		);
		/// Calculates the horizontal layout of the given \ref element, given the client area that contains it.
		static void layout_child_horizontal(element&, double xmin, double xmax);
		/// Calculates the vertical layout of the given \ref element, given the client area that contains it.
		static void layout_child_vertical(element&, double ymin, double ymax);
		/// Calculates the layout of the given \ref element, given the area that supposedly contains it (usually the
		/// client region of its parent). This function simply calls \ref layout_child_horizontal() and
		/// \ref layout_child_vertical().
		inline static void layout_child(element &child, rectd client) {
			layout_child_horizontal(child, client.xmin, client.xmax);
			layout_child_vertical(child, client.ymin, client.ymax);
		}
		/// Computes the available size for a child on one orientation.
		static double get_available_size_for_child(
			double available, size_allocation anchor_min, size_allocation anchor_max,
			size_allocation_type size_type, double size
		);

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"panel";
		}
	protected:
		/// Utility class used for computing desired size on one orientation. This class assumes that each child
		/// affects the desired size individually.
		template <
			size_allocation (element::*MarginMin)() const,
			size_allocation (element::*MarginMax)() const,
			size_allocation_type (element::*SizeType)() const,
			double (vec2d::*Size)
		> struct _basic_desired_size_accumulator {
			/// Default constructor.
			_basic_desired_size_accumulator() = default;
			/// Initializes \ref available_size.
			explicit _basic_desired_size_accumulator(double avail) : available_size(avail) {
			}

			/// Returns the size available for the given child. Calls \ref get_available_size_for_child().
			[[nodiscard]] double get_available(element &child) const {
				return get_available_size_for_child(
					available_size, (child.*MarginMin)(), (child.*MarginMax)(),
					(child.*SizeType)(), child.get_layout_parameters().size.*Size
				);
			}
			/// Updates \ref maximum_size using the newly-computed desired size of the given child.
			void accumulate(element &child) {
				double pixel_sum = 0.0, prop_sum = 0.0;
				(child.*MarginMin)().accumulate_to(pixel_sum, prop_sum);
				(child.*MarginMax)().accumulate_to(pixel_sum, prop_sum);
				double result_size = pixel_sum;
				auto alloc_type = (child.*SizeType)();
				switch (alloc_type) {
				case size_allocation_type::fixed:
					result_size += child.get_layout_parameters().size.*Size;
					break;
				case size_allocation_type::automatic:
					result_size += child.get_desired_size().*Size;
					break;
				case size_allocation_type::proportion:
					{
						double child_prop = child.get_layout_parameters().size.*Size;
						result_size += child.get_desired_size().*Size * (child_prop + prop_sum) / child_prop;
					}
					break;
				}
				maximum_size = std::max(maximum_size, result_size);
			}

			double available_size = 0.0; ///< The total available size.
			double maximum_size = 0.0; ///< Maximum desired computed from a child.
		};


		/// Calls \ref scheduler::invalidate_children_layout() to mark the layout of all children for updating.
		void _invalidate_children_layout();

		/// Called when an element is about to be added to this panel. Derived classes can override this function to
		/// introduce desired behavior.
		virtual void _on_child_adding(element&, element*) {
		}
		/// Called when an element is about to be removed from the panel. Derived classes can override this function
		/// to introduce desired behavior.
		///
		/// \note Classes that store additional information about their children and allow users to add/remove
		///       elements to/from the elements should override this function to handle the case where an element is
		///       added and then disposed directly, and update stored information accordingly in the overriden
		///       function. This is because elements are automatically detached from their parents when disposed, and
		///       the user may be able to dispose them without notifying the parent.
		virtual void _on_child_removing(element&) {
		}
		/// Called when the z-index of an element is about to be changed. Derived classes can override this function
		/// to introduce desired behavior.
		virtual void _on_child_zindex_changing(element&) {
		}
		/// Called when the order of an element in the container is about to be changed. Derived classes can override
		/// this function to introduce desired behavior.
		virtual void _on_child_order_changing(element&, element*) {
		}

		/// Called when an element has been added to this panel. Invalidates the layout of the newly added element,
		/// and the visual of the panel itself.
		virtual void _on_child_added(element &e, element*) {
			_on_desired_size_changed();
			e.invalidate_layout();
			invalidate_visual();
		}
		/// Called when an element has been removed from the panel. Invalidates the visual of the panel. See
		/// \ref _on_child_removing for some important notes.
		///
		/// \sa _on_child_removing
		virtual void _on_child_removed(element&) {
			_on_desired_size_changed();
			invalidate_visual();
		}
		/// Called when the z-index of an element has been changed. Invalidates the visual of the panel.
		virtual void _on_child_zindex_changed(element&) {
			invalidate_visual();
		}
		/// Called when the order of an element in the container has been changed. Invalidates the visual of the
		/// panel.
		virtual void _on_child_order_changed(element&, element*) {
			invalidate_visual();
		}

		/// Invalidates the layout of all children when padding is changed.
		void _on_padding_changed() override {
			_invalidate_children_layout();
			element::_on_padding_changed();
		}

		/// Renders all element in ascending order of their z-index.
		void _custom_render() const override {
			element::_custom_render();
			for (auto i = _children.z_ordered().rbegin(); i != _children.z_ordered().rend(); ++i) {
				(*i)->_on_render();
			}
		}

		/// Computes the desired size of all children, and finds the minimum size that can accommodate them.
		vec2d _compute_desired_size_impl(vec2d) const override;
		/// Called to update the layout of all children. This is called automatically in \ref _on_layout_changed(),
		/// which in turn can also be explicitly scheduled by using \ref scheduler::notify_layout_change().
		virtual void _on_update_children_layout() {
			rectd client = get_client_region();
			for (auto *elem : _children.items()) {
				layout_child(*elem, client);
			}
		}
		/// Called by a child whose desired size has just changed.
		///
		/// \sa element::_on_desired_size_changed()
		/// \sa _on_child_layout_parameters_changed()
		virtual void _on_child_desired_size_changed(element &child);
		/// Invoked by any child in \ref element::_on_layout_parameters_changed().
		///
		/// \sa _on_child_desired_size_changed()
		virtual void _on_child_layout_parameters_changed(element&) {
		}
		/// Updates the layout of all children.
		void _on_layout_changed() override {
			_on_update_children_layout();
			element::_on_layout_changed();
		}

		/// If the mouse is over a child, calls element::_on_mouse_down on that element.
		/// Sets itself as focused if it can have the focus, and the focus has not been set by another element.
		void _on_mouse_down(mouse_button_info&) override;
		/// Calls element::_on_mouse_leave for all children that still thinks the mouse is over it.
		void _on_mouse_leave() override {
			for (auto i : _children.items()) {
				if (i->is_mouse_over()) {
					i->_on_mouse_leave();
				}
			}
			element::_on_mouse_leave();
		}
		/// Tests for the element that the mouse is over, and calls \ref element::_on_mouse_move(). It also calls
		/// \ref element::_on_mouse_enter() and \ref element::_on_mouse_leave() automatically when necessary.
		void _on_mouse_move(mouse_move_info&) override;
		/// If the mouse is over a child, calls element::_on_mouse_hover on that element.
		void _on_mouse_hover(mouse_hover_info&) override;
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_scroll(mouse_scroll_info &p) override {
			if (element *mouseover = _hit_test_for_child(p.position)) {
				mouseover->_on_mouse_scroll(p);
			}
			element::_on_mouse_scroll(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_up(mouse_button_info &p) override {
			if (element *mouseover = _hit_test_for_child(p.position)) {
				mouseover->_on_mouse_up(p);
			}
			element::_on_mouse_up(p);
		}

		/// Finds the element with the largest Z-index that is interactive and contains the given point.
		[[nodiscard]] virtual element *_hit_test_for_child(const mouse_position&) const;

		/// For each child, removes it from \ref _children, and marks it for disposal if \ref _dispose_children is
		/// \p true.
		void _dispose() override;


		// utility functions used by derived classes to manipulate children
		/// Returns \ref element::_parent_data.
		inline static const std::any &_child_get_parent_data(const element &e) {
			return e._parent_data;
		}
		/// \overload
		inline static std::any &_child_get_parent_data(element &e) {
			return e._parent_data;
		}

		/// Sets the horizontal layout of a given child.
		inline static void _child_set_horizontal_layout(element &e, double xmin, double xmax) {
			e._layout.xmin = xmin;
			e._layout.xmax = xmax;
		}
		/// Sets the vertical layout of a given child.
		inline static void _child_set_vertical_layout(element &e, double ymin, double ymax) {
			e._layout.ymin = ymin;
			e._layout.ymax = ymax;
		}
		/// Sets the layout of a given child.
		inline static void _child_set_layout(element &e, rectd r) {
			e._layout = r;
		}

		/// Calls \ref element::_on_render on a given child.
		inline static void _child_on_render(element &e) {
			e._on_render();
		}


		element_collection _children{ *this }; ///< The collection of its children.
		/// Caches the cursor of the child that the mouse is over.
		cursor _children_cursor = cursor::not_specified;
		/// The child that's focused in this focus scope, if \ref _is_focus_scope is \p true.
		element *_scope_focus = nullptr;
		bool
			/// Indicates whether the panel should mark all children for disposal when disposed.
			_dispose_children = true,
			_is_focus_scope = false; ///< Indicates if this panel is a focus scope (borrowed from WPF).
	};
}
