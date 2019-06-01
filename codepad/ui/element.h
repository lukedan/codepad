// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of elements, the basic component of the user interface.

#include <list>
#include <stack>
#include <any>

#include "../apigen_definitions.h"

#include "../core/encodings.h"
#include "../core/event.h"
#include "../core/misc.h"
#include "../os/misc.h"
#include "animation_path.h"
#include "hotkey_registry.h"
#include "element_classes.h"
#include "renderer.h"

namespace codepad::ui {
	class panel;
	class manager;
	class scheduler;
	class window_base;

	/// Used to transform mouse position between the coordinate systems of a hierarchy of elements.
	struct mouse_position {
		friend window_base;
	public:
		/// Returns the mouse position relative to the specified \ref element.
		vec2d get(element&) const;
	protected:
		size_t _timestamp = 0; ///< The timestamp of the corresponding mouse event.

		/// Initializes \ref _timestamp.
		explicit mouse_position(size_t ts) : _timestamp(ts) {
		}
	};

	/// Contains information about mouse movement.
	struct mouse_move_info {
	public:
		/// Constructor.
		explicit mouse_move_info(mouse_position m) : new_position(m) {
		}

		/// The position that the mouse has moved to, relative to the top-left corner of the window's client area.
		const mouse_position new_position;
	};
	/// Contains information about mouse scrolling.
	struct mouse_scroll_info {
	public:
		/// Constructor.
		mouse_scroll_info(vec2d d, mouse_position pos) : delta(d), position(pos) {
		}

		const vec2d delta; ///< The offset of the mouse scroll.
		/// The position of the mouse when the scroll took place, relative to the top-left corner of the window's
		/// client area.
		const mouse_position position;

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
	public:
		/// Constructor.
		mouse_button_info(mouse_button mb, modifier_keys mods, mouse_position pos) :
			button(mb), modifiers(mods), position(pos) {
		}

		const mouse_button button; ///< The mouse button that the user has pressed.
		const modifier_keys modifiers; ///< The modifiers that are pressed.
		const mouse_position position; ///< The position of the mouse when the event took place.

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
		explicit key_info(key k) : key_pressed(k) {
		}
		const key key_pressed; ///< The key that has been pressed.
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

	/// Defines commonly used Z-index values.
	namespace zindex {
		constexpr static int
			background = -10, ///< Z-index of those elements that are supposed to be below regular ones.
			normal = 0, ///< Default value for all elements.
			foreground = 10, ///< Z-index of those elements that are supposed to be above regular ones.
			overlay = 20, ///< Z-index of those elements that should be always on top.
			max_value = std::numeric_limits<int>::max(), ///< Maximum possible Z-index.
			min_value = std::numeric_limits<int>::min(); ///< Minimum possible Z-index.
	}

	/// The base class of elements of the user interface.
	class APIGEN_EXPORT_RECURSIVE APIGEN_EXPORT_BASE element {
		APIGEN_ENABLE_PRIVATE_EXPORT;
		friend class element_collection;
		friend class window_base;
		friend manager;
		friend scheduler;
		friend panel;
		friend class_arrangements;
		friend mouse_position;
		friend animation_path::builder::getter_components::element_parameters_getter_component;
	public:
		/// Default virtual destrucor.
		virtual ~element() = default;

		/// Returns the parent element.
		panel *parent() const {
			return _parent;
		}
		/// Returns the element's logical parent.
		panel *logical_parent() const {
			return _logical_parent;
		}

		/// Returns the current layout of this element.
		rectd get_layout() const {
			return _layout;
		}
		/// Calculates and returns the current client region, i.e.,
		/// the layout with padding subtracted from it, of the element.
		rectd get_client_region() const {
			return get_padding().shrink(_layout);
		}
		/// Returns the width value used for layout calculation. If the current width allocation type is
		/// \ref size_allocation_type::automatic, the result will be that of \ref get_desired_width; otherwise the
		/// user-defined width will be returned.
		size_allocation get_layout_width() const {
			size_allocation_type type = get_width_allocation();
			if (type == size_allocation_type::automatic) {
				return get_desired_width();
			}
			return size_allocation(get_size().x, type == size_allocation_type::fixed);
		}
		/// Returns the height value used for layout calculation.
		///
		/// \sa get_layout_width
		size_allocation get_layout_height() const {
			size_allocation_type type = get_height_allocation();
			if (type == size_allocation_type::automatic) {
				return get_desired_height();
			}
			return size_allocation(get_size().y, type == size_allocation_type::fixed);
		}

