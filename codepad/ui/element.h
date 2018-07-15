#pragma once

/// \file
/// Implementation of elements, the basic component of the user interface.

#include <list>

#include "../core/hotkey_registry.h"
#include "../core/encodings.h"
#include "../core/event.h"
#include "../core/misc.h"
#include "../os/renderer.h"
#include "../os/misc.h"
#include "element_classes.h"
#include "visual.h"
#include "draw.h"

namespace codepad::ui {
	class panel_base;

	/// Contains information about mouse movement.
	struct mouse_move_info {
		/// Constructor.
		explicit mouse_move_info(vec2d p) : new_pos(p) {
		}
		/// The position that the mouse has moved to, relative to the top-left corner of the window's client area.
		const vec2d new_pos;
	};
	/// Contains information about mouse scrolling.
	struct mouse_scroll_info {
		/// Constructor.
		mouse_scroll_info(double d, vec2d pos) : offset(d), position(pos) {
		}
		const double offset; ///< The offset of the mouse scroll.
		/// The position of the mouse when the scroll took place, relative to the top-left corner of the window's
		/// client area.
		const vec2d position;
		/// Returns \p true if the scroll has been handled by an element.
		bool handled() const {
			return _handled;
		}
		/// Marks this event as handled.
		void mark_handled() {
			assert_true_usage(!_handled, "event is being marked as handled twice");
			_handled = true;
		}
	protected:
		bool _handled = false; ///< Marks if the event has been handled by an element.
	};
	/// Contains information about mouse clicks.
	struct mouse_button_info {
		/// Constructor.
		mouse_button_info(os::input::mouse_button mb, modifier_keys mods, vec2d pos) :
			button(mb), modifiers(mods), position(pos) {
		}
		const os::input::mouse_button button; ///< The mouse button that the user has pressed.
		const modifier_keys modifiers; ///< The modifiers that are pressed.
		const vec2d position; ///< The position of the mouse when the event took place.
		/// Returns \p true if the click has caused the focused element to change.
		bool focus_set() const {
			return _focus_set;
		}
		/// Marks that the focused element has changed.
		void mark_focus_set() {
			assert_true_usage(!_focus_set, "focus is being changed twice");
			_focus_set = true;
		}
	protected:
		bool _focus_set = false; ///< Marks if the click has caused the focused element to change.
	};
	/// Contains information about key presses.
	struct key_info {
		/// Constructor.
		explicit key_info(os::input::key k) : key(k) {
		}
		const os::input::key key; ///< The key that has been pressed.
	};
	/// Contains information about text input.
	struct text_info {
		/// Constructor.
		explicit text_info(str_t s) : content(std::move(s)) {
		}
		const str_t content; ///< The content that the user has entered.
	};
	/// Contains information about the ongoing composition.
	///
	/// \todo Add composition underline.
	struct composition_info {
		/// Constructor.
		explicit composition_info(str_t s) : composition_string(std::move(s)) {
		}
		const str_t composition_string; ///< The current composition string.
	};
#ifdef CP_CHECK_LOGICAL_ERRORS
	/// Keeps track of the number of elements created and disposed,
	/// to verify that all created elements are properly disposed.
	struct control_disposal_rec {
		/// Destructor. Verifies that all elements are properly disposed.
		~control_disposal_rec() {
			assert_true_logical(
				reg_created == disposed && reg_disposed == disposed,
				"undisposed controls remaining"
			);
		}
		size_t
			reg_created = 0, ///< The number of elements created.
			disposed = 0, ///< The number of elements that has been disposed.
			reg_disposed = 0; ///< The number of elements that has been properly registered as disposed.

		/// Returns the global instance.
		static control_disposal_rec &get();
	};
#endif

	/// The base class of elements of the user interface.
	class element {
		friend class manager;
		friend class element_collection;
		friend class panel_base;
		friend class os::window_base;
		friend class visual_state;
		friend class visual;
	public:
		constexpr static int
			max_zindex = std::numeric_limits<int>::max(), ///< The maximum possible z-index.
			min_zindex = std::numeric_limits<int>::min(); ///< The minimum possible z-index.

