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
	class panel_base;

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
		explicit element_collection(panel_base &b) : _f(b) {
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
		size_t size() const {
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
		panel_base & _f; ///< The panel that this collection belongs to.
		std::list<element*>
			_children, ///< The list of children.
			_zorder; ///< Also the list of children, stored in descending order of their Z-index.
	};

	/// Base class for all elements that contains other elements.
	///
	/// \remark Before creating classes that derive from this class, read the documentation of
	///         \ref _on_child_removed first.
	class panel_base : public element {
		friend element;
		friend element_collection;
		friend manager;
		friend scheduler;
		friend class_arrangements;
	public:
		/// If the mouse is over any of its children, then returns the cursor of the children.
		/// Otherwise just returns element::get_current_display_cursor().
		cursor get_current_display_cursor() const override {
			if (_children_cursor != cursor::not_specified) {
				return _children_cursor;
			}
			return element::get_current_display_cursor();
		}

		/// Returns the maximum width of its children, plus padding.
		size_allocation get_desired_width() const override {
			double maxw = 0.0;
			for (const element *e : _children.items()) {
				if (e->is_visible(visibility::layout)) {
					maxw = std::max(maxw, _get_horizontal_absolute_span(*e));
				}
			}
			return size_allocation(maxw + get_padding().width(), true);
		}
		/// Returns the maximum height of its children, plus padding.
		size_allocation get_desired_height() const override {
			double maxh = 0.0;
			for (const element *e : _children.items()) {
				if (e->is_visible(visibility::layout)) {
					maxh = std::max(maxh, _get_vertical_absolute_span(*e));
				}
			}
			return size_allocation(maxh + get_padding().height(), true);
		}

		/// Sets whether it should mark all its children for disposal when it's being disposed.
		void set_dispose_children(bool v) {
			_dispose_children = v;
		}
		/// Returns whether it would dispose its children upon disposal.
		bool get_dispose_children() const {
			return _dispose_children;
		}


		/// Returns whether this \ref panel_base is a focus scope.
		bool is_focus_scope() const {
			return _is_focus_scope;
		}
		/// Returns the focused element in this focus scope. If this panel gets the focus, this element will get the
		/// focus instead.
		element *get_focused_element_in_scope() const {
			return _scope_focus;
		}


		/// Calculates the layout of an \ref element on a direction (horizontal or vertical) in a \ref panel_base
		/// with the given parameters. If all of \p anchormin, \p pixelsize, and \p anchormax are \p true, all sizes
		/// are taken into account and the extra space is distributed evenly before and after the element.
		///
		/// \param anchormin \p true if the element is anchored towards the `negative' (left or top) direction.
		/// \param pixelsize \p true if the size of the element is specified in pixels.
		/// \param anchormax \p true if the element is anchored towards the `positive' (right or bottom) direction.
		/// \param clientmin Passes the minimum (left or top) boundary of the client region, and will contain the
		///                  minimum boundary of the element's layout as a return value.
		/// \param clientmax Passes the maximum (right or bottom) boundary of the client region, and will contain the
		///                  maximum boundary of the element's layout as a return value.
		/// \param marginmin The element's margin at the `negative' border.
		/// \param size The size of the element in the direction.
		/// \param marginmax The element's margin at the `positive' border.
		inline static void layout_on_direction(
			bool anchormin, bool pixelsize, bool anchormax, double &clientmin, double &clientmax,
			double marginmin, double size, double marginmax
		) {
			double totalspace = clientmax - clientmin, totalprop = 0.0;
			if (anchormax) {
				totalspace -= marginmax;
			} else {
				totalprop += marginmax;
			}
			if (pixelsize) {
				totalspace -= size;
			} else {
				totalprop += size;
			}
			if (anchormin) {
				totalspace -= marginmin;
			} else {
				totalprop += marginmin;
			}
			double propmult = totalspace / totalprop;
			// size in pixels are prioritized so that zero-size proportion parts are ignored when possible
			if (anchormin && anchormax) {
				if (pixelsize) {
					double midpos = 0.5 * (clientmin + clientmax);
					clientmin = midpos - 0.5 * size;
					clientmax = midpos + 0.5 * size;
				} else {
					clientmin += marginmin;
					clientmax -= marginmax;
				}
			} else if (anchormin) {
				clientmin += marginmin;
				clientmax = clientmin + (pixelsize ? size : size * propmult);
			} else if (anchormax) {
				clientmax -= marginmax;
				clientmin = clientmax - (pixelsize ? size : size * propmult);
			} else {
				clientmin += marginmin * propmult;
				clientmax -= marginmax * propmult;
			}
		}
		/// Calculates the horizontal layout of the given \ref element, given the client area that contains it.
		inline static void layout_child_horizontal(element & child, double xmin, double xmax) {
			anchor anc = child.get_anchor();
			thickness margin = child.get_margin();
			auto wprop = child.get_layout_width();
			child._layout.xmin = xmin;
			child._layout.xmax = xmax;
			layout_on_direction(
				(anc & anchor::left) != anchor::none, wprop.is_pixels, (anc & anchor::right) != anchor::none,
				child._layout.xmin, child._layout.xmax, margin.left, wprop.value, margin.right
			);
		}
		/// Calculates the vertical layout of the given \ref element, given the client area that contains it.
		inline static void layout_child_vertical(element & child, double ymin, double ymax) {
			anchor anc = child.get_anchor();
			thickness margin = child.get_margin();
			auto hprop = child.get_layout_height();
			child._layout.ymin = ymin;
			child._layout.ymax = ymax;
			layout_on_direction(
				(anc & anchor::top) != anchor::none, hprop.is_pixels, (anc & anchor::bottom) != anchor::none,
				child._layout.ymin, child._layout.ymax, margin.top, hprop.value, margin.bottom
			);
		}
		/// Calculates the layout of the given \ref element, given the area that supposedly contains it (usually the
		/// client region of its parent). This function simply calls \ref layout_child_horizontal() and
		/// \ref layout_child_vertical().
		inline static void layout_child(element & child, rectd client) {
			layout_child_horizontal(child, client.xmin, client.xmax);
			layout_child_vertical(child, client.ymin, client.ymax);
		}
	protected:
		/// Calls \ref manager::invalidate_children_layout() to mark the layout of all children for update.
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
		virtual void _on_child_added(element & e, element*) {
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
		void _custom_render() override {
			for (auto i = _children.z_ordered().rbegin(); i != _children.z_ordered().rend(); ++i) {
				(*i)->_on_render();
			}
		}

		/// Called to update the layout of all children. This is called automatically in \ref _on_layout_changed(),
		/// or it can also be explicitly scheduled by using \ref element::notify_layout_change().
		virtual void _on_update_children_layout() {
			rectd client = get_client_region();
			for (auto *elem : _children.items()) {
				layout_child(*elem, client);
			}
		}
		/// Called by \ref _on_child_desired_size_changed() to determine if the change of a child's desired size may
		/// affect the layout of this panel.
		///
		/// \sa _on_child_desired_size_changed()
		virtual bool _is_child_desired_size_relevant(element&, bool width, bool height) const {
			return
				(width && get_width_allocation() == size_allocation_type::automatic) ||
				(height && get_height_allocation() == size_allocation_type::automatic);
		}
		/// Called by a child whose desired size has just changed in a way that may affect its layout. This function
		/// calls \ref _on_child_desired_size_changed() to determine if the change may affect the layout of this
		/// panel, the it calls either \ref _on_child_desired_size_changed() on the parent if the result is yes, or
		/// \ref invalidate_layout() on the child otherwise.
		///
		/// \sa element::_on_desired_size_changed()
		void _on_child_desired_size_changed(element & child, bool width, bool height) {
			if (_is_child_desired_size_relevant(child, width, height)) { // actually affects something
				if (_parent != nullptr) {
					_parent->_on_child_desired_size_changed(*this, width, height);
				} else {
					invalidate_layout();
				}
			} else {
				child.invalidate_layout();
			}
		}
		/// Invalidate all children's layout.
		void _on_layout_changed() override {
			_on_update_children_layout();
			element::_on_layout_changed();
		}

		/// If the mouse is over an element, calls element::_on_mouse_down on that element.
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
		void _on_mouse_move(mouse_move_info & p) override {
			_children_cursor = cursor::not_specified; // reset cursor
			element *mouseover = _hit_test_for_child(p.new_position);
			for (element *j : _children.z_ordered()) { // the mouse cannot be over any other element
				if (mouseover != j && j->is_mouse_over()) {
					j->_on_mouse_leave();
				}
			}
			if (mouseover != nullptr) {
				if (!mouseover->is_mouse_over()) { // just moved onto the element
					mouseover->_on_mouse_enter();
				}
				mouseover->_on_mouse_move(p);
				_children_cursor = mouseover->get_current_display_cursor(); // get cursor
			}
			element::_on_mouse_move(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_scroll(mouse_scroll_info & p) override {
			element *mouseover = _hit_test_for_child(p.position);
			if (mouseover != nullptr) {
				mouseover->_on_mouse_scroll(p);
			}
			element::_on_mouse_scroll(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_up(mouse_button_info & p) override {
			element *mouseover = _hit_test_for_child(p.position);
			if (mouseover != nullptr) {
				mouseover->_on_mouse_up(p);
			}
			element::_on_mouse_up(p);
		}

		/// For each child, removes it from \ref _children, and marks it for disposal if \ref _dispose_children is
		/// \p true.
		void _dispose() override;

		/// Finds the element with the largest Z-index that is interactive and contains the given point.
		element *_hit_test_for_child(vec2d);

		/// Returns \ref element::_parent_data.
		inline static const std::any &_child_get_parent_data(const element & e) {
			return e._parent_data;
		}
		/// \overload
		inline static std::any &_child_get_parent_data(element & e) {
			return e._parent_data;
		}

		/// Sets the horizontal layout of a given child.
		inline static void _child_set_horizontal_layout(element & e, double xmin, double xmax) {
			e._layout.xmin = xmin;
			e._layout.xmax = xmax;
		}
		/// Sets the vertical layout of a given child.
		inline static void _child_set_vertical_layout(element & e, double ymin, double ymax) {
			e._layout.ymin = ymin;
			e._layout.ymax = ymax;
		}
		/// Sets the layout of a given child.
		inline static void _child_set_layout(element & e, rectd r) {
			e._layout = r;
		}
		/// Sets the logical parent of a child.
		inline static void _child_set_logical_parent(element & e, panel_base * logparent) {
			e._logical_parent = logparent;
		}

		/// Calls \ref element::_on_render on a given child.
		inline static void _child_on_render(element & e) {
			e._on_render();
		}

		/// Returns the total horizontal span of the specified element that is specified in pixels.
		inline static double _get_horizontal_absolute_span(const element & e) {
			double cur = 0.0;
			thickness margin = e.get_margin();
			anchor anc = e.get_anchor();
			if ((anc & anchor::left) != anchor::none) {
				cur += margin.left;
			}
			auto sz = e.get_layout_width();
			if (sz.is_pixels) {
				cur += sz.value;
			}
			if ((anc & anchor::right) != anchor::none) {
				cur += margin.right;
			}
			return cur;
		}
		/// Returns the total vertical span of the specified element that is specified in pixels.
		inline static double _get_vertical_absolute_span(const element & e) {
			double cur = 0.0;
			thickness margin = e.get_margin();
			anchor anc = e.get_anchor();
			if ((anc & anchor::top) != anchor::none) {
				cur += margin.top;
			}
			auto sz = e.get_layout_height();
			if (sz.is_pixels) {
				cur += sz.value;
			}
			if ((anc & anchor::bottom) != anchor::none) {
				cur += margin.bottom;
			}
			return cur;
		}

		/// Used by composite elements to automatically check and cast a component pointer to the correct type, and
		/// assign it to the given pointer.
		template <typename Elem> inline static class_arrangements::construction_notify _role_cast(Elem * &elem) {
			return [ppelem = &elem](element * ptr) {
				*ppelem = dynamic_cast<Elem*>(ptr);
				if (*ppelem == nullptr) {
					logger::get().log_warning(
						CP_HERE, "potentially incorrect component type, need ", demangle(typeid(Elem).name())
					);
				}
			};
		}

		element_collection _children{*this}; ///< The collection of its children.
		/// Caches the cursor of the child that the mouse is over.
		cursor _children_cursor = cursor::not_specified;
		/// The child that's focused in this focus scope, if \ref _is_focus_scope is \p true.
		element *_scope_focus = nullptr;
		bool
			/// Indicates whether the panel should mark all children for disposal when disposed.
			_dispose_children = true,
			_is_focus_scope = false; ///< Indicates if this panel is a focus scope (borrowed from WPF).
	};

	/// A basic panel whose children can be freely accessed.
	class panel : public panel_base {
	public:
		/// Returns all of its children.
		element_collection &children() {
			return _children;
		}

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("panel");
		}
	};

	/// Arranges all children sequentially in a given orientation.
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
			}
		}

		/// Returns the minimum width that can contain all elements in pixels, plus padding. More specifically, the
		/// return value is the padding plus the sum of all horizontal sizes specified in pixels, ignoring those
		/// specified as proportions, if the panel is in a horizontal state; or the padding plus the maximum
		/// horizontal size specified in pixels otherwise.
		size_allocation get_desired_width() const override {
			double val = 0.0;
			for (element *e : _children.items()) {
				if (e->is_visible(visibility::layout)) {
					double span = _get_horizontal_absolute_span(*e);
					val = get_orientation() == orientation::vertical ? std::max(val, span) : val + span;
				}
			}
			return size_allocation(val + get_padding().width(), true);
		}
		/// Returns the minimum height that can contain all elements in pixels, plus padding. All heights specified
		/// in proportions are ignored.
		///
		/// \sa get_desired_width
		size_allocation get_desired_height() const override {
			double val = 0.0;
			for (element *e : _children.items()) {
				if (e->is_visible(visibility::layout)) {
					double span = _get_vertical_absolute_span(*e);
					val = get_orientation() == orientation::vertical ? val + span : std::max(val, span);
				}
			}
			return size_allocation(val + get_padding().height(), true);
		}

		/// Calculates the layout of a list of elements as if they were in a \ref stack_panel with the given
		/// orientation and client area. All elements must be children of the given \ref panel_base.
		/// \ref element::notify_layout_change() is called automatically.
		template <bool Vertical> inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems
		) {
			if constexpr (Vertical) {
				_layout_elements_in_impl<
					true, &panel_base::layout_child_horizontal,
					&rectd::ymin, &rectd::ymax, &rectd::xmin, &rectd::xmax
				>(client, elems);
			} else {
				_layout_elements_in_impl<
					false, &panel_base::layout_child_vertical,
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

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("stack_panel");
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
		> inline static void _layout_elements_in_impl(rectd client, const std::vector<element*> & elems) {
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
				} else { // not accounted for; behave as panel_base
					panel_base::layout_child(*e, client);
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
		void _on_child_added(element & elem, element * before) override {
			_invalidate_children_layout();
			panel::_on_child_added(elem, before);
		}
		/// Invalidates the children's layout as well.
		void _on_child_removed(element & elem) override {
			_invalidate_children_layout();
			panel::_on_child_removed(elem);
		}
		/// Invalidates the children's layout since it is determined by their ordering.
		void _on_child_order_changed(element & elem, element * before) override {
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

		/// Handles the orientation attribute.
		void _set_attribute(str_view_t name, const json::value_storage &value) override {
			if (name == u8"orientation") {
				if (orientation o; json_object_parsers::try_parse(value.get_value(), o)) {
					set_orientation(o);
				}
				return;
			}
			panel::_set_attribute(name, value);
		}
	};
}