		/// Returns the margin metric of this element.
		thickness get_margin() const {
			return _params.layout_parameters.margin;
		}
		/// Returns the padding metric of this element.
		thickness get_padding() const {
			return _params.layout_parameters.padding;
		}
		/// Returns the size metric of this element. Note that this is not the element's actual size, and this value
		/// may or may not be used in layout calculation.
		///
		/// \sa get_actual_size()
		vec2d get_size() const {
			return _params.layout_parameters.size;
		}
		/// Returns the anchor metric of this element.
		anchor get_anchor() const {
			return _params.layout_parameters.elem_anchor;
		}
		/// Returns the width allocation metric of this element.
		size_allocation_type get_width_allocation() const {
			return _params.layout_parameters.width_alloc;
		}
		/// Returns the height allocation metric of this element.
		size_allocation_type get_height_allocation() const {
			return _params.layout_parameters.height_alloc;
		}

		/// Returns the desired width of the element. Derived elements can override this to change the default
		/// behavior, which simply makes the element fill all available space horizontally.
		///
		/// \return A \p std::pair<double, bool>, in which the first element is the value and the second element
		///         indicates whether the value is specified in pixels.
		virtual size_allocation get_desired_width() const {
			return size_allocation(1.0, false);
		}
		/// Returns the desired height of the element. Derived elements can override this to change the default
		/// behavior, which simply makes the element fill all available space vertically.
		///
		/// \return A \p std::pair<double, bool>, in which the first element is the value and the second element
		///         indicates whether the value is specified in pixels.
		virtual size_allocation get_desired_height() const {
			return size_allocation(1.0, false);
		}

		/// Used to test if a given point lies in the element.
		/// Derived classes can override this function to create elements with more complex shapes.
		virtual bool hit_test(vec2d p) const {
			return p.x > 0.0 && p.x < _layout.width() && p.y > 0.0 && p.y < _layout.height();
		}


		/// Returns the default cursor of the element. Derived classes can override this function to change
		/// the default behavior.
		virtual cursor get_default_cursor() const {
			return cursor::normal;
		}
		/// Returns the custom cursor displayed when the mouse is over this element.
		cursor get_custom_cursor() const {
			return _params.custom_cursor;
		}
		/// Returns the cursor displayed when the mouse is over this element.
		/// This function returns the cusrom cursor if there is one,
		/// or the result returned by \ref get_default_cursor.
		virtual cursor get_current_display_cursor() const {
			cursor overriden = get_custom_cursor();
			if (overriden == cursor::not_specified) {
				return get_default_cursor();
			}
			return overriden;
		}

		/// Returns the window that contains the element, or \p nullptr if the element's not currently attached to
		/// one.
		window_base *get_window();
		/// Returns the \ref manager of this element. Use this instead of directly accessing \ref _manager.
		manager &get_manager() const {
			return *_manager;
		}

		/// Sets the z-index of the element.
		void set_zindex(int);
		/// Returns the z-index of the element.
		int get_zindex() const {
			return _zindex;
		}

		/// Sets the visibility of this element.
		void set_visibility(visibility v) {
			value_update_info<visibility> info(get_visibility());
			_params.visibility = v;
			_on_visibility_changed(info);
		}
		/// Returns the current visibility of this element.
		visibility get_visibility() const {
			return _params.visibility;
		}
		/// Tests if the visiblity of this element have all the given visiblity flags set.
		bool is_visible(visibility vis) const {
			return (get_visibility() & vis) == vis;
		}

		/// Returns if the mouse is hovering over this element.
		bool is_mouse_over() const {
			return _mouse_over;
		}

		/// Returns the current generic parameters of this element.
		const element_parameters &get_parameters() const {
			return _params;
		}