		/// Virtual destrucor.
		virtual ~element() {
#ifdef CP_CHECK_LOGICAL_ERRORS
			++control_disposal_rec::get().disposed;
#endif
		}

		/// Returns the current layout of this element.
		rectd get_layout() const {
			return _layout;
		}
		/// Calculates and returns the current client region, i.e.,
		/// the layout with padding subtracted from it, of the element.
		rectd get_client_region() const {
			return _padding.shrink(_layout);
		}

		/// Returns the parent element.
		panel_base *parent() const {
			return _parent;
		}

		/// Sets the width of the element in pixels.
		void set_width_pixels(double w) {
			_width = w;
			_wtype = size_allocation_type::fixed;
			invalidate_layout();
		}
		/// Sets the height of the element in pixels.
		void set_height_pixels(double h) {
			_height = h;
			_htype = size_allocation_type::fixed;
			invalidate_layout();
		}
		/// Sets the size of the element in pixels.
		void set_size_pixels(vec2d s) {
			_width = s.x;
			_height = s.y;
			_wtype = _htype = size_allocation_type::fixed;
			invalidate_layout();
		}

		/// Sets the width of the element as a proportion.
		void set_width_proportion(double w) {
			_width = w;
			_wtype = size_allocation_type::proportion;
			invalidate_layout();
		}
		/// Sets the height of the element as a proportion.
		void set_height_proportion(double h) {
			_height = h;
			_htype = size_allocation_type::proportion;
			invalidate_layout();
		}
		/// Sets the size of the element as a proportion.
		void set_size_proportion(vec2d s) {
			_width = s.x;
			_height = s.y;
			_wtype = _htype = size_allocation_type::proportion;
			invalidate_layout();
		}

		/// Sets the \ref size_allocation_type of width to \ref size_allocation_type::automatic.
		void unset_width() {
			_wtype = size_allocation_type::automatic;
			invalidate_layout();
		}
		/// Sets the \ref size_allocation_type of height to \ref size_allocation_type::automatic.
		void unset_height() {
			_htype = size_allocation_type::automatic;
			invalidate_layout();
		}
		/// Sets the \ref size_allocation_type of both width and height to \ref size_allocation_type::automatic.
		void unset_size() {
			_wtype = _htype = size_allocation_type::automatic;
			invalidate_layout();
		}

		/// Returns the \ref size_allocation_type for width.
		size_allocation_type get_width_allocation_type() const {
			return _wtype;
		}
		/// Returns the \ref size_allocation_type for height.
		size_allocation_type get_height_allocation_type() const {
			return _htype;
		}

		/// Returns the desired width of the element. Derived elements can override this to change the default
		/// behavior, which simply makes the element fill all available space horizontally.
		///
		/// \return A \p std::pair<double, bool>, in which the first element is the value and the second element
		///         indicates whether the value is specified in pixels.
		virtual std::pair<double, bool> get_desired_width() const {
			return {1.0, false};
		}
		/// Returns the desired height of the element. Derived elements can override this to change the default
		/// behavior, which simply makes the element fill all available space vertically.
		///
		/// \return A \p std::pair<double, bool>, in which the first element is the value and the second element
		///         indicates whether the value is specified in pixels.
		virtual std::pair<double, bool> get_desired_height() const {
			return {1.0, false};
		}
		/// Returns the width value used for layout calculation. If \ref _wtype is
		/// \ref size_allocation_type::automatic, the result will be that of \ref get_desired_width; otherwise the
		/// user-defined width will be returned.
		std::pair<double, bool> get_layout_width() const {
			if (_wtype == size_allocation_type::automatic) {
				return get_desired_width();
			}
			return {_width, _wtype == size_allocation_type::fixed};
		}
		/// Returns the height value used for layout calculation.
		///
		/// \sa get_layout_width
		std::pair<double, bool> get_layout_height() const {
			if (_htype == size_allocation_type::automatic) {
				return get_desired_height();
			}
			return {_height, _htype == size_allocation_type::fixed};
		}
		/// Returns the actual size, calculated from the actual layout.
		vec2d get_actual_size() const {
			return _layout.size();
		}

