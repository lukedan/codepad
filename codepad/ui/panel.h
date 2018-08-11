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
			set_zindex, ///< Change of an element's z-index.
			set_order ///< Change of an element's order in the container.
		};
		/// Constructor.
		element_collection_change_info(type t, element &c) : change_type(t), subject(c) {
		}
		const type change_type; ///< The type of the change.
		element &subject; ///< The \ref element that's involved.
	};
	/// Stores and manages a collection of elements.
	class element_collection {
	public:
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

		event<element_collection_change_info>
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
		friend class element;
		friend class element_collection;
		friend class class_arrangements;
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

		/// Returns the maximum width of its children, plus padding.
		std::pair<double, bool> get_desired_width() const override {
			double maxw = 0.0;
			for (const element *e : _children.items()) {
				maxw = std::max(maxw, _get_horizontal_absolute_span(*e));
			}
			return {maxw + get_padding().width(), true};
		}
		/// Returns the maximum height of its children, plus padding.
		std::pair<double, bool> get_desired_height() const override {
			double maxh = 0.0;
			for (const element *e : _children.items()) {
				maxh = std::max(maxh, _get_vertical_absolute_span(*e));
			}
			return {maxh + get_padding().height(), true};
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
		virtual void _on_child_adding(element&) {
		}
		/// Called when an element is about to be removed from the panel. Classes that stores additional information
		/// about their children and allow users to add/remove elements to/from the elements should override this
		/// function to handle the case where an element is added and then disposed directly, and update stored
		/// information accordingly in the overriden function. This is because elements are automatically detached
		/// from their parents when disposed, and the user may be able to dispose them without notifying the parent.
		virtual void _on_child_removing(element&) {
		}
		/// Called when the z-index of an element is about to be changed. Derived classes can override this function
		/// to introduce desired behavior.
		virtual void _on_child_zindex_changing(element&) {
		}
		/// Called when the order of an element in the container is about to be changed. Derived classes can override
		/// this function to introduce desired behavior.
		virtual void _on_child_order_changing(element&) {
		}

		/// Called when an element has been added to this panel. Invalidates the layout of the newly added element,
		/// and the visual of the panel itself.
		virtual void _on_child_added(element &e) {
			e.invalidate_layout();
			invalidate_visual();
		}
		/// Called when an element has been removed from the panel. Invalidates the visual of the panel. See
		/// \ref _on_child_removing for some important notes.
		///
		/// \sa _on_child_removing
		virtual void _on_child_removed(element&) {
			invalidate_visual();
		}
		/// Called when the z-index of an element has been changed. Invalidates the visual of the panel.
		virtual void _on_child_zindex_changed(element&) {
			invalidate_visual();
		}
		/// Called when the order of an element in the container has been changed. Invalidates the visual of the
		/// panel.
		virtual void _on_child_order_changed(element&) {
			invalidate_visual();
		}

		/// Invalidates the layout of all children when padding is changed.
		void _on_padding_changed() override {
			for (auto i : _children.items()) {
				i->invalidate_layout();
			}
			element::_on_padding_changed();
		}

		/// Renders all element in ascending order of their z-index.
		void _custom_render() override {
			for (auto i = _children.z_ordered().rbegin(); i != _children.z_ordered().rend(); ++i) {
				(*i)->_on_render();
			}
		}

		/// If this panel doesn't override the children's layout, then invalidate all children's layout.
		void _finish_layout() override {
			if (!override_children_layout()) {
				for (auto i : _children.items()) {
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
			for (auto i : _children.items()) {
				if (i->is_mouse_over()) {
					i->_on_mouse_leave();
				}
			}
			element::_on_mouse_leave();
		}
		/// Tests for the element that the mouse is over, and calls \ref element::_on_mouse_move(). It also calls
		/// \ref element::_on_mouse_enter() and \ref element::_on_mouse_leave() automatically when necessary.
		void _on_mouse_move(mouse_move_info &p) override {
			_children_cursor = os::cursor::not_specified; // reset cursor
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
		void _on_mouse_scroll(mouse_scroll_info &p) override {
			element *mouseover = _hit_test_for_child(p.position);
			if (mouseover != nullptr) {
				mouseover->_on_mouse_scroll(p);
			}
			element::_on_mouse_scroll(p);
		}
		/// Sends the message to the topmost element that the mouse is over.
		void _on_mouse_up(mouse_button_info &p) override {
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

		/// Calls \ref element::_recalc_layout on a given child.
		inline static void _child_recalc_layout_noreval(element &e, rectd r) {
			e._recalc_layout(r);
		}
		/// Calls \ref element::_recalc_horizontal_layout on a given child.
		inline static void _child_recalc_horizontal_layout_noreval(element &e, double xmin, double xmax) {
			e._recalc_horizontal_layout(xmin, xmax);
		}
		/// Sets the horizontal layout of a given child.
		inline static void _child_set_horizontal_layout_noreval(element &e, double xmin, double xmax) {
			e._layout.xmin = xmin;
			e._layout.xmax = xmax;
		}
		/// Calls \ref element::_recalc_vertical_layout on a given child.
		inline static void _child_recalc_vertical_layout_noreval(element &e, double ymin, double ymax) {
			e._recalc_vertical_layout(ymin, ymax);
		}
		/// Sets the vertical layout of a given child.
		inline static void _child_set_vertical_layout_noreval(element &e, double ymin, double ymax) {
			e._layout.ymin = ymin;
			e._layout.ymax = ymax;
		}
		/// Sets the layout of a given child.
		inline static void _child_set_layout_noreval(element &e, rectd r) {
			e._layout = r;
		}
		/// Calls \ref element::_recalc_layout and \ref revalidate_layout on a given child.
		inline static void _child_recalc_layout(element &e, rectd r) {
			_child_recalc_layout_noreval(e, r);
			e.revalidate_layout();
		}
		/// Sets the layout of a given child, then calls \ref revalidate_layout.
		inline static void _child_set_layout(element &e, rectd r) {
			_child_set_layout_noreval(e, r);
			e.revalidate_layout();
		}
		/// Sets the logical parent of a child.
		inline static void _child_set_logical_parent(element &e, panel_base *logparent) {
			e._logical_parent = logparent;
		}

		/// Calls \ref element::_on_render on a given child.
		inline static void _child_on_render(element &e) {
			e._on_render();
		}

		/// Returns the total horizontal span of the specified element that is specified in pixels.
		inline static double _get_horizontal_absolute_span(const element &e) {
			double cur = 0.0;
			thickness margin = e.get_margin();
			anchor anc = e.get_anchor();
			if (test_bits_any(anc, anchor::left)) {
				cur += margin.left;
			}
			auto sz = e.get_layout_width();
			if (sz.second) {
				cur += sz.first;
			}
			if (test_bits_any(anc, anchor::right)) {
				cur += margin.right;
			}
			return cur;
		}
		/// Returns the total vertical span of the specified element that is specified in pixels.
		inline static double _get_vertical_absolute_span(const element &e) {
			double cur = 0.0;
			thickness margin = e.get_margin();
			anchor anc = e.get_anchor();
			if (test_bits_any(anc, anchor::top)) {
				cur += margin.top;
			}
			auto sz = e.get_layout_height();
			if (sz.second) {
				cur += sz.first;
			}
			if (test_bits_any(anc, anchor::bottom)) {
				cur += margin.bottom;
			}
			return cur;
		}

		/// Used by composite elements to automatically check and cast a component pointer to the correct type, and
		/// assign it to the given pointer.
		template <typename Elem> inline static class_arrangements::construction_notify _role_cast(Elem *&elem) {
			return [ppelem = &elem](element *ptr) {
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

		/// Returns the default class of elements of this type.
		inline static str_t get_default_class() {
			return CP_STRLIT("panel");
		}
	protected:
		/// Sets \ref _can_focus to \p false.
		void _initialize(const str_t &cls, const element_metrics &metrics) override {
			panel_base::_initialize(cls, metrics);
			_can_focus = false;
		}
	};

	/// Arranges all children sequentially in a given orientation.
	class stack_panel : public panel {
	public:
		/// Overrides the layout of children.
		bool override_children_layout() const override {
			return true;
		}

		/// Returns the minimum width that can contain all elements in pixels, plus padding. More specifically, the
		/// return value is the padding plus the sum of all horizontal sizes specified in pixels, ignoring those
		/// specified as proportions, if the panel is in a horizontal state; or the padding plus the maximum
		/// horizontal size specified in pixels otherwise.
		std::pair<double, bool> get_desired_width() const override {
			double val = 0.0;
			for (element *e : _children.items()) {
				double span = _get_horizontal_absolute_span(*e);
				val = is_vertical() ? std::max(val, span) : val + span;
			}
			return {val + get_padding().width(), true};
		}
		/// Returns the minimum height that can contain all elements in pixels, plus padding. All heights specified
		/// in proportions are ignored.
		///
		/// \sa get_desired_width
		std::pair<double, bool> get_desired_height() const override {
			double val = 0.0;
			for (element *e : _children.items()) {
				double span = _get_vertical_absolute_span(*e);
				val = is_vertical() ? val + span : std::max(val, span);
			}
			return {val + get_padding().height(), true};
		}

		/// Calculates the layout of a list of elements as if they were in a \ref stack_panel with the given
		/// orientation and client area. All elements must be children of the given \ref panel_base.
		template <bool Vertical> inline static void layout_elements_in(
			rectd client, const std::vector<element*> &elems
		) {
			if constexpr (Vertical) {
				_layout_elements_in_impl<
					true, &element::_recalc_horizontal_layout,
					&rectd::ymin, &rectd::ymax, &rectd::xmin, &rectd::xmax
				>(client, elems);
			} else {
				_layout_elements_in_impl<
					false, &element::_recalc_vertical_layout,
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
		inline static str_t get_default_class() {
			return CP_STRLIT("stack_panel");
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

			/// Extracts the information corresponding to the given orientation from the given element.
			template <bool Vertical> inline static _elem_layout_info extract(const element &e) {
				_elem_layout_info res;
				thickness margin = e.get_margin();
				anchor anc = e.get_anchor();
				if constexpr (Vertical) {
					res.margin_min = {margin.top, test_bits_any(anc, anchor::top)};
					res.margin_max = {margin.bottom, test_bits_any(anc, anchor::bottom)};
					res.size = e.get_layout_height();
				} else {
					res.margin_min = {margin.left, test_bits_any(anc, anchor::left)};
					res.margin_max = {margin.right, test_bits_any(anc, anchor::right)};
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
			void (element::*CalcDefaultDir)(double, double),
			double rectd::*MainMin, double rectd::*MainMax,
			double rectd::*DefMin, double rectd::*DefMax
		> inline static void _layout_elements_in_impl(rectd client, const std::vector<element*> &elems) {
			std::vector<_elem_layout_info> layoutinfo;
			double total_prop = 0.0, total_px = 0.0;
			for (element *e : elems) {
				(e->*CalcDefaultDir)(client.*DefMin, client.*DefMax);
				_elem_layout_info info = _elem_layout_info::extract<Vertical>(*e);
				(info.margin_min.second ? total_px : total_prop) += info.margin_min.first;
				(info.size.second ? total_px : total_prop) += info.size.first;
				(info.margin_max.second ? total_px : total_prop) += info.margin_max.first;
				layoutinfo.emplace_back(info);
			}
			// distribute the remaining space
			double prop_mult = (client.*MainMax - client.*MainMin - total_px) / total_prop, pos = client.*MainMin;
			auto it = layoutinfo.begin();
			for (element *e : elems) {
				const _elem_layout_info &info = *it;
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
				_child_set_layout(*e, nl);
				++it;
			}
		}

		/// Calls \ref layout_elements_in to calculate the layout of all children.
		void _finish_layout() override {
			layout_elements_in(
				get_client_region(),
				std::vector<element*>(_children.items().begin(), _children.items().end()),
				is_vertical()
			);
			panel::_finish_layout();
		}

		/// Invalidates the children's layout since it is determined by their ordering.
		void _on_child_order_changed(element&) override {
			revalidate_layout();
		}

		/// Sets \ref _can_focus to \p false by default.
		void _initialize(const str_t &cls, const element_metrics &metrics) override {
			panel::_initialize(cls, metrics);
			_can_focus = false;
		}

		/// Invalidates the element's layout if the element's orientation has been changed.
		void _on_state_changed(value_update_info<element_state_id>&) override;
	};
}
