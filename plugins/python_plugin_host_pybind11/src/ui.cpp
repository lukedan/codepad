// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of the \ref register_ui_classes() function.

#include <pybind11/pybind11.h>

namespace py = ::pybind11;

#include <codepad/ui/misc.h>
#include <codepad/ui/element.h>

#include <codepad/ui/manager.h>

namespace cp = ::codepad;

#include "shared.h"

namespace python_plugin_host_pybind11 {
	/// `Trampoline' class for \ref codepad::ui::element.
	template<typename Base> class py_ui_element : public Base {
	public:
		/// Wrapper around \ref codepad::ui::element::get_desired_width().
		cp::ui::size_allocation get_desired_width() const override {
			TRY_OVERLOAD(cp::ui::size_allocation, Base, get_desired_width,);
			return cp::ui::size_allocation();
		}
		/// Wrapper around \ref codepad::ui::element::get_desired_height().
		cp::ui::size_allocation get_desired_height() const override {
			TRY_OVERLOAD(cp::ui::size_allocation, Base, get_desired_height,);
			return cp::ui::size_allocation();
		}

		/// Wrapper around \ref codepad::ui::element::hit_test().
		bool hit_test(cp::vec2d p) const override {
			TRY_OVERLOAD(bool, Base, hit_test, p);
			return false;
		}
		/// Wrapper around \ref codepad::ui::element::get_default_cursor().
		cp::ui::cursor get_default_cursor() const override {
			TRY_OVERLOAD(cp::ui::cursor, Base, get_default_cursor,);
			return cp::ui::cursor::denied;
		}
		/// Wrapper around \ref codepad::ui::element::get_current_display_cursor().
		cp::ui::cursor get_current_display_cursor() const override {
			TRY_OVERLOAD(cp::ui::cursor, Base, get_current_display_cursor,);
			return cp::ui::cursor::denied;
		}
	};