		/// Sets the margin of the element. The margin determines how much space there should
		/// be around the element, together with the anchor of the element.
		void set_margin(thickness t) {
			_margin = t;
			invalidate_layout();
		}
		/// Returns the margin of the element.
		thickness get_margin() const {
			return _margin;
		}
		/// Sets the internal padding of the element.
		void set_padding(thickness t) {
			_padding = t;
			_on_padding_changed();
		}
		/// Returns the internal padding of the element.
		thickness get_padding() const {
			return _padding;
		}

		/// Sets the anchor of the element. If the anchor on a side is set, then the corresponding margin
		/// is treated to be in pixels, otherwise it's treated as a proportion.
		void set_anchor(anchor a) {
			_anchor = a;
			invalidate_layout();
		}
		/// Returns the anchor of the element.
		anchor get_anchor() const {
			return static_cast<anchor>(_anchor);
		}

		/// Used to test if a given point lies in the element.
		/// Derived classes can override this function to create elements with more complex shapes.
		virtual bool hit_test(vec2d p) const {
			return _layout.contains(p);
		}

		/// Returns the default cursor of the element. Derived classes can override this function to change
		/// the default behavior.
		virtual os::cursor get_default_cursor() const {
			return os::cursor::normal;
		}
		/// Sets the custom cursor displayed when the mouse is over this element.
		/// If the given cursor is os::cursor::not_specified, then the element will have no custom cursor.
		void set_overriden_cursor(os::cursor c) {
			_crsr = c;
		}
		/// Returns the custom cursor displayed when the mouse is over this element.
		os::cursor get_overriden_cursor() const {
			return _crsr;
		}
		/// Returns the cursor displayed when the mouse is over this element.
		/// This function returns the cusrom cursor if there is one,
		/// or the result returned by \ref get_default_cursor.
		virtual os::cursor get_current_display_cursor() const {
			if (_crsr == os::cursor::not_specified) {
				return get_default_cursor();
			}
			return _crsr;
		}

		/// Sets if this element can have the focus.
		void set_can_focus(bool v) {
			_can_focus = v;
		}
		/// Returns whether the element can have the focus.
		bool get_can_focus() const {
			return _can_focus;
		}

		/// Returns the window that contains the element, and \p nullptr otherwise.
		os::window_base *get_window();

		/// Sets the z-index of the element.
		void set_zindex(int);
		/// Returns the z-index of the element.
		int get_zindex() const {
			return _zindex;
		}

		/// Sets the class of the element. The class is used when
		/// displaying the element and processing hotkeys.
		void set_class(str_t s) {
			_rst.set_class(std::move(s));
			invalidate_visual();
		}
		/// Returns the current class of the element.
		const str_t &get_class() const {
			return _rst.get_class();
		}
		/// Returns the state of this element.
		element_state_id get_state() const {
			return _rst.get_state();
		}

		/// Invalidates the visual of the element so that it'll be re-rendered next frame.
		///
		/// \sa manager::invalidate_visual
		void invalidate_visual();
		/// Invalidates the layout of the element so that its layout will be re-calculated next frame.
		/// This will cause \ref _recalc_layout and \ref _finish_layout to be called.
		///
		/// \sa manager::invalid_layout
		void invalidate_layout();
		/// Used by parent elements when calculating layout, to indicate that the parent overrides
		/// the layout of this element and has finished calculating it. This will only cause \ref _finish_layout
		/// to be called.
		///
		/// \sa manager::revalidate_layout
		void revalidate_layout();

		/// Allocates and creates an element of the given type with the given arguments.
		///
		/// \tparam T The type of the element to create.
		template <typename T> inline static T *create() {
			static_assert(std::is_base_of_v<element, T>, "cannot create non-elements");
			element *elem = new T();
#ifdef CP_CHECK_LOGICAL_ERRORS
			++control_disposal_rec::get().reg_created; // register its creation
#endif
			elem->_initialize();
#ifdef CP_CHECK_USAGE_ERRORS
			assert_true_usage(elem->_initialized, "element::_initialize() must be called by derived classes");
#endif
			elem->set_class(T::get_default_class()); // set the class to its default
			return static_cast<T*>(elem);
		}

