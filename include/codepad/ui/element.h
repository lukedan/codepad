// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of elements, the basic component of the user interface.

#include <list>
#include <stack>
#include <any>

#include "codepad/apigen_definitions.h"

#include "codepad/core/encodings.h"
#include "codepad/core/event.h"
#include "codepad/core/misc.h"
#include "property_path.h"
#include "hotkey_registry.h"
#include "element_classes.h"
#include "renderer.h"
#include "scheduler.h"

namespace codepad::ui {
	class panel;
	class manager;
	class scheduler;
	class window;
	class element_collection;

	/// Used to transform mouse position between the coordinate systems of a hierarchy of elements. This struct does
	/// not take into account factors like hit testing, visibility, etc.
	struct mouse_position {
		friend window;
	public:
		/// Returns the mouse position relative to the specified \ref element.
		vec2d get(element&) const;
	protected:
		static window *_active_window; ///< The window that currently has valid mouse position data.
		static std::size_t _global_timestamp; ///< The global timestamp of the latest mouse position.

		/// Private constructor; only windows can create \ref mouse_position objects.
		mouse_position() = default;
	};

	/// Contains information about mouse movement.
	struct mouse_move_info {
	public:
		/// Constructor.
		explicit mouse_move_info(mouse_position m) : new_position(m) {
		}

		/// The position that the mouse has moved to.
		const mouse_position new_position;
	};
	/// Contains information about mouse hover events.
	struct mouse_hover_info {
		/// Initializes all fields of this struct.
		mouse_hover_info(modifier_keys mods, mouse_position pos) : modifiers(mods), position(pos) {
		}

		const modifier_keys modifiers; ///< Modifier keys that are pressed during the event.
		const mouse_position position; ///< The mouse position of the hover event.
	};
	/// Contains information about mouse scrolling.
	struct mouse_scroll_info {
	public:
		/// Constructor.
		mouse_scroll_info(vec2d d, mouse_position pos, bool smooth) :
			raw_delta(d), position(pos), is_smooth(smooth), _remaining_delta(d) {
		}

		/// The original offset of the mouse scroll.
		const vec2d raw_delta;
		/// The position of the mouse when the scroll took place.
		const mouse_position position;
		const bool is_smooth = false; ///< Whether this scroll operation is smooth scrolling.

		/// Returns the delta that has not been consumed by \ref consume().
		vec2d delta() const {
			return _remaining_delta;
		}
		/// Consumes the delta by the given amount. Warns if too much is consumed.
		void consume(vec2d v) {
			consume_horizontal(v.x);
			consume_vertical(v.y);
		}
		/// Consumes the horizontal delta by the given amount. Warns if too much is consumed.
		void consume_horizontal(double val) {
			_consume_impl(_remaining_delta.x, val, u8"x");
		}
		/// Consumes the vertical delta by the given amount. Warns if too much is consumed.
		void consume_vertical(double val) {
			_consume_impl(_remaining_delta.y, val, u8"y");
		}
	protected:
		vec2d _remaining_delta; ///< The amount of delta that has not been consumed.

		/// Implementation of \ref consume() for one axis.
		void _consume_impl(double &current, double amount, std::u8string_view name) {
			double new_value = current - amount;
			double product = new_value * current;
			if (product < 0.0) {
				if (product < -1e-6) {
					logger::get().log_warning(CP_HERE) <<
						"consuming too much scroll delta on the " << name << "axis" << logger::stacktrace;
				}
				new_value = 0.0;
			}
			current = new_value;
		}
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

		/// Packs \ref button and \ref modifiers as a \ref mouse_gesture.
		[[nodiscard]] mouse_gesture get_gesture() const {
			return mouse_gesture(button, modifiers);
		}

		/// Returns \p true if the click has caused the focused element to change.
		[[nodiscard]] bool is_focus_set() const {
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
		explicit text_info(std::u8string s) : content(std::move(s)) {
		}
		const std::u8string content; ///< The content that the user has entered.
	};
	/// Contains information about the ongoing composition when the user is using an input method.
	///
	/// \todo Add composition underline.
	struct composition_info {
		/// Constructor.
		explicit composition_info(std::u8string s) : composition_string(std::move(s)) {
		}
		const std::u8string composition_string; ///< The current composition string.
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
		friend element_collection;
		friend window;
		friend manager;
		friend scheduler;
		friend panel;
		friend class_arrangements;
		friend mouse_position;
	public:
		/// Default virtual destrucor.
		virtual ~element() = default;