		/// Invalidates the visual of the element so that it'll be re-rendered next frame.
		///
		/// \sa manager::invalidate_visual()
		void invalidate_visual();
		/// Invalidates the layout of the element so that its parent will be notified and will recalculate its
		/// layout.
		///
		/// \sa manager::invalid_layout()
		void invalidate_layout();


		info_event<>
			mouse_enter, ///< Triggered when the mouse starts to be over the element.
			mouse_leave, ///< Triggered when the mouse ceases to be over the element.
			got_focus, ///< Triggered when the element gets the focus.
			lost_focus, ///< Triggered when the element loses the focus.
			lost_capture, ///< Triggered when the element loses mouse capture.
			layout_changed; ///< Triggered when the layout of this element has changed.
		info_event<mouse_move_info> mouse_move; ///< Triggered when the mouse moves over the element.
		info_event<mouse_button_info>
			mouse_down, ///< Triggered when a mouse button is pressed when the mouse is over the element.
			mouse_up; ///< Triggered when a mouse button is released when the mouse is over the elemnet.
		info_event<mouse_scroll_info> mouse_scroll; ///< Triggered when the user scrolls the mouse over the element.
		info_event<key_info>
			key_down, ///< Triggered when a key is pressed when the element has the focus.
			key_up; ///< Triggered when a key is released when the element has the focus.
		info_event<text_info> keyboard_text; ///< Triggered as the user types when the element has the focus.

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("element");
		}
	private:
		element_parameters _params; ///< The parameters of this element.
		const class_hotkey_group *_hotkeys = nullptr; ///< The hotkey group of this element.
		int _zindex = zindex::normal; ///< The z-index of the element.

		panel
			*_parent = nullptr, ///< Pointer to the element's parent.
			/// The element's logical parent. In composite elements this points to the top level composite element.
			/// Composite elements that allow users to dynamically add children may also use this to indicate the
			/// actual element that handles their logic.
			*_logical_parent = nullptr;
		std::any _parent_data; ///< Data generated and used by the parent.

		vec2d _cached_mouse_position; ///< Cached mouse position relative to this elemnt.
		/// The timestamp used to check if \ref _cached_mouse_position is valid.
		size_t _cached_mouse_position_timestamp = 0;
		bool _mouse_over = false; ///< Indicates if the mouse is hoverihg this element.
	protected:
		rectd _layout; ///< The absolute layout of the element in the window.