		/// Shorthand for testing if the \ref predefined_states::mouse_over bit is set in the element's states. Note
		/// that this function may not be accurate when, e.g., the mouse is captured.
		///
		/// \sa hit_test
		bool is_mouse_over() const;
		/// Shorthand for testing if the \ref predefined_states::invisible bit is \em not set in the element's
		/// states.
		bool is_visible() const;
		/// Shorthand for setting or unsetting the \ref predefined_states::invisible state.
		void set_visibility(bool);
		/// Shorthand for testing if the \ref predefined_states::ghost bit is \em not set in the element's states.
		bool is_interactive() const;
		/// Shorthand for setting or unsetting the \ref predefined_states::ghost state.
		void set_is_interactive(bool);
		/// Shorthand for testing if the \ref predefined_states::focused bit is \em not set in the element's states.
		/// The result may differ from that from testing if \p this is equal to \ref manager::get_focused_element on
		/// certain occasions, more specifically when the focused element is changing.
		bool is_focused() const;

		/// Calculates the layout of an element on a direction (horizontal or vertical) with the given parameters.
		/// If all of \p anchormin, \p pixelsize, and \p anchormax are \p true, all sizes are respected and the extra
		/// space is distributed evenly between before and after the client region.
		///
		/// \param anchormin \p true if the element is anchored towards the `negative' (left or top) direction.
		/// \param pixelsize \p true if the size of the element is specified in pixels.
		/// \param anchormax \p true if the element is anchored towards the `positive' (right or bottom) direction.
		/// \param clientmin Passes the minimum (left or top) boundary of the client region,
		///        and will contain the minimum boundary of the element's layout as a return value.
		/// \param clientmax Passes the maximum (right or bottom) boundary of the client region,
		///        and will contain the maximum boundary of the element's layout as a return value.
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
			// prioritize size in pixels
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

		event<void>
			mouse_enter, ///< Triggered when the mouse starts to be over the element.
			mouse_leave, ///< Triggered when the mouse ceases to be over the element.
			got_focus, ///< Triggered when the element gets the focus.
			lost_focus; ///< Triggered when the element loses the focus.
		event<mouse_move_info> mouse_move; ///< Triggered when the mouse moves over the element.
		event<mouse_button_info>
			mouse_down, ///< Triggered when a mouse button is pressed when the mouse is over the element.
			mouse_up; ///< Triggered when a mouse button is released when the mouse is over the elemnet.
		event<mouse_scroll_info> mouse_scroll; ///< Triggered when the user scrolls the mouse over the element.
		event<key_info>
			key_down, ///< Triggered when a key is pressed when the element has the focus.
			key_up; ///< Triggered when a key is released when the element has the focus.
		event<text_info> keyboard_text; ///< Triggered as the user types when the element has the focus.
	protected:
		visual::render_state _rst; ///< The render state that determines how this element is to be rendered.
		rectd _layout; ///< The current layout of the element.
		thickness
			_margin, ///< The margin around the element.
			_padding; ///< The padding inside the element.
		panel_base * _parent = nullptr; ///< Pointer to the element's parent.
		/// A token indicating the place of the element among its parent's children,
		/// used to speed up children insertion and removal.
		std::list<element*>::iterator _col_token;
		double
			_width = 0.0, ///< The custom width, if it has one.
			_height = 0.0; ///< The custom height, if it has one.
		int _zindex = 0; ///< The z-index of the element.
		os::cursor _crsr = os::cursor::not_specified; ///< The custom cursor of the element.
		anchor _anchor = anchor::all; ///< The element's anchor.
		size_allocation_type
			_wtype = size_allocation_type::automatic, ///< Determines how the element's width is computed.
			_htype = size_allocation_type::automatic; ///< Determines how the element's height is computed.
		bool _can_focus = true; ///< Indicates whether this element can have the focus.