		/// Returns the parent element.
		[[nodiscard]] panel *parent() const {
			return _parent;
		}

		/// Returns the current layout of this element, with respect to the window this element is in, and ignoring
		/// all transformations of all elements.
		[[nodiscard]] rectd get_layout() const {
			return _layout;
		}
		/// Calculates and returns the current client region, i.e., the layout of the element with padding subtracted
		/// from it.
		[[nodiscard]] rectd get_client_region() const {
			return get_padding().shrink(_layout);
		}
		/// Returns the width value used for layout calculation. If the current width allocation type is
		/// \ref size_allocation_type::automatic, the result will be that of \ref get_desired_width(); otherwise the
		/// user-defined width will be returned.
		[[nodiscard]] size_allocation get_layout_width() const;
		/// Returns the height value used for layout calculation.
		///
		/// \sa get_layout_width()
		[[nodiscard]] size_allocation get_layout_height() const;
		/// Returns the size allocation for the left margin.
		[[nodiscard]] size_allocation get_margin_left() const {
			return size_allocation(get_margin().left, (get_anchor() & anchor::left) != anchor::none);
		}
		/// Returns the size allocation for the right margin.
		[[nodiscard]] size_allocation get_margin_right() const {
			return size_allocation(get_margin().right, (get_anchor() & anchor::right) != anchor::none);
		}
		/// Returns the size allocation for the top margin.
		[[nodiscard]] size_allocation get_margin_top() const {
			return size_allocation(get_margin().top, (get_anchor() & anchor::top) != anchor::none);
		}
		/// Returns the size allocation for the bottom margin.
		[[nodiscard]] size_allocation get_margin_bottom() const {
			return size_allocation(get_margin().bottom, (get_anchor() & anchor::bottom) != anchor::none);
		}

		/// Returns the margin metric of this element.
		[[nodiscard]] thickness get_margin() const {
			return _layout_params.margin;
		}
		/// Returns the padding metric of this element.
		[[nodiscard]] thickness get_padding() const {
			return _layout_params.padding;
		}
		/// Returns the anchor metric of this element.
		[[nodiscard]] anchor get_anchor() const {
			return _layout_params.elem_anchor;
		}
		/// Returns the width allocation metric of this element.
		[[nodiscard]] size_allocation_type get_width_allocation() const {
			return _layout_params.width_alloc;
		}
		/// Returns the height allocation metric of this element.
		[[nodiscard]] size_allocation_type get_height_allocation() const {
			return _layout_params.height_alloc;
		}

		/// Sets the layout parameters of this element. This is only for debugging and could be overriden by
		/// animations.
		void set_layout_parameters_debug(const element_layout &layout) {
			_layout_params = layout;
			_on_layout_parameters_changed();
		}
		/// Returns the layout parameters for this \ref element.
		[[nodiscard]] const element_layout &get_layout_parameters() const {
			return _layout_params;
		}

		/// Sets the layout parameters of this element. This is only for debugging and could be overriden by
		/// animations. It's also potentially dangerous to override existing visuals with animations using this,
		/// which could lead to crashes and/or invalid reads/writes.
		void set_visual_parameters_debug(visuals visual) {
			_visual_params = std::move(visual);
			invalidate_visual();
		}
		/// Returns the visual parameters for this \ref element.
		[[nodiscard]] const visuals &get_visual_parameters() const {
			return _visual_params;
		}

		/// Computes the desired size of this element, in pixels, by calling \ref _compute_desired_size_impl(), and
		/// stores it in \ref _desired_size.
		void compute_desired_size(vec2d available_size) {
			_desired_size = _compute_desired_size_impl(available_size);
		}
		/// Returns the previously computed desired size.
		[[nodiscard]] vec2d get_desired_size() const {
			return _desired_size;
		}

		/// Used to test if a given point lies in the element.
		/// Derived classes can override this function to create elements with more complex shapes.
		[[nodiscard]] virtual bool hit_test(vec2d p) const {
			return p.x > 0.0 && p.x < _layout.width() && p.y > 0.0 && p.y < _layout.height();
		}


		/// Returns the default cursor of the element. Derived classes can override this function to change
		/// the default behavior.
		[[nodiscard]] virtual cursor get_default_cursor() const {
			return cursor::normal;
		}
		/// Returns the custom cursor displayed when the mouse is over this element.
		[[nodiscard]] cursor get_custom_cursor() const {
			return _custom_cursor;
		}
		/// Returns the cursor displayed when the mouse is over this element. By default this function returns the
		/// cusrom cursor if there is one, or the result returned by \ref get_default_cursor() otherwise.
		[[nodiscard]] virtual cursor get_current_display_cursor() const {
			cursor overriden = get_custom_cursor();
			if (overriden == cursor::not_specified) {
				return get_default_cursor();
			}
			return overriden;
		}

