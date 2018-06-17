#pragma once

/// \file
/// Panels used to contain and organize elements.

#include <chrono>
#include <unordered_map>

#include "element.h"
#include "../os/renderer.h"

namespace codepad::ui {
	class panel_base;
	class element;

	/// Contains information about changes in an \ref element_collection.
	struct element_collection_change_info {
		/// Specifies the type of the change.
		enum class type {
			add, ///< Addition of an element.
			remove, ///< Removal of an element.
			set_zindex ///< Change of an element's z-index.
		};
		/// Constructor.
		element_collection_change_info(type t, element *c) : change_type(t), subject(c) {
		}
		const type change_type; ///< The type of the change.
		element *const subject; ///< The \ref element that's involved.
	};
	/// Stores and manages a collection of elements.
	class element_collection {
	public:
		/// Constructs the collection with the given parent.
		explicit element_collection(panel_base &b) : _f(b) {
		}
		/// Destructor. Checks to ensure that the collection has been cleared.
		~element_collection();

		/// Adds an element to the collection. The element is added using its original z-index.
		/// The element must not be a member of another element collection.
		void add(element&);
		/// Removes an element from the collection. The element must be a member of this collection.
		void remove(element&);
		/// Removes all elements from the collection.
		void clear();
		/// Sets the z-index of an element. The element must be a member of this collection.
		void set_zindex(element&, int);

		/// Returns the iterator to the first element.
		std::list<element*>::iterator begin() {
			return _cs.begin();
		}
		/// Const version of begin().
		std::list<element*>::const_iterator begin() const {
			return _cs.begin();
		}
		/// Returns the iterator past the last element.
		std::list<element*>::iterator end() {
			return _cs.end();
		}
		/// Const version of end().
		std::list<element*>::const_iterator end() const {
			return _cs.end();
		}

		/// Returns the reverse iterator to the last element.
		std::list<element*>::reverse_iterator rbegin() {
			return _cs.rbegin();
		}
		/// Const version of rbegin().
		std::list<element*>::const_reverse_iterator rbegin() const {
			return _cs.rbegin();
		}
		/// Returns the reverse iterator past the first element.
		std::list<element*>::reverse_iterator rend() {
			return _cs.rend();
		}
		/// Const version of rend().
		std::list<element*>::const_reverse_iterator rend() const {
			return _cs.rend();
		}

		/// Returns the number of elements currently in the collection.
		size_t size() const {
			return _cs.size();
		}

		event<element_collection_change_info>
			/// Invoked before an \ref element's about to be added, removed, or before when an element's z-index has
			/// been changed.
			changing,
			/// Invoked after an \ref element has been added, removed, or after when an element's z-index has been
			/// changed.
			changed;
	protected:
		panel_base & _f; ///< The panel that this collection belongs to.
		std::list<element*> _cs; ///< The children, stored in descending order of their z-index.
	};

	/// Base class for all elements that contains other elements.
	///
	/// \remark Before creating classes that derive from this class, read the documentation of
	///         \ref _on_child_removed first.
	class panel_base : public element {
		friend class element;
		friend class element_collection;
	public:
		/// Returns whether the panel overrides the layout of its children.
		/// If so, it should calculate and assign their layout in \ref _finish_layout
		/// by overriding the method.
		virtual bool override_children_layout() const {
			return false;
		}

		/// If the mouse is over any of its children, then returns the cursor of the children.
		/// Otherwise just returns element::get_current_display_cursor().
		os::cursor get_current_display_cursor() const override {
			if (_children_cursor != os::cursor::not_specified) {
				return _children_cursor;
			}
			return element::get_current_display_cursor();
		}

		/// Sets whether it should mark all its children for disposal when it's being disposed.
		void set_dispose_children(bool v) {
			_dispose_children = v;
		}
		/// Returns whether it would dispose its children upon disposal.
		bool get_dispose_children() const {
			return _dispose_children;
		}
	protected:
		/// Called when an element is about to be added to this panel. Derived classes can override this function to
		/// introduce desired behavior.
		virtual void _on_child_adding(element*) {
		}
		/// Called when an element is about to be removed from the panel. Classes that stores additional information
		/// about their children and allow users to add/remove elements to/from the elements should override this
		/// function to handle the case where an element is added and then disposed directly, and update stored
		/// information accordingly in the overriden function. This is because elements are automatically detached
		/// from their parents when disposed, and the user may be able to dispose them without notifying the parent.
		virtual void _on_child_removing(element*) {
		}
		/// Called when the z-index of an element is about to be changed. Derived classes can override this function
		/// to introduce desired behavior.
		virtual void _on_child_zindex_changing(element*) {
		}