		/// Called when the mouse starts to be over the element.
		/// Updates the visual style and invokes \ref mouse_enter.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_enter();
		/// Called when the mouse ceases to be over the element.
		/// Updates the visual style and invokes \ref mouse_leave.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_leave();
		/// Called when the element gets the focus.
		/// Updates the visual style and invokes \ref got_focus.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_got_focus();
		/// Called when the element loses the focus.
		/// Updates the visual style and invokes \ref lost_focus.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_lost_focus();
		/// Called when the mouse moves over the element. Invokes \ref mouse_move.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_move(mouse_move_info &p) {
			mouse_move.invoke(p);
		}
		/// Called when a mouse button is pressed when the mouse is over this element.
		/// Changes the focus and the visual style if appropriate, and invokes \ref mouse_down.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_down(mouse_button_info&);
		/// Called when a mouse button is released when the mouse is over this element.
		/// Updates the visual style and invokes \ref mouse_up.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_up(mouse_button_info&);
		/// Called when the user scrolls the mouse over the element. Invokes \ref mouse_scroll.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_scroll(mouse_scroll_info &p) {
			mouse_scroll.invoke(p);
		}
		/// Called when a key is pressed when the element has the focus. Invokes \ref key_down.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_key_down(key_info &p) {
			key_down.invoke(p);
		}
		/// Called when a key is released when the element has the focus. Invokes \ref key_up.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_key_up(key_info &p) {
			key_up.invoke(p);
		}
		/// Called when the users types characters when the element has the focus. Invokes \ref keyboard_text.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_keyboard_text(text_info &p) {
			keyboard_text.invoke(p);
		}
		/// Called when the state of the ongoing composition has changed. Derived classes can override this
		/// function to display the composition string, for example.
		virtual void _on_composition(composition_info&) {
		}
		/// Called when the ongoing composition has finished.
		virtual void _on_composition_finished() {
		}

		/// Sets a certain bit of the visual style to the given value,
		/// invoking \ref _on_state_changed when necessary.
		void _set_state_bit(element_state_id id, bool v) {
			element_state_id oldv = _rst.get_state();
			if (_rst.set_state_bit(id, v)) {
				value_update_info<element_state_id> info(oldv);
				_on_state_changed(info);
			}
		}

		/// Called when the padding of the element has changed. Calls \ref invalidate_visual.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_padding_changed() {
			invalidate_visual();
		}

		/// Called when the element has lost the capture it previously got. Note that
		/// \ref os::window::set_mouse_capture should not be called in this handler.
		virtual void _on_capture_lost() {
		}

		/// Called when the \ref manager updates all elements that have been registered.
		virtual void _on_update() {
		}

		/// Called when the state of \ref _rst has changed. Calls \ref invalidate_visual.
		virtual void _on_state_changed(value_update_info<element_state_id>&) {
			invalidate_visual();
		}

		/// Called when the element is about to be rendered.
		/// Pushes a clip that prevents anything to be drawn outside of its layout.
		virtual void _on_prerender() {
			os::renderer_base::get().push_clip(_layout.fit_grid_enlarge<int>());
		}
		/// Called when the element is rendered.
		/// Derived classes should override this function to perform custom rendering.
		virtual void _custom_render() {
		}
		/// Called after the element has been rendered. Pops the clip pushed in \ref _on_prerender.
		virtual void _on_postrender() {
			os::renderer_base::get().pop_clip();
		}
		/// If the visibility of the element is set to visibility::visible or visibility::render_only,
		/// renders the element. This function first calls \ref _on_prerender, then updates \ref _rst and
		/// renders the background, calls \ref _custom_render, and finally calls \ref _on_postrender.
		virtual void _on_render();

		/// Called to calculate the horizontal layout of the element, given the area that contains the element.
		virtual void _recalc_horizontal_layout(double, double);
		/// Called to calculate the vertical layout of the element, given the area that contains the element.
		virtual void _recalc_vertical_layout(double, double);
		/// Called by the manager to recalculate the layout of the element, given the area that supposedly contains
		/// the element (usually the client region of its parent). This function simply calls
		/// \ref _recalc_horizontal_layout and \ref _recalc_vertical_layout.
		virtual void _recalc_layout(rectd rgn) {
			_recalc_horizontal_layout(rgn.xmin, rgn.xmax);
			_recalc_vertical_layout(rgn.ymin, rgn.ymax);
		}
		/// Called by the manager when the new layout has been calculated. Calls \ref invalidate_visual.
		/// Derived classes can override this to update what depends on its layout.
		virtual void _finish_layout() {
			invalidate_visual();
		}

		/// Called immediately after the element is created to initialize it.
		/// All derived classes should call \p base::_initialize
		/// <em>before</em> performing any other initialization.
		/// It is primarily intended to avoid pitfalls associated with virtual functions to have this function.
		virtual void _initialize() {
#ifdef CP_CHECK_USAGE_ERRORS
			_initialized = true;
#endif
		}
		/// Called immediately before the element is disposed. Removes the element from its parent if it has one.
		/// All derived classes should call \p base::_dispose <em>after</em> disposing of their own components.
		/// It is primarily intended to avoid pitfalls associated with virtual functions to have this function.
		virtual void _dispose();