		/// Returns the window that contains the element, or \p nullptr if the element's not currently attached to
		/// one. For windows themselves, this function returns a pointer to the window itself, even if it's a child
		/// of another window.
		[[nodiscard]] window *get_window();
		/// Returns the \ref manager of this element.
		[[nodiscard]] manager &get_manager() const {
			return *_manager;
		}

		/// Sets the z-index of the element.
		void set_zindex(int);
		/// Returns the z-index of the element.
		[[nodiscard]] int get_zindex() const {
			return _zindex;
		}

		/// Sets the visibility of this element.
		void set_visibility(visibility v) {
			if (v != get_visibility()) {
				_visibility_changed_info info(get_visibility());
				_visibility = v;
				_on_visibility_changed(info);
			}
		}
		/// Returns the current visibility of this element.
		[[nodiscard]] visibility get_visibility() const {
			return _visibility;
		}
		/// Tests if the visiblity of this element have all the given visiblity flags set.
		[[nodiscard]] bool is_visible(visibility vis) const {
			return (get_visibility() & vis) == vis;
		}

		/// Sets whether to clip this element when rendering.
		void set_clip_to_bounds(bool clip) {
			if (clip != _clip_to_bounds) {
				_clip_to_bounds = clip;
				invalidate_visual();
			}
		}
		/// Returns whether this element is clipped when rendering.
		[[nodiscard]] bool get_clip_to_bounds() const {
			return _clip_to_bounds;
		}

		/// Returns if the mouse is hovering over this element.
		[[nodiscard]] bool is_mouse_over() const {
			return _mouse_over;
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
			layout_changed, ///< Triggered when the layout of this element has changed.
			destroying; ///< Triggered when this element is about to be destroyed.
		info_event<mouse_move_info> mouse_move; ///< Triggered when the mouse moves over the element.
		/// Triggered when the mouse hovers over the window for a while without moving.
		info_event<mouse_hover_info> mouse_hover;
		info_event<mouse_button_info>
			mouse_down, ///< Triggered when a mouse button is pressed when the mouse is over the element.
			mouse_up; ///< Triggered when a mouse button is released when the mouse is over the elemnet.
		info_event<mouse_scroll_info> mouse_scroll; ///< Triggered when the user scrolls the mouse over the element.
		info_event<key_info>
			key_down, ///< Triggered when a key is pressed when the element has the focus.
			key_up; ///< Triggered when a key is released when the element has the focus.
		info_event<text_info> keyboard_text; ///< Triggered as the user types when the element has the focus.

		/// Returns the default class of elements of this type.
		[[nodiscard]] inline static std::u8string_view get_default_class() {
			return u8"element";
		}
	private:
		/// Information about an ongoing animation.
		struct _animation_info {
			std::unique_ptr<playing_animation_base> animation; ///< The actual animation.
			scheduler::sync_task_token task; ///< The task related to this animation.
		};

		visuals _visual_params; ///< The visual parameters of this \ref element.
		element_layout _layout_params; ///< The layout parameters of this \ref element.
		vec2d _desired_size; ///< Desired size computed by \ref compute_desired_size().
		visibility _visibility = visibility::full; ///< The visibility of this \ref element.
		cursor _custom_cursor = cursor::not_specified; ///< The custom cursor for this \ref element.
		int _zindex = zindex::normal; ///< The z-index of the element.

		panel *_parent = nullptr; ///< Pointer to the element's parent.
		std::any _parent_data; ///< Data generated and used by the parent.

		vec2d _cached_mouse_position; ///< Cached mouse position relative to this elemnt.
		/// The timestamp used to check if \ref _cached_mouse_position is valid.
		std::size_t _cached_mouse_position_timestamp = 0;
		bool
			_mouse_over = false, ///< Indicates if the mouse is hovering over this element.
			/// Indicates that all contents that lie outside of the layout of this element should be clipped.
			_clip_to_bounds = false;

		std::list<_animation_info> _animations; ///< A list of playing animations.

		/// Starts an animation.
		void _start_animation(std::unique_ptr<playing_animation_base>);
	protected:
		rectd _layout; ///< The absolute layout of the element in the window.

		/// Contains information about a change in visibility.
		using _visibility_changed_info = value_update_info<visibility, value_update_info_contents::old_value>;

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
		virtual void _on_mouse_move(mouse_move_info &p) {
			mouse_move.invoke(p);
		}
		/// Called when the mouse hovers over the element. Invokes \ref mouse_hover.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_hover(mouse_hover_info &p) {
			mouse_hover.invoke(p);
		}
		/// Called when a mouse button is pressed when the mouse is over this element.
		/// Changes the focus if appropriate, and invokes \ref mouse_down.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_down(mouse_button_info&);
		/// Called when a mouse button is released when the mouse is over this element. Invokes \ref mouse_up.
		/// Derived classes should override this function instead of registering for the event.
		virtual void _on_mouse_up(mouse_button_info &p) {
			mouse_up.invoke(p);
		}
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