		/// Called when an element is added to this panel. Invalidates the layout of the newly added element,
		/// and the visual of the panel itself.
		virtual void _on_child_added(element *e) {
			e->invalidate_layout();
			invalidate_visual();
		}
		/// Called when an element is removed from the panel. Invalidates the visual of the panel. See
		/// \ref _on_child_removing for some important notes.
		///
		/// \sa _on_child_removing
		virtual void _on_child_removed(element*) {
			invalidate_visual();
		}
		/// Called when the z-index of an element is changed. Invalidates the visual of the panel.
		virtual void _on_child_zindex_changed(element*) {
			invalidate_visual();
		}

		/// Invalidates the layout of all children when padding is changed.
		void _on_padding_changed() override {
			for (auto i : _children) {
				i->invalidate_layout();
			}
			element::_on_padding_changed();
		}

		/// Renders all element in ascending order of their z-index.
		void _custom_render() override {
			for (auto i = _children.rbegin(); i != _children.rend(); ++i) {
				(*i)->_on_render();
			}
		}

		/// If this panel doesn't override the children's layout, then invalidate all children's layout.
		void _finish_layout() override {
			if (!override_children_layout()) {
				for (auto i : _children) {
					i->invalidate_layout();
				}
			}
			element::_finish_layout();
		}