#ifdef CP_CHECK_USAGE_ERRORS
	private:
		bool _initialized = false; ///< Indicates wheter the element has been properly initialized and disposed.
#endif
	};

	/// A decoration that is rendered above all elements.
	/// The user cannot interact with the decoration in any way.
	/// If the state of the decoration contains the bit visual::predefined_states::corpse,
	/// then the decoration will be automatically disposed when inactive if it's registered
	/// to a window. All decorations should be created with <tt>new decoration()</tt>.
	/// When deleted, the decoration will automatically be unregistered from the window
	/// it's currently registered to.
	class decoration {
		friend class os::window_base;
	public:
		/// Default constructor.
		decoration() = default;
		/// No move construction.
		decoration(decoration&&) = delete;
		/// Copy constructor. The decoration will have the same layout and render state of the original
		/// decoration, but it won't be registered to any window.
		decoration(const decoration &src) : _layout(src._layout), _st(src._st) {
		}
		/// No move assignment.
		decoration &operator=(decoration&&) = delete;
		/// Copies the layout and render state of the given decoration,
		/// but leaves the registration state unchanged.
		decoration &operator=(const decoration &src) {
			_layout = src._layout;
			_st = src._st;
			return *this;
		}
		/// Destructor. If \ref _wnd is not \p nullptr, notifies the window of its disposal.
		~decoration();

		/// Sets the layout of the decoration in the window.
		void set_layout(rectd r) {
			_layout = r;
			_on_visual_changed();
		}
		/// Returns the layout of the decoration.
		rectd get_layout() const {
			return _layout;
		}

		/// Sets the class used to render the decoration.
		void set_class(str_t cls) {
			_st.set_class(std::move(cls));
			_on_visual_changed();
		}
		/// Returns the class used to render the decoration.
		const str_t &get_class() const {
			return _st.get_class();
		}

		/// Sets the state used to render the decoration.
		void set_state(element_state_id id) {
			_st.set_state(id);
			_on_visual_changed();
		}
		/// Returns the state used to render the decoration.
		element_state_id get_state() const {
			return _st.get_state();
		}

		/// Returns the os::window_base that the decoration is currently registered to.
		os::window_base *get_window() const {
			return _wnd;
		}
	protected:
		rectd _layout; ///< The layout of the decoration.
		visual::render_state _st; ///< Used to render the decoration.
		os::window_base *_wnd = nullptr; ///< The window that the decoration is currently registered to.
		/// A token used to accelerate the insertion and removal of decorations.
		std::list<decoration*>::const_iterator _tok;

		/// Called when the visual of the decoration should be updated.
		/// Calls element::invalidate_visual.
		void _on_visual_changed();
	};
}