		/// Called after the element is added to a \ref panel. This method is invoked after
		/// \ref panel::_on_child_added() and \ref element_collection::changed.
		virtual void _on_added_to_parent() {
		}
		/// Called when the element is about to be removed from a \ref panel. This method is invoked before
		/// \ref panel::_on_child_removing() and \ref element_collection::changing.
		virtual void _on_removing_from_parent() {
		}

		/// Called whenever \ref _layout_params is changed. Note that this does not necessarily mean that the layout
		/// of the element will change, nor will this function be called in all situations that changes the layout
		/// (e.g., when size allocation is set to automatic and the desired size changes).
		virtual void _on_layout_parameters_changed();

		/// Called when the padding of the element has changed. Calls \ref invalidate_visual.
		virtual void _on_padding_changed() {
			invalidate_visual();
		}
		/// Called when the visibility of this element has changed.
		virtual void _on_visibility_changed(_visibility_changed_info&);

		/// Called when the element has lost the capture it previously got. This can happen when the capture is
		/// broken by an external event or when the element or one of its ancestors is removed from the window. This
		/// function invokes \ref lost_capture. Note that \ref window::set_mouse_capture() should not be called in
		/// this handler.
		virtual void _on_capture_lost() {
			lost_capture.invoke();
		}

		/// Called when the element is about to be rendered.
		virtual void _on_prerender();
		/// Called when the element is rendered. Renders all \ref visuals::geometries of \ref _params.
		/// Derived classes should override this function to perform custom rendering.
		virtual void _custom_render() const;
		/// Called after the element has been rendered.
		virtual void _on_postrender();
		/// Renders the element if the element is visible for \ref visibility::visual. This function first calls
		/// \ref _on_prerender(), then calls \ref _custom_render(), and finally calls \ref _on_postrender().
		void _on_render();

		/// Computes and returns the desired size of this element given available space. Returns the full size by
		/// default.
		[[nodiscard]] virtual vec2d _compute_desired_size_impl(vec2d available) {
			return available;
		}
		/// Called by the element itself when its desired size has changed.
		virtual void _on_desired_size_changed();
		/// Called by \ref manager when the layout has changed. Calls \ref invalidate_visual. Derived classes can
		/// override this to update layout-dependent properties. For panels, override
		/// \ref panel::_on_update_children_layout() instead when re-calculating the layout of its children.
		virtual void _on_layout_changed();


		/// Helper pseudo-namespace that contains utility functions to handle event registrations.
		struct _event_helpers {
			/// Registers the callback to the given event.
			template <typename Info> inline static void register_event(
				info_event<Info> &event, std::function<void()> callback
			) {
				if constexpr (std::is_same_v<Info, void>) { // register directly
					event += std::move(callback);
				} else { // ignore the arguments
					event += [cb = std::move(callback)](Info&) {
						cb();
					};
				}
			}
			/// Calls \ref register_event() to register the event if the given name matches the expected name.
			///
			/// \return Whether the event has been registered.
			template <typename Info> inline static bool try_register_event(
				std::u8string_view name, std::u8string_view expected, info_event<Info> &event, std::function<void()> &callback
			) {
				if (name == expected) {
					register_event(event, std::move(callback));
					return true;
				}
				return false;
			}

			/// Registers events for \p set_horizontal and \p set_vertical events.
			template <typename Info> inline static bool try_register_orientation_events(
				std::u8string_view name, info_event<Info> &event,
				std::function<orientation()> get_orientation, std::function<void()> &callback
			) {
				if (name == u8"set_vertical") {
					register_event(event, [cb = std::move(callback), get_ori = std::move(get_orientation)]() {
						if (get_ori() == orientation::vertical) {
							cb();
						}
					});
					return true;
				} else if (name == u8"set_horizontal") {
					register_event(event, [cb = std::move(callback), get_ori = std::move(get_orientation)]() {
						if (get_ori() == orientation::horizontal) {
							cb();
						}
					});
					return true;
				}
				return false;
			}

