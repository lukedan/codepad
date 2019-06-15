// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Elements that are commonly used in the UI environment.

#include "element.h"
#include "panel.h"
#include "manager.h"
#include "text_rendering.h"
#include "window.h"
#include "../core/misc.h"
#include "../core/encodings.h"

namespace codepad::ui {
	/// The parameters used to identify a font.
	struct font_parameters {
		/// Default constructor.
		font_parameters() = default;
		/// Initializes all fields of this struct.
		font_parameters(
			str_t f, double sz, font_style st = font_style::normal,
			font_weight w = font_weight::normal, font_stretch width = font_stretch::normal
		) : family(std::move(f)), size(sz), style(st), weight(w), stretch(width) {
		}

		str_t family; ///< The font family.
		double size = 10.0; ///< The font size.
		font_style style = font_style::normal; ///< The font style.
		font_weight weight = font_weight::normal; ///< The font weight.
		font_stretch stretch = font_stretch::normal; ///< The stretch of the font.
	};

	/// A label that displays plain text. Non-focusable by default.
	class label : public element {
	public:
		/// Returns the combined width of the text and the padding in pixels.
		size_allocation get_desired_width() const override {
			_check_cache_format();
			return size_allocation(_cached_fmt->get_layout().width(), true);
		}
		/// Returns the combined height of the text and the padding in pixels.
		size_allocation get_desired_height() const override {
			_check_cache_format();
			return size_allocation(_cached_fmt->get_layout().height(), true);
		}

		/// Returns the text.
		const str_t get_text() const {
			return _text;
		}
		/// Sets the text.
		void set_text(str_t t) {
			_text = std::move(t);
			_on_text_layout_changed();
		}

		/// Returns the brush.
		const generic_brush &get_brush() const {
			return _text_brush;
		}
		/// Sets the brush.
		void set_brush(generic_brush b) {
			_text_brush = std::move(b);
			invalidate_visual();
		}

		/// Returns the font parameters.
		const font_parameters get_font_parameters() const {
			return _font;
		}
		/// Sets the font parameters.
		void set_font_parameters(font_parameters params) {
			_font = std::move(params);
			_on_text_layout_changed();
		}

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("label");
		}
	protected:
		str_t _text; ///< The text.
		generic_brush _text_brush; ///< The brush.
		font_parameters _font; ///< The font.
		mutable std::unique_ptr<formatted_text> _cached_fmt; ///< The cached formatted text.

		/// Calls \ref _check_cache_format().
		void _on_prerender() override {
			element::_on_prerender();
			_check_cache_format();
		}
		/// Renders the text.
		void _custom_render() const override {
			element::_custom_render();

			rectd client = get_client_region();
			generic_brush_parameters brush = _text_brush.get_parameters(client.size());
			brush.transform *= matd3x3::translate(client.xmin_ymin() - get_layout().xmin_ymin());
			get_manager().get_renderer().draw_formatted_text(
				*_cached_fmt, client.xmin_ymin(), brush
			);
		}