		/// Called when the mouse starts to be over the element.
		/// Updates \ref _mouse_over and invokes \ref mouse_enter.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_enter() {
			_mouse_over = true;
			mouse_enter.invoke();
		}
		/// Called when the mouse ceases to be over the element.
		/// Updates \ref _mouse_over and invokes \ref mouse_leave.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_leave() {
			_mouse_over = false;
			mouse_leave.invoke();
		}
		/// Called when the element gets the focus. Invokes \ref got_focus.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_got_focus() {
			got_focus.invoke();
		}
		/// Called when the element loses the focus. Invokes \ref lost_focus.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_lost_focus() {
			lost_focus.invoke();
		}
		/// Called when the mouse moves over the element. Invokes \ref mouse_move.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_move(mouse_move_info & p) {
			mouse_move.invoke(p);
		}
		/// Called when a mouse button is pressed when the mouse is over this element.
		/// Changes the focus if appropriate, and invokes \ref mouse_down.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_down(mouse_button_info&);
		/// Called when a mouse button is released when the mouse is over this element. Invokes \ref mouse_up.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_up(mouse_button_info & p) {
			mouse_up.invoke(p);
		}
		/// Called when the user scrolls the mouse over the element. Invokes \ref mouse_scroll.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_scroll(mouse_scroll_info & p) {
			mouse_scroll.invoke(p);
		}
		/// Called when a key is pressed when the element has the focus. Invokes \ref key_down.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_key_down(key_info & p) {
			key_down.invoke(p);
		}
		/// Called when a key is released when the element has the focus. Invokes \ref key_up.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_key_up(key_info & p) {
			key_up.invoke(p);
		}
		/// Called when the users types characters when the element has the focus. Invokes \ref keyboard_text.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_keyboard_text(text_info & p) {
			keyboard_text.invoke(p);
		}
		/// Called when the state of the ongoing composition has changed. Derived classes can override this
		/// function to display the composition string, for example.
		virtual void _on_composition(composition_info&) {
		}
		/// Called when the ongoing composition has finished.
		virtual void _on_composition_finished() {
		}

		/// Called after the element is added to a \ref panel. This method is invoked after
		/// \ref panel::_on_child_added() and \ref element_collection::changed.
		virtual void _on_added_to_parent() {
		}
		/// Called when the element is about to be removed from a \ref panel. This method is invoked before
		/// \ref panel::_on_child_removing() and \ref element_collection::changing.
		virtual void _on_removing_from_parent() {
		}

		/// Called when the padding of the element has changed. Calls \ref invalidate_visual.
		virtual void _on_padding_changed() {
			invalidate_visual();
		}
		/// Called when the visibility of this element has changed.
		virtual void _on_visibility_changed(value_update_info<visibility> & p) {
			visibility changed = p.old_value ^ get_visibility();
			if ((changed & visibility::layout) != visibility::none) {
				invalidate_layout();
			} else if ((changed & visibility::visual) != visibility::none) {
				invalidate_visual();
			}
			// TODO handle visibility::focus
		}

		/// Called when the element has lost the capture it previously got. Note that
		/// \ref os::window::set_mouse_capture should not be called in this handler.
		virtual void _on_capture_lost() {
		}

		/// Called when the \ref manager updates all elements that have been registered. Updates can be scheduled for
		/// various reasons, so derived classes should check for additional conditions when performing updates.
		/// \ref _config is updated here by default, and \ref invalidate_visual is called when necessary.
		virtual void _on_update() {
		}

		/// Called when the element is about to be rendered.
		/// Pushes a clip that prevents anything to be drawn outside of its layout.
		virtual void _on_prerender();
		/// Called when the element is rendered. Renders all visual geometries.
		/// Derived classes should override this function to perform custom rendering.
		virtual void _custom_render() const;
		/// Called after the element has been rendered. Pops the clip pushed in \ref _on_prerender.
		virtual void _on_postrender();
		/// Renders the element if the element does not have \ref manager::predefined_states::render_invisible state.
		/// This function first calls \ref _on_prerender, then updates \ref _state and renders the background, calls
		/// \ref _custom_render, and finally calls \ref _on_postrender.
		virtual void _on_render();

		/// Called by the element itself when its desired size has changed. It should be left for the parent to
		/// decide whether it should invalidate its own layout or call \ref invalidate_layout() on this child.
		///
		/// \param width Whether the width of the desired size has changed.
		/// \param height Whether the height of the desired size has changed.
		void _on_desired_size_changed(bool width, bool height);
		/// Called by \ref manager when the layout has changed. Calls \ref invalidate_visual. Derived classes can
		/// override this to update layout-dependent properties. For panels, override
		/// \ref panel::_on_update_children_layout() instead when re-calculating the layout of its children.
		virtual void _on_layout_changed() {
			if (
				std::isnan(get_layout().xmin) || std::isnan(get_layout().xmax) ||
				std::isnan(get_layout().ymin) || std::isnan(get_layout().ymax)
				) {
				logger::get().log_warning(CP_HERE, "layout system produced nan on ", demangle(typeid(*this).name()));
			}
			invalidate_visual();
			layout_changed.invoke();
		}

		/// Registers the callback to the event if the names match. The callback will be moved out of place if it's
		/// registered.
		///
		/// \return Whether the names match or not.
		template <typename Info> bool _try_register_event(
			str_view_t name, str_view_t expected, info_event<Info> & event, std::function<void()> & callback
		) {
			if (name == expected) {
				if constexpr (std::is_same_v<Info, void>) { // register directly
					event += std::move(callback);
				} else { // ignore the arguments
					event += [cb = std::move(callback)](Info&) {
						cb();
					};
				}
				return true;
			}
			return false;
		}
		/// Registers the callback for the event with the given name. This is used mainly for storyboard animations.
		virtual void _register_event(str_view_t name, std::function<void()> callback) {
			_try_register_event(name, u8"mouse_enter", mouse_enter, callback);
			_try_register_event(name, u8"mouse_leave", mouse_leave, callback);
			_try_register_event(name, u8"got_focus", got_focus, callback);
			_try_register_event(name, u8"lost_focus", lost_focus, callback);
			_try_register_event(name, u8"mouse_down", mouse_down, callback);
			_try_register_event(name, u8"mouse_up", mouse_up, callback);
		}

		/// Sets a custom attribute for this element.
		virtual void _set_attribute(str_view_t name, const json::value_storage & v) {
			if (name == u8"z_index") {
				std::int32_t z;
				if (json::try_cast(v.get_value(), z)) {
					set_zindex(z);
				}
				return;
			}
		}

		/// Called immediately after the element is created to initialize it. Initializes \ref _config with the given
		/// class. All derived classes should call \p base::_initialize <em>before</em> performing any other
		/// initialization. It is primarily intended to avoid pitfalls associated with virtual function calls in
		/// constructors to have this function.
		///
		/// \param cls The class of the element.
		/// \param metrics The element's metrics configuration.
		virtual void _initialize(str_view_t cls, const element_configuration & config);
		/// Called after the logical parent of this element (which is a composite element) has been fully constructed,
		/// i.e., it and all of its children (including this element) has been constructed and properly initialized.
		/// If this element does not have a logical parent, this function will not be called. The order in which this
		/// is called for all descendents is arbitrary.
		virtual void _on_logical_parent_constructed() {
		}
		/// Called immediately before the element is disposed. Removes the element from its parent if it has one.
		/// All derived classes should call \p base::_dispose <em>after</em> disposing of their own components.
		/// It is primarily intended to avoid pitfalls associated with virtual function calls in destructors to have
		/// this function.
		virtual void _dispose();
	private:
		/// The \ref manager of this element. This is set after this element is constructed but before
		/// \ref _initialize() is called.
		manager *_manager = nullptr;
