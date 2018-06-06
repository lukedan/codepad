#pragma once

/// \file
/// Panels used to contain and organize elements.

#include <chrono>
#include <unordered_map>

#include "element.h"
#include "manager.h"
#include "../os/renderer.h"

namespace codepad::ui {
	class panel_base;
	class element;

	/// Contains information about changes in an \ref element_collection.
	struct element_collection_change_info {
		/// Specifies the type of the change.
		enum class type {
			add, ///< An element has been added to the collection.
			remove, ///< An element has been removed from the collection.
			set_zindex ///< The z-index of an element has changed.
		};
		/// Constructor.
		element_collection_change_info(type t, element *c) : change_type(t), changed(c) {
		}
		const type change_type; ///< The type of the change.
		element *const changed; ///< The element that has been changed.
	};
	/// Stores and manages a collection of elements.
	class element_collection {
	public:
		/// Constructs the collection with the given parent.
		explicit element_collection(panel_base &b) : _f(b) {
		}
		/// Destructor. Simply sets element::_parent to \p nullptr for all children.
		~element_collection();

		/// Adds an element to the collection. The element is added using its original z-index.
		/// The element must not be a member of another element collection.
		void add(element&);
		/// Removes an element from the collection. The element must be a member of this collection.
		void remove(element&);
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

		/// Invoked when an element is added, removed, or when an element's z-index has been changed.
		event<element_collection_change_info> changed;
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
		/// Called by \ref element_collection whenever it has changed. Calls the method that
		/// corresponds to the type of the change.
		void _on_children_changed(element_collection_change_info &p) {
			switch (p.change_type) {
			case element_collection_change_info::type::add:
				_on_child_added(p.changed);
				break;
			case element_collection_change_info::type::remove:
				_on_child_removed(p.changed);
				break;
			case element_collection_change_info::type::set_zindex:
				_on_child_zindex_changed(p.changed);
				break;
			}
		}
		/// Called when an element is added to this panel. Invalidates the layout of the newly added element,
		/// and the visual of the panel itself.
		virtual void _on_child_added(element *e) {
			e->invalidate_layout();
			invalidate_visual();
		}
		/// Called when an element is removed from the panel. Invalidates the visual of the panel.
		///
		/// \remark Classes that stores additional information about the children and allow users to add/remove
		///         elements to/from the elements should override this function to handle the case where an element
		///         is added and then disposed directly, and update stored information accordingly in the overriden
		///         function.
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
		void _on_mouse_down(mouse_button_info &p) override {
			bool childhit = false;
			for (auto i : _children) {
				if (_hit_test_child(i, p.position)) {
					i->_on_mouse_down(p);
					childhit = true;
					break;
				}
			}
			mouse_down.invoke(p);
			if (p.button == os::input::mouse_button::primary) {
				if (_can_focus && !p.focus_set()) {
					p.mark_focus_set();
					manager::get().set_focus(this);
				}
				if (!childhit) { // mouse is over a child
					_set_visual_style_bit(visual::get_predefined_states().mouse_down, true);
				}
			}
		}
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

		/// Marks all children for disposal if \ref _dispose_children is \p true.
		void _dispose() override {
			if (_dispose_children) {
				for (auto i : _children) {
					manager::get().mark_disposal(*i);
				}
			}
			element::_dispose();
		}

		/// Tests if a given point is within an element, with regard to its visibility.
		bool _hit_test_child(element *c, vec2d p) {
			if (test_bit_any(c->get_visibility(), visibility::interaction_only)) {
				return c->hit_test(p);
			}
			return false;
		}

		/// Calls element::_recalc_layout on a given child.
		void _child_recalc_layout_noreval(element *e, rectd r) const {
			assert_true_usage(e->_parent == this, "_recalc_layout() can only be invoked on children");
			e->_recalc_layout(r);
		}
		/// Sets the layout of a given child.
		void _child_set_layout_noreval(element *e, rectd r) const {
			assert_true_usage(e->_parent == this, "layout can only be set for children");
			e->_layout = r;
		}
		/// Calls element::_recalc_layout and \ref revalidate_layout on a given child.
		void _child_recalc_layout(element *e, rectd r) const {
			_child_recalc_layout_noreval(e, r);
			e->revalidate_layout();
		}
		/// Sets the layout of a given child, and then calls \ref revalidate_layout.
		void _child_set_layout(element *e, rectd r) const {
			_child_set_layout_noreval(e, r);
			e->revalidate_layout();
		}

		/// Calls element::_on_render on a given child.
		void _child_on_render(element *e) const {
			assert_true_usage(e->_parent == this, "_on_render() can only be invoked on children");
			e->_on_render();
		}

		element_collection _children{*this}; ///< The collection of its children.
		/// Caches the cursor of the child that the mouse is over.
		os::cursor _children_cursor = os::cursor::not_specified;
		bool _dispose_children = true; ///< Indicates whether the panel marks all children for disposal when disposed.
	};

	/// A basic panel whose children can be freely accessed. Cannot have the focus by default.
	class panel : public panel_base {
	public:
		/// Returns all of its children.
		element_collection &children() {
			return _children;
		}
	protected:
		/// Sets \ref _can_focus to \p false.
		void _initialize() override {
			panel_base::_initialize();
			_can_focus = false;
		}
	};
}