		/// If the mouse is over an element, calls element::_on_mouse_down on that element.
		/// Sets itself as focused if it can have the focus, and the focus has not been set by another element.
		void _on_mouse_down(mouse_button_info&) override;
		/// Calls element::_on_mouse_leave for all children that still thinks the mouse is over it.
		void _on_mouse_leave() override {
			for (auto i : _children) {
				if (i->is_mouse_over()) {
					i->_on_mouse_leave();
				}
			}
			element::_on_mouse_leave();
		}
		/// Tests for the topmost element that the mouse is over, and calls element::_on_mouse_move.
		/// It may also call element::_on_mouse_enter and element::_on_mouse_leave depending on the state
		/// of the elements.
		void _on_mouse_move(mouse_move_info &p) override {
			bool hit = false;
			_children_cursor = os::cursor::not_specified; // reset cursor
			for (auto i : _children) {
				if (!hit && _hit_test_child(i, p.new_pos)) {
					if (!i->is_mouse_over()) {
						i->_on_mouse_enter();
					}
					i->_on_mouse_move(p);
					_children_cursor = i->get_current_display_cursor(); // get cursor
					hit = true;
					continue;
				}
				// only when the mouse is not over i or it's obstructed by another element
				if (i->is_mouse_over()) {
					i->_on_mouse_leave();
				}
			}
			element::_on_mouse_move(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_scroll(mouse_scroll_info &p) override {
			for (auto i : _children) {
				if (_hit_test_child(i, p.position)) {
					i->_on_mouse_scroll(p);
					break;
				}
			}
			element::_on_mouse_scroll(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_up(mouse_button_info &p) override {
			for (auto i : _children) {
				if (_hit_test_child(i, p.position)) {
					i->_on_mouse_up(p);
					break;
				}
			}
			element::_on_mouse_up(p);
		}

		/// For each child, removes it from \ref _children, and marks it for disposal if \ref _dispose_children is
		/// \p true.
		void _dispose() override;

		/// Tests if a given point is within an element, with regard to its visibility.
		bool _hit_test_child(element *c, vec2d p) {
			if (test_bit_any(c->get_visibility(), visibility::interaction_only)) {
				return c->hit_test(p);
			}
			return false;
		}

		/// Calls \ref element::_recalc_layout on a given child.
		inline static void _child_recalc_layout_noreval(element *e, rectd r) {
			e->_recalc_layout(r);
		}
		/// Calls \ref element::_recalc_horizontal_layout on a given child.
		inline static void _child_recalc_horizontal_layout_noreval(element *e, double xmin, double xmax) {
			e->_recalc_horizontal_layout(xmin, xmax);
		}
		/// Calls \ref element::_recalc_vertical_layout on a given child.
		inline static void _child_recalc_vertical_layout_noreval(element *e, double ymin, double ymax) {
			e->_recalc_vertical_layout(ymin, ymax);
		}
		/// Sets the layout of a given child.
		inline static void _child_set_layout_noreval(element *e, rectd r) {
			e->_layout = r;
		}
		/// Calls \ref element::_recalc_layout and \ref revalidate_layout on a given child.
		inline static void _child_recalc_layout(element *e, rectd r) {
			_child_recalc_layout_noreval(e, r);
			e->revalidate_layout();
		}
		/// Sets the layout of a given child, then calls \ref revalidate_layout.
		inline static void _child_set_layout(element *e, rectd r) {
			_child_set_layout_noreval(e, r);
			e->revalidate_layout();
		}

		/// Calls \ref element::_on_render on a given child.
		inline static void _child_on_render(element *e) {
			e->_on_render();
		}

		element_collection _children{*this}; ///< The collection of its children.
		/// Caches the cursor of the child that the mouse is over.
		os::cursor _children_cursor = os::cursor::not_specified;
		/// Indicates whether the panel marks all children for disposal when disposed.
		bool _dispose_children = true;
	};

	/// A basic panel whose children can be freely accessed. Cannot have the focus by default.
	class panel : public panel_base {
	public:
		/// Returns all of its children.
		element_collection & children() {
			return _children;
		}
	protected:
		/// Sets \ref _can_focus to \p false.
		void _initialize() override {
			panel_base::_initialize();
			_can_focus = false;
		}
	};

	/// Arranges all children sequentially in a given \ref orientation.
	class stack_panel_base : public panel_base {
	public:
		/// Overrides the layout of children.
		bool override_children_layout() const override {
			return true;
		}

		/// Calculates the layout of a list of elements as if they were in a \ref stack_panel_base with the given
		/// orientation and client area. All elements must be children of the given \ref panel_base.
		template <orientation Orientation> inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems
		) {
			if constexpr (Orientation == orientation::horizontal) {
				_layout_elements_in_impl<
					Orientation, &element::_recalc_vertical_layout,
					&rectd::xmin, &rectd::xmax, &rectd::ymin, &rectd::ymax
				>(client, elems);
			} else {
				_layout_elements_in_impl<
					Orientation, &element::_recalc_horizontal_layout,
					&rectd::ymin, &rectd::ymax, &rectd::xmin, &rectd::xmax
				>(client, elems);
			}
		}
		/// Calls the corresponding templated version of \ref layout_elements_in.
		inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems, orientation ori
		) {
			if (ori == orientation::horizontal) {
				layout_elements_in<orientation::horizontal>(client, elems);
			} else {
				layout_elements_in<orientation::vertical>(client, elems);
			}
		}
	protected:
		/// Contains information (size, whether size is a proportion) used for layout calculation.
		struct _elem_layout_info {
			/// Default constructor.
			_elem_layout_info() = default;

			std::pair<double, bool>
				margin_min, ///< The element's layout settings for its margin in the `negative' direction.
				size, ///< The element's layout settings for its size.
				margin_max; ///< The element's layout settings for the margin in the `positive' direction.

			/// Extracts the information corresponding to the given \ref orientation from the given element.
			template <orientation Orient> inline static _elem_layout_info extract(const element &e) {
				_elem_layout_info res;
				if constexpr (Orient == orientation::horizontal) {
					res.margin_min = {e.get_margin().left, test_bit_any(e.get_anchor(), anchor::left)};
					res.margin_max = {e.get_margin().right, test_bit_any(e.get_anchor(), anchor::right)};
					res.size = e.get_layout_width();
				} else {
					res.margin_min = {e.get_margin().top, test_bit_any(e.get_anchor(), anchor::top)};
					res.margin_max = {e.get_margin().bottom, test_bit_any(e.get_anchor(), anchor::bottom)};
					res.size = e.get_layout_height();
				}
				return res;
			}
		};
		/// Implementation of \ref layout_elements_in for a certain \ref orientation.
		///
		/// \tparam Orientation The orientation of the stack panel.
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
			orientation Orientation,
			void (element::*CalcDefaultDir)(double, double),
			double rectd::*MainMin, double rectd::*MainMax,
			double rectd::*DefMin, double rectd::*DefMax
		> inline static void _layout_elements_in_impl(rectd client, const std::vector<element*> &elems) {
			std::vector<_elem_layout_info> layoutinfo;
			double total_prop = 0.0, total_px = 0.0;
			for (element *e : elems) {
				(e->*CalcDefaultDir)(client.*DefMin, client.*DefMax);
				_elem_layout_info info = _elem_layout_info::extract<Orientation>(*e);
				(info.margin_min.second ? total_px : total_prop) += info.margin_min.first;
				(info.size.second ? total_px : total_prop) += info.size.first;
				(info.margin_max.second ? total_px : total_prop) += info.margin_max.first;
				layoutinfo.emplace_back(info);
			}
			// distribute the remaining space
			double prop_mult = (client.*MainMax - client.*MainMin - total_px) / total_prop, pos = client.*MainMin;
			for (size_t i = 0; i < elems.size(); ++i) {
				element *e = elems[i];
				const _elem_layout_info &info = layoutinfo[i];
				rectd nl = e->get_layout();
				nl.*MainMin = pos +=
					info.margin_min.second ?
					info.margin_min.first :
					info.margin_min.first * prop_mult;
				nl.*MainMax = pos +=
					info.size.second ?
					info.size.first :
					info.size.first * prop_mult;
				pos +=
					info.margin_max.second ?
					info.margin_max.first :
					info.margin_max.first * prop_mult;
				_child_set_layout(e, nl);
			}
		}

		orientation _ori = orientation::vertical; ///< The orientation in which children are placed.
	};
}