			/// Registers an event that only triggers if the associated mouse button matches the given one.
			inline static void register_mouse_button_event(
				info_event<mouse_button_info> &event, mouse_button mb, std::function<void()> callback
			) {
				event += [cb = std::move(callback), mb](mouse_button_info &info) {
					if (info.button == mb) {
						cb();
					}
				};
			}
			/// Registers separate mouse button events.
			template <bool Down> inline static bool try_register_all_mouse_button_events(
				std::u8string_view name, info_event<mouse_button_info> &event, std::function<void()> &callback
			) {
				std::u8string_view prim, scnd, trty;
				if constexpr (Down) {
					prim = u8"mouse_primary_down";
					scnd = u8"mouse_secondary_down";
					trty = u8"mouse_tertiary_down";
				} else {
					prim = u8"mouse_primary_up";
					scnd = u8"mouse_secondary_up";
					trty = u8"mouse_tertiary_up";
				}
				if (name == prim) {
					register_mouse_button_event(event, mouse_button::primary, std::move(callback));
					return true;
				} else if (name == scnd) {
					register_mouse_button_event(event, mouse_button::secondary, std::move(callback));
					return true;
				} else if (name == trty) {
					register_mouse_button_event(event, mouse_button::tertiary, std::move(callback));
					return true;
				}
				return false;
			}
		};
		/// Registers the callback for the event with the given name. This is used mainly for storyboard animations.
		///
		/// \return Whether the event is handled.
		virtual bool _register_event(std::u8string_view name, std::function<void()>);
		/// Parses an property path and returns the corresponding \ref property_info. If parsing fails the returned
		/// struct will contain null pointers.
		[[nodiscard]] virtual property_info _find_property_path(const property_path::component_list &path) const;
		/// Handles a reference. This will be called for all references, even those that failed to resolve to a valid
		/// element.
		///
		/// \return Whether the role is handled.
		virtual bool _handle_reference(std::u8string_view, element*) {
			return false; // no references for a basic element; if this is reached then it's unhandled
		}
		/// Called as the last step for all elements in a hierarchy created by one call to
		/// \ref manager::create_element(). This can be useful for, e.g., handling two-level references. By default
		/// this does nothing.
		virtual void _on_hierarchy_constructed() {
		}

		/// Used by elements to check and cast a reference pointer to the correct type, and assign it to the given
		/// pointer.
		///
		/// \return Whether the casted pointer is non-null.
		template <typename DesiredType> inline static bool _reference_cast_to(DesiredType *&target, element *value) {
			target = dynamic_cast<DesiredType*>(value);
			if (target == nullptr) {
				logger::get().log_error(CP_HERE) <<
					"incorrect reference type, need " << demangle(typeid(DesiredType).name()) <<
					", found " << demangle(typeid(*value).name());
				return false;
			}
			return true;
		}


		/// Called after the element is created to initialize it. When this is called, the element can assume that
		/// \ref _manager, and \ref _hotkeys are initialized, but all properties, event triggers, and element
		/// references are **not** initialized. All derived classes should call \p base::_initialize(). This function
		/// is primarily used to avoid pitfalls associated with virtual function calls in constructors to have this
		/// function - use this instead of the constructor whenever possible.
		virtual void _initialize();
		/// Called immediately before the element is disposed. Removes the element from its parent if it has one.
		/// All derived classes should call \p base::_dispose <em>after</em> disposing of their own components.
		/// It is primarily intended to avoid pitfalls associated with virtual function calls in destructors to have
		/// this function.
		virtual void _dispose();
	private:
		const hotkey_group *_hotkeys = nullptr; ///< The hotkey group of this element.
		/// The \ref manager of this element. This is set after this element is constructed but before
		/// \ref _initialize() is called.
		manager *_manager = nullptr;
#ifdef CP_CHECK_USAGE_ERRORS
		bool _initialized = false; ///< Indicates wheter the element has been properly initialized and disposed.
#endif

		/// Returns \p this only for windows.
		[[nodiscard]] virtual window *_get_as_window() {
			return nullptr;
		}
		/// \override
		[[nodiscard]] virtual const window *_get_as_window() const {
			return nullptr;
		}
	};
}