		/// Ensures that \ref _cached_fmt is valid.
		void _check_cache_format() const {
			if (!_cached_fmt) {
				rectd client = get_client_region();
				auto fmt = get_manager().get_renderer().create_text_format(
					_font.family, _font.size, _font.style, _font.weight, _font.stretch
				);
				_cached_fmt = get_manager().get_renderer().format_text(
					get_text(), *fmt, client.size(), wrapping_mode::none,
					horizontal_text_alignment::front, vertical_text_alignment::top
				);
			}
		}
		/// Called when the layout of the text has potentially changed.
		virtual void _on_text_layout_changed() {
			_cached_fmt.reset();
			_on_desired_size_changed(true, true);
		}
	};

	/// Base class of button-like elements, that provides all interfaces only internally.
	class button : public panel {
	public:
		/// Indicates when the click event is triggered.
		enum class trigger_type {
			mouse_down, ///< The event is triggered as soon as the button is pressed.
			mouse_up ///< The event is triggered after the user presses then releases the button.
		};

		/// Returns \p true if the button is currently pressed.
		bool is_trigger_button_pressed() const {
			return _trigbtn_down;
		}

		/// Sets the mouse button used to press the button, the `trigger button'.
		void set_trigger_button(mouse_button btn) {
			_trigbtn = btn;
		}
		/// Returns the current trigger button.
		mouse_button get_trigger_button() const {
			return _trigbtn;
		}

		/// Sets when the button click is triggered.
		void set_trigger_type(trigger_type t) {
			_trigtype = t;
		}
		/// Returns the current trigger type.
		trigger_type get_trigger_type() const {
			return _trigtype;
		}

		/// Sets whether the user is allowed to cancel the click halfway.
		void set_allow_cancel(bool v) {
			_allow_cancel = v;
		}
		/// Returns \p true if the user is allowed to cancel the click.
		bool get_allow_cancel() const {
			return _allow_cancel;
		}

		info_event<> click; ///< Triggered when the button is clicked.

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("button");
		}
	protected:
		bool
			_trigbtn_down = false, ///< Indicates whether the trigger button is currently down.
			/// If \p true, the user will be able to cancel the click after pressing the trigger button,
			/// by moving the cursor out of the button and releasing the trigger button,
			/// if  \ref _trigtype is set to trigger_type::mouse_up.
			_allow_cancel = true;
		trigger_type _trigtype = trigger_type::mouse_up; ///< The trigger type of this button.
		/// The mouse button that triggers the button.
		mouse_button _trigbtn = mouse_button::primary;

		/// Checks for clicks.
		void _on_mouse_down(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position.get(*this));
			if (_hit_test_for_child(p.position) == nullptr) {
				if (p.button == _trigbtn) {
					_trigbtn_down = true;
					get_window()->set_mouse_capture(*this); // capture the mouse
					if (_trigtype == trigger_type::mouse_down) {
						_on_click();
					}
				}
			}
			panel::_on_mouse_down(p);
		}
		/// Calls _on_trigger_button_up().
		void _on_capture_lost() override {
			_trigbtn_down = false;
			panel::_on_capture_lost();
		}
		/// Checks if this is a valid click.
		void _on_mouse_up(mouse_button_info &p) override {
			_on_update_mouse_pos(p.position.get(*this));
			if (
				_trigbtn_down && p.button == _trigbtn &&
				_hit_test_for_child(p.position) == nullptr
				) {
				_trigbtn_down = false;
				get_window()->release_mouse_capture();
				if (is_mouse_over() && _trigtype == trigger_type::mouse_up) {
					_on_click();
				}
			}
			panel::_on_mouse_up(p);
		}
		/// Called when the mouse position need to be updated. If \ref _allow_cancel is \p true, checks if the mouse
		/// is still over the element, and invokes \ref _on_mouse_enter() or \ref _on_mouse_leave() accordingly.
		void _on_update_mouse_pos(vec2d pos) {
			if (_allow_cancel) {
				bool over = hit_test(pos);
				if (over != is_mouse_over()) {
					if (over) {
						_on_mouse_enter();
					} else {
						_on_mouse_leave();
					}
				}
			}
		}
		/// Updates the mouse position.
		void _on_mouse_move(mouse_move_info &p) override {
			_on_update_mouse_pos(p.new_position.get(*this));
			element::_on_mouse_move(p);
		}

		/// Callback that is called when the user clicks the button. Invokes \ref click by default.
		virtual void _on_click() {
			click.invoke();
		}
	};

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
		inline static str_view_t get_default_class() {
			return CP_STRLIT("scrollbar_drag_button");
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

		/// Returns the default desired width of the scroll bar.
		size_allocation get_desired_width() const override {
			if (get_orientation() == orientation::vertical) {
				return size_allocation(default_thickness, true);
			}
			return size_allocation(1.0, false);
		}
		/// Returns the default desired height of the scroll bar.
		size_allocation get_desired_height() const override {
			if (get_orientation() != orientation::vertical) {
				return size_allocation(default_thickness, true);
			}
			return size_allocation(1.0, false);
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

		/// Scrolls the scroll bar so that a certain point is in the visible region.
		/// Note that the point appears at the very edge of the region if it's not visible before.
		void make_point_visible(double v) {
			if (_value > v) {
				set_value(v);
			} else {
				v -= _visible_range;
				if (_value < v) {
					set_value(v);
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

		/// Invoked when the value of the scrollbar is changed.
		info_event<value_update_info<double>> value_changed;

		/// Returns the default class of elements of this type.
		inline static str_view_t get_default_class() {
			return CP_STRLIT("scrollbar");
		}

		/// Returns the name identifier of the `page up' button.
		inline static str_view_t get_page_up_button_name() {
			return CP_STRLIT("page_up_button");
		}
		/// Returns the name identifier of the `page down' button.
		inline static str_view_t get_page_down_button_name() {
			return CP_STRLIT("page_down_button");
		}
		/// Returns the name identifier of the drag button.
		inline static str_view_t get_drag_button_name() {
			return CP_STRLIT("drag_button");
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
		void _on_update_children_layout() override {
			rectd cln = get_client_region();
			double min, max, mid1, mid2;
			if (get_orientation() == orientation::vertical) {
				min = cln.ymin;
				max = cln.ymax;
			} else {
				min = cln.xmin;
				max = cln.xmax;
			}
			double
				totsize = max - min,
				btnlen = totsize * _visible_range / _total_range;
			_drag_button_extended = btnlen < _drag->get_minimum_length();
			if (_drag_button_extended) {
				btnlen = _drag->get_minimum_length();
				double percentage = _value / (_total_range - _visible_range);
				mid1 = min + (totsize - btnlen) * percentage;
				mid2 = mid1 + btnlen;
			} else {
				double ratio = totsize / _total_range;
				mid1 = min + ratio * _value;
				mid2 = mid1 + ratio * _visible_range;
			}
			if (get_orientation() == orientation::vertical) {
				panel::layout_child_horizontal(*_drag, cln.xmin, cln.xmax);
				panel::layout_child_horizontal(*_pgup, cln.xmin, cln.xmax);
				panel::layout_child_horizontal(*_pgdn, cln.xmin, cln.xmax);
				_child_set_vertical_layout(*_drag, mid1, mid2);
				_child_set_vertical_layout(*_pgup, min, mid1);
				_child_set_vertical_layout(*_pgdn, mid2, max);
			} else {
				panel::layout_child_vertical(*_drag, cln.ymin, cln.ymax);
				panel::layout_child_vertical(*_pgup, cln.ymin, cln.ymax);
				panel::layout_child_vertical(*_pgdn, cln.ymin, cln.ymax);
				_child_set_horizontal_layout(*_drag, mid1, mid2);
				_child_set_horizontal_layout(*_pgup, min, mid1);
				_child_set_horizontal_layout(*_pgdn, mid2, max);
			}
		}
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

		/// Sets the orientation of this element if requested.
		void _set_attribute(str_view_t name, const json::value_storage & value) override {
			if (name == u8"orientation") {
				if (orientation o; json::object_parsers::try_parse(value.get_value(), o)) {
					set_orientation(o);
				}
				return;
			}
			panel::_set_attribute(name, value);
		}

		/// Initializes the three buttons and adds them as children.
		void _initialize(str_view_t cls, const element_configuration & config) override {
			panel::_initialize(cls, config);

			get_manager().get_class_arrangements().get_or_default(cls).construct_children(
				*this, {
					{get_drag_button_name(), _name_cast(_drag)},
				{get_page_up_button_name(), _name_cast(_pgup)},
				{get_page_down_button_name(), _name_cast(_pgdn)}
				});

			_pgup->set_trigger_type(button::trigger_type::mouse_down);
			_pgup->click += [this]() {
				set_value(get_value() - get_visible_range());
			};

			_pgdn->set_trigger_type(button::trigger_type::mouse_down);
			_pgdn->click += [this]() {
				set_value(get_value() + get_visible_range());
			};
		}

		/// Called after the orientation has been changed. Invalidates the layout of all components.
		virtual void _on_orientation_changed() {
			_on_desired_size_changed(true, true);
			_invalidate_children_layout();
		}
	};
}