#ifdef CP_CHECK_USAGE_ERRORS
		bool _initialized = false; ///< Indicates wheter the element has been properly initialized and disposed.
#endif
	};

	/// A decoration that is rendered above all elements. The user cannot interact with the decoration in any way.
	/// If the state of the decoration contains the bit visual::predefined_states::corpse, then the decoration will
	/// be automatically disposed when inactive if it's registered to a window. Decorations can be created with
	/// <tt>new decoration()</tt>. When deleted, the decoration will automatically be unregistered from the window
	/// it's currently registered to.'
	class decoration {
		friend class window_base;
	public:
		/// Initializes this decoration with the corresponding \ref manager.
		explicit decoration(manager &man) : _manager(man) {
		}
		/// No move construction.
		decoration(decoration&&) = delete;
		/// Copy constructor. The decoration will have the same class, state, manager, etc. of the original
		/// decoration, only that it won't be registered to any window.
		decoration(const decoration &src) :
			_class(src._class), _layout(src._layout), _manager(src._manager) {
		}
		/// Destructor. If \ref _wnd is not \p nullptr, notifies the window of its disposal.
		~decoration();

		/// Sets the layout of the decoration in the window.
		void set_layout(rectd);
		/// Returns the layout of the decoration.
		rectd get_layout() const {
			return _layout;
		}

		/// Sets the class used to render this decoration.
		void set_class(const str_t&);
		/// Returns the class used to render the decoration.
		const str_t &get_class() const {
			return _class;
		}

		/// Returns the os::window_base that the decoration is currently registered to.
		window_base *get_window() const {
			return _wnd;
		}
	protected:
		str_t _class; ///< The decoration's class.
		rectd _layout; ///< The layout of the decoration.
		/// A token used to accelerate the insertion and removal of decorations.
		std::list<decoration*>::const_iterator _tok;
		window_base *_wnd = nullptr; ///< The window that the decoration is currently registered to.
		manager &_manager; ///< The \ref manager of this decoration.
	};
}
