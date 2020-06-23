// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Scrollbars.

#include "button.h"

namespace codepad::ui {
	class scrollbar;
	/// The draggable button of a \ref scrollbar.
	class scrollbar_drag_button : public button {
	public:
		/// Returns the minimum length of this button.
		double get_minimum_length() const {
			return _min_length;
		}
		/// Sets the minimum length of this button.
		void set_minimum_length(double len) {
			_min_length = len;
			invalidate_layout();
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"scrollbar_drag_button";
		}
	protected:
		double
			_doffset = 0.0, ///< The offset of the mouse when the button is being dragged.
			_min_length = 15.0; ///< The minimum length of this butten.

		/// Returns the \ref scrollbar that this button belongs to.
		scrollbar &_get_bar() const;

		/// Overridden so that instances this element can be created.
		void _on_click() override {
		}

		/// Sets \ref _doffset accordingly if dragging starts.
		void _on_mouse_down(mouse_button_info&) override;
		/// Updates the value of the parent \ref scrollbar when dragging.
		void _on_mouse_move(mouse_move_info&) override;
	};
	/// A scroll bar.
	class scrollbar : public panel {
		friend scrollbar_drag_button;
	public:
		/// The default thickness of scrollbars.
		constexpr static double default_thickness = 10.0;

		/// Contains the old value when the value of this \ref scrollbar has changed.
		using value_changed_info = value_update_info<double, value_update_info_contents::old_value>;

		/// Returns the default desired width of the scroll bar.
		size_allocation get_desired_width() const override {
			if (get_orientation() == orientation::vertical) {
				return size_allocation::pixels(default_thickness);
			}
			return size_allocation::proportion(1.0);
		}
		/// Returns the default desired height of the scroll bar.
		size_allocation get_desired_height() const override {
			if (get_orientation() != orientation::vertical) {
				return size_allocation::pixels(default_thickness);
			}
			return size_allocation::proportion(1.0);
		}

		/// Sets the current value of the scroll bar.
		virtual void set_value(double v) {
			double ov = _value;
			_value = std::clamp(v, 0.0, _total_range - _visible_range);
			_invalidate_children_layout();
			value_changed.invoke_noret(ov);
		}
		/// Returns the current value of the scroll bar.
		double get_value() const {
			return _value;
		}
		/// Sets the parameters of the scroll bar.
		///
		/// \param tot The total length of the region.
		/// \param vis The visible length of the region.
		void set_params(double tot, double vis) {
			assert_true_usage(vis <= tot, "scrollbar visible range too large");
			_total_range = tot;
			_visible_range = vis;
			set_value(_value);
		}
		/// Returns the total length of the region.
		double get_total_range() const {
			return _total_range;
		}
		/// Returns the visible length of the region.
		double get_visible_range() const {
			return _visible_range;
		}

		/// Scrolls the scroll bar so that as much of the given range is visible as possible.
		void make_range_visible(double min, double max) {
			if (max - min > get_visible_range()) {
				if (min > get_value()) {
					set_value(min);
				} else if (double maxtop = max - get_visible_range(); maxtop < get_value()) {
					set_value(maxtop);
				}
			} else {
				if (min < get_value()) {
					set_value(min);
				} else if (double mintop = max - get_visible_range(); mintop > get_value()) {
					set_value(mintop);
				}
			}
		}

		/// Returns the current orientation.
		orientation get_orientation() const {
			return _orientation;
		}
		/// Sets the current orientation.
		void set_orientation(orientation o) {
			if (o != _orientation) {
				_orientation = o;
				_on_orientation_changed();
			}
		}

		/// Returns the list of properties.
		const property_mapping &get_properties() const override;

		/// Invoked when the value of the scrollbar is changed.
		info_event<value_changed_info> value_changed;
		info_event<> orientation_changed; ///< Invoked when the orientation of this element is changed.

		/// Adds the \p orientation property.
		static const property_mapping &get_properties_static();

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"scrollbar";
		}

		/// Returns the name identifier of the `page up' button.
		inline static std::u8string_view get_page_up_button_name() {
			return u8"page_up_button";
		}
		/// Returns the name identifier of the `page down' button.
		inline static std::u8string_view get_page_down_button_name() {
			return u8"page_down_button";
		}
		/// Returns the name identifier of the drag button.
		inline static std::u8string_view get_drag_button_name() {
			return u8"drag_button";
		}
	protected:
		double
			_total_range = 1.0, ///< The length of the whole range.
			_value = 0.0, ///< The minimum visible position.
			_visible_range = 0.1; ///< The length of the visible range.
		orientation _orientation = orientation::horizontal; ///< The orientation of this scrollbar.
		scrollbar_drag_button *_drag = nullptr; ///< The drag button.
		button
			*_pgup = nullptr, ///< The `page up' button.
			*_pgdn = nullptr; ///< The `page down' button.
		/// Marks if the length of \ref _drag is currently extended so that it's easier to interact with.
		bool _drag_button_extended = false;

		/// Calculates the layout of the three buttons.
		void _on_update_children_layout() override;
		/// Called when \ref _drag is being dragged by the user. Calculate the new value of this \ref scrollbar.
		///
		/// \param newmin The new top or left boundary of \ref _drag relative to this element.
		virtual void _on_drag_button_moved(double newmin) {
			double range, draglen;
			rectd client = get_client_region();
			if (get_orientation() == orientation::vertical) {
				range = client.height();
				draglen = _drag->get_layout().height();
			} else {
				range = client.width();
				draglen = _drag->get_layout().width();
			}
			double
				diff = newmin,
				totsz = range;
			if (_drag_button_extended) {
				set_value((get_total_range() - get_visible_range()) * diff / (totsz - draglen));
			} else {
				set_value(get_total_range() * diff / totsz);
			}
		}

		/// Handles the \p set_horizontal and \p set_vertical events.
		bool _register_event(std::u8string_view name, std::function<void()> callback) override {
			return
				_event_helpers::try_register_orientation_events(
					name, orientation_changed, [this]() {
						return get_orientation();
					}, callback
				) || panel::_register_event(name, std::move(callback));
		}

		/// Adds \ref _drag, \ref _pgup, and \ref _pgdn to the mapping.
		class_arrangements::notify_mapping _get_child_notify_mapping() override;

		/// Initializes the three buttons and adds them as children.
		void _initialize(std::u8string_view cls) override;

		/// Called after the orientation has been changed. Invalidates the layout of all components.
		virtual void _on_orientation_changed() {
			_on_desired_size_changed(true, true);
			_invalidate_children_layout();
			orientation_changed.invoke();
		}
	};
}
