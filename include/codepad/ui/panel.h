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

		/// Returns the maximum width of its children that is specified in pixels, plus padding.
		size_allocation get_desired_width() const override;
		/// Returns the maximum height of its children that is specified in pixels, plus padding.
		size_allocation get_desired_height() const override;

		/// Sets whether it should mark all its children for disposal when it's being disposed.
		void set_dispose_children(bool v) {
			_dispose_children = v;
		}
		/// Returns whether it would dispose its children upon disposal.
		bool get_dispose_children() const {
			return _dispose_children;
		}

		/// Returns whether this \ref panel is a focus scope.
		bool is_focus_scope() const {
			return _is_focus_scope;
		}
		/// Returns the focused element in this focus scope. If this panel gets the focus, this element will get the
		/// focus instead.
		element *get_focused_element_in_scope() const {
			return _scope_focus;
		}

		/// Returns all of its children.
		element_collection &children() {
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
		/// \param anchormin \p true if the element is anchored towards the `negative' (left or top) direction.
		/// \param pixelsize \p true if the size of the element is specified in pixels.
		/// \param anchormax \p true if the element is anchored towards the `positive' (right or bottom) direction.
		/// \param marginmin The element's margin at the `negative' border.
		/// \param size The size of the element in the direction.
		/// \param marginmax The element's margin at the `positive' border.
		static void layout_on_direction(
			double &clientmin, double &clientmax,
			bool anchormin, bool pixelsize, bool anchormax,
			double marginmin, double size, double marginmax
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

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"panel";
		}
	protected:
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
			_on_desired_size_changed(true, true);
			e.invalidate_layout();
			invalidate_visual();
		}
		/// Called when an element has been removed from the panel. Invalidates the visual of the panel. See
		/// \ref _on_child_removing for some important notes.
		///
		/// \sa _on_child_removing
		virtual void _on_child_removed(element&) {
			_on_desired_size_changed(true, true);
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

		/// Called to update the layout of all children. This is called automatically in \ref _on_layout_changed(),
		/// which in turn can also be explicitly scheduled by using \ref scheduler::notify_layout_change().
		virtual void _on_update_children_layout() {
			rectd client = get_client_region();
			for (auto *elem : _children.items()) {
				layout_child(*elem, client);
			}
		}
		/// Called by a child whose desired size has just changed in a way that may affect its layout. This function
		/// calls \ref _on_child_desired_size_changed() to determine if the change may affect the layout of this
		/// panel, the it calls either \ref _on_child_desired_size_changed() on the parent if the result is yes, or
		/// \ref invalidate_layout() on the child otherwise.
		///
		/// \sa element::_on_desired_size_changed()
		/// \sa _on_child_layout_parameters_changed()
		virtual void _on_child_desired_size_changed(element &child, bool width, bool height);
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
		element *_hit_test_for_child(const mouse_position&) const;

		/// Calls \ref class_arrangements::construct_children() to construct children elements.
		void _initialize(std::u8string_view cls) override;
		/// For each child, removes it from \ref _children, and marks it for disposal if \ref _dispose_children is
		/// \p true.
		void _dispose() override;

		/// Returns a \ref class_arrangements::notify_mapping that contains information about children that are
		/// relevant to the logic of this \ref panel.
		virtual class_arrangements::notify_mapping _get_child_notify_mapping() {
			return class_arrangements::notify_mapping();
		}

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
		/// Sets the logical parent of a child.
		inline static void _child_set_logical_parent(element &e, panel *logparent) {
			e._logical_parent = logparent;
		}

		/// Calls \ref element::_on_render on a given child.
		inline static void _child_on_render(element &e) {
			e._on_render();
		}


		// utility function used to obtain pixel sizes.
		/// Returns the total horizontal span of the given element that is specified in pixels, i.e., excluding
		/// all widths specified in proportions. Returns \p std::nullopt if all sizes are proportional.
		static std::optional<double> _get_horizontal_absolute_span(const element&);
		/// Similar to \ref _get_horizontal_absolute_span(), but for the height.
		static std::optional<double> _get_vertical_absolute_span(const element&);

		/// Returns the maximum horizontal span returned by \ref _get_horizontal_absolute_span(). This function takes
		/// element visibility (more specifically, \ref visibility::layout) into account.
		static std::optional<double> _get_max_horizontal_absolute_span(const element_collection&);
		/// Similar to \ref _get_max_horizontal_absolute_span().
		static std::optional<double> _get_max_vertical_absolute_span(const element_collection&);

		/// Returns the total horizontal span returned by \ref _get_horizontal_absolute_span(). This function takes
		/// element visibility (more specifically, \ref visibility::layout) into account.
		static std::optional<double> _get_total_horizontal_absolute_span(const element_collection&);
		/// Similar to \ref _get_total_horizontal_absolute_span().
		static std::optional<double> _get_total_vertical_absolute_span(const element_collection&);


		/// Used by composite elements to automatically check and cast a component pointer to the correct type, and
		/// assign it to the given pointer.
		template <typename Elem> inline static class_arrangements::construction_notify _name_cast(Elem *&elem) {
			return [ppelem = &elem](element *ptr) {
				*ppelem = dynamic_cast<Elem*>(ptr);
				if (*ppelem == nullptr) {
					logger::get().log_error(CP_HERE) <<
						"incorrect component type, need " << demangle(typeid(Elem).name()) <<
						", found " << demangle(typeid(*ptr).name());
				}
			};
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