	/// Registers all \p codepad::ui classes.
	void register_ui_classes(py::module &m) {
		// enums
		py::enum_<cp::ui::orientation>(m, "orientation")
			.value("horizontal", cp::ui::orientation::horizontal)
			.value("vertical", cp::ui::orientation::vertical);

		py::enum_<cp::ui::visibility>(m, "visibility", py::arithmetic())
			.value("none", cp::ui::visibility::none)
			.value("visual", cp::ui::visibility::visual)
			.value("interact", cp::ui::visibility::interact)
			.value("layout", cp::ui::visibility::layout)
			.value("focus", cp::ui::visibility::focus)
			.value("full", cp::ui::visibility::full);

		py::enum_<cp::ui::cursor>(m, "cursor")
			.value("normal", cp::ui::cursor::normal)
			.value("busy", cp::ui::cursor::busy)
			.value("crosshair", cp::ui::cursor::crosshair)
			.value("hand", cp::ui::cursor::hand)
			.value("help", cp::ui::cursor::help)
			.value("text_beam", cp::ui::cursor::text_beam)
			.value("denied", cp::ui::cursor::denied)
			.value("arrow_all", cp::ui::cursor::arrow_all)
			.value("arrow_northeast_southwest", cp::ui::cursor::arrow_northeast_southwest)
			.value("arrow_north_south", cp::ui::cursor::arrow_north_south)
			.value("arrow_northwest_southeast", cp::ui::cursor::arrow_northwest_southeast)
			.value("arrow_east_west", cp::ui::cursor::arrow_east_west)
			.value("invisible", cp::ui::cursor::invisible)
			.value("not_specified", cp::ui::cursor::not_specified);

		py::enum_<cp::ui::mouse_button>(m, "mouse_button")
			.value("primary", cp::ui::mouse_button::primary)
			.value("tertiary", cp::ui::mouse_button::tertiary)
			.value("secondary", cp::ui::mouse_button::secondary);

		py::enum_<cp::ui::key>(m, "key"); // TODO

		// TODO anchor
		py::enum_<cp::ui::anchor>(m, "anchor", py::arithmetic())
			.value("none", cp::ui::anchor::none);

		py::enum_<cp::ui::size_allocation_type>(m, "size_allocation_type")
			.value("automatic", cp::ui::size_allocation_type::automatic)
			.value("fixed", cp::ui::size_allocation_type::fixed)
			.value("proportion", cp::ui::size_allocation_type::proportion);


		py::class_<cp::ui::thickness>(m, "thickness")
			.def(py::init<>())
			.def(py::init<double>())
			.def(py::init<double, double, double, double>())
			.def_readwrite("left", &cp::ui::thickness::left)
			.def_readwrite("top", &cp::ui::thickness::top)
			.def_readwrite("right", &cp::ui::thickness::right)
			.def_readwrite("bottom", &cp::ui::thickness::bottom)
			.def("extend", &cp::ui::thickness::extend)
			.def("shrink", &cp::ui::thickness::shrink)
			.def_property_readonly("width", &cp::ui::thickness::width)
			.def_property_readonly("height", &cp::ui::thickness::height)
			.def_property_readonly("size", &cp::ui::thickness::size);

		py::class_<cp::ui::size_allocation>(m, "size_allocation")
			.def(py::init<>())
			.def_static("pixels", &cp::ui::size_allocation::pixels)
			.def_static("proportion", &cp::ui::size_allocation::proportion)
			.def_readwrite("value", &cp::ui::size_allocation::value)
			.def_readwrite("is_pixels", &cp::ui::size_allocation::is_pixels);


		py::class_<cp::ui::mouse_position>(m, "mouse_position")
			.def("get", &cp::ui::mouse_position::get);


		// event info types
		py::class_<cp::ui::mouse_move_info>(m, "mouse_move_info")
			.def_readonly("new_position", &cp::ui::mouse_move_info::new_position);

		py::class_<cp::ui::mouse_scroll_info>(m, "mouse_scroll_info")
			.def_readonly("raw_delta", &cp::ui::mouse_scroll_info::raw_delta)
			.def_readonly("position", &cp::ui::mouse_scroll_info::position)
			.def_readonly("is_smooth", &cp::ui::mouse_scroll_info::is_smooth)
			.def_property_readonly("delta", &cp::ui::mouse_scroll_info::delta)
			.def("consume", &cp::ui::mouse_scroll_info::consume)
			.def("consume_horizontal", &cp::ui::mouse_scroll_info::consume_horizontal)
			.def("consume_vertical", &cp::ui::mouse_scroll_info::consume_vertical);

		py::class_<cp::ui::mouse_button_info>(m, "mouse_button_info")
			.def_readonly("button", &cp::ui::mouse_button_info::button)
			.def_readonly("modifiers", &cp::ui::mouse_button_info::modifiers)
			.def_readonly("position", &cp::ui::mouse_button_info::position)
			.def_property_readonly("gesture", &cp::ui::mouse_button_info::get_gesture)
			.def_property_readonly("is_focus_set", &cp::ui::mouse_button_info::is_focus_set)
			.def("mark_focus_set", &cp::ui::mouse_button_info::mark_focus_set);

		py::class_<cp::ui::key_info>(m, "key_info")
			.def_readonly("key_pressed", &cp::ui::key_info::key_pressed);

		py::class_<cp::ui::text_info>(m, "text_info")
			.def_readonly("content", &cp::ui::text_info::content);

		py::class_<cp::ui::composition_info>(m, "composition_info")
			.def_readonly("composition_string", &cp::ui::composition_info::composition_string);


		py::class_<cp::ui::element, py_ui_element<cp::ui::element>>(m, "element")
			.def_property_readonly("parent", &cp::ui::element::parent, py::return_value_policy::reference)
			.def_property_readonly("logical_parent", &cp::ui::element::parent, py::return_value_policy::reference)
			.def_property_readonly("layout", &cp::ui::element::get_layout)
			.def_property_readonly("client_region", &cp::ui::element::get_client_region)
			.def_property_readonly("layout_width", &cp::ui::element::get_layout_width)
			.def_property_readonly("layout_height", &cp::ui::element::get_layout_height)
			.def_property_readonly("margin", &cp::ui::element::get_margin)
			.def_property_readonly("padding", &cp::ui::element::get_padding)
			.def_property_readonly("anchor", &cp::ui::element::get_anchor)
			.def_property_readonly("width_allocation", &cp::ui::element::get_width_allocation)
			.def_property_readonly("height_allocation", &cp::ui::element::get_height_allocation)
			.def_property_readonly(
				"layout_parameters", &cp::ui::element::get_layout_parameters, py::return_value_policy::reference
			)
			.def_property_readonly(
				"visual_parameters", &cp::ui::element::get_visual_parameters, py::return_value_policy::reference
			)
			.def_property_readonly("desired_width", &cp::ui::element::get_desired_width)
			.def_property_readonly("desired_height", &cp::ui::element::get_desired_height)
			.def("hit_test", &cp::ui::element::hit_test)

			.def("get_default_cursor", &cp::ui::element::get_default_cursor) // function version for overriding
			.def_property_readonly("default_cursor", &cp::ui::element::get_default_cursor)
			.def_property_readonly("custom_cursor", &cp::ui::element::get_custom_cursor)
				// function version for overriding
			.def("get_current_display_cursor", &cp::ui::element::get_current_display_cursor)
			.def_property_readonly("current_display_cursor", &cp::ui::element::get_current_display_cursor)

			.def_property_readonly("window", &cp::ui::element::get_window, py::return_value_policy::reference)
			.def_property_readonly("manager", &cp::ui::element::get_manager, py::return_value_policy::reference)

			.def_property("zindex", &cp::ui::element::get_zindex, &cp::ui::element::set_zindex)
			.def_property("visibility", &cp::ui::element::get_visibility, &cp::ui::element::set_visibility)
			.def_property_readonly("is_visible", &cp::ui::element::is_visible)
			.def_property_readonly("is_mouse_over", &cp::ui::element::is_mouse_over)

			.def("invalidate_visual", &cp::ui::element::invalidate_visual)
			.def("invalidate_layout", &cp::ui::element::invalidate_layout)

			.def_property_readonly("properties", &cp::ui::element::get_properties, py::return_value_policy::reference)

			.def_readonly("mouse_enter", &cp::ui::element::mouse_enter)
			.def_readonly("mouse_leave", &cp::ui::element::mouse_leave)
			.def_readonly("got_focus", &cp::ui::element::got_focus)
			.def_readonly("lost_focus", &cp::ui::element::lost_focus)
			.def_readonly("lost_capture", &cp::ui::element::lost_capture)
			.def_readonly("layout_changed", &cp::ui::element::layout_changed)
			.def_readonly("mouse_move", &cp::ui::element::mouse_move)
			.def_readonly("mouse_down", &cp::ui::element::mouse_down)
			.def_readonly("mouse_up", &cp::ui::element::mouse_up)
			.def_readonly("mouse_scroll", &cp::ui::element::mouse_scroll)
			.def_readonly("key_down", &cp::ui::element::key_down)
			.def_readonly("key_up", &cp::ui::element::key_up)
			.def_readonly("keyboard_text", &cp::ui::element::keyboard_text);
	}
}
