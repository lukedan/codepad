// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of the \ref register_core_classes() function.

#include <pybind11/pybind11.h>
#include <pybind11/embed.h>
#include <pybind11/operators.h>
#include <pybind11/functional.h>

namespace py = ::pybind11;

#include <codepad/core/event.h>
#include <codepad/core/logging.h>
#include <codepad/core/math.h>
#include <codepad/core/misc.h>
#include <codepad/core/plugins.h>
#include <codepad/core/settings.h>
#include <codepad/ui/manager.h>
#include <codepad/ui/elements/tabs/manager.h>

namespace cp = ::codepad;

#include "shared.h"

namespace python_plugin_host_pybind11 {
	/// Registers a \p codepad::info_event type.
	template<typename T> void register_info_event_and_token(py::module &m, const char *tok_name, const char *ev_name) {
		auto info_event = py::class_<cp::info_event<T>>(m, ev_name);
		info_event.def(
			"register",
			[](cp::info_event<T> &ev, typename cp::info_event<T>::handler func) {
				return (ev += std::move(func));
			}
		);
		info_event.def(
			"unregister",
			[](cp::info_event<T> &ev, typename cp::info_event<T>::token &tok) {
				ev -= tok;
			}
		);

		py::class_<typename cp::info_event<T>::token>(info_event, tok_name);
	}

	/// Registers a \p codepad::vec2 type.
	template<typename T> void register_codepad_vec(py::module &m, const char *name) {
		auto vec2d = py::class_<cp::vec2<T>>(m, name)
			// constructors
			.def(py::init<>())
			.def(py::init<T, T>())
				// attributes
			.def_readwrite("x", &cp::vec2<T>::x)
			.def_readwrite("y", &cp::vec2<T>::y)
				// properties
			.def_property_readonly("length_sqr", &cp::vec2<T>::length_sqr)
			.def_property_readonly("length", &cp::vec2<T>::length)
				// operators
			.def(py::self += py::self)
			.def(py::self + py::self)
			.def(-py::self)
			.def(py::self -= py::self)
			.def(py::self - py::self)
			.def(py::self *= T())
			.def(py::self * T())
			.def(T() * py::self)
			.def(py::self /= T())
			.def(py::self / T());
		// indexing
		vec2d.def(
			"__getitem__",
			[](const cp::vec2<T> &v, std::size_t i) {
				return v[i];
			}
		);
		vec2d.def(
			"__setitem__",
			[](cp::vec2<T> &v, std::size_t i, T val) {
				v[i] = val;
			}
		);
	}

	/// `Trampoline' class for \ref codepad::plugin.
	class py_plugin : public cp::plugin {
	public:
		/// Wrapper around pure \ref codepad::plugin::initialize().
		void initialize(const cp::plugin_context &ctx) override {
			add_dependency(*this_plugin);
			TRY_OVERRIDE_PURE(void, cp::plugin, initialize, ctx);
		}
		/// Wrapper around \ref codepad::plugin::finalize().
		void finalize() override {
			TRY_OVERRIDE(void, cp::plugin, finalize,);
		}
		/// Wrapper around pure \ref codepad::plugin::get_name().
		std::u8string get_name() const override {
			TRY_OVERRIDE_PURE(std::u8string, cp::plugin, get_name,);
			return u8"";
		}

		/// Wrapper around pure \ref codepad::plugin::_on_enabled().
		void _on_enabled() override {
			TRY_OVERRIDE_PURE(void, cp::plugin, _on_enabled,);
		}
		/// Wrapper around pure \ref codepad::plugin::_on_disabled().
		void _on_disabled() override {
			TRY_OVERRIDE_PURE(void, cp::plugin, _on_disabled,);
		}
	private:
		/// This is a managed plugin.
		bool _is_managed() const override {
			return true;
		}
	};

	void register_core_classes(pybind11::module &m) {
		m.doc() = "Python binding for codepad generated using pybind11";

		register_info_event_and_token<void>(m, "info_event_token", "info_event");

		{
			py::enum_<cp::log_level>(m, "log_level")
				.value("error", cp::log_level::error)
				.value("warning", cp::log_level::warning)
				.value("info", cp::log_level::info)
				.value("debug", cp::log_level::debug);
			py::class_<cp::log_sink>(m, "log_sink")
				.def("on_message", &cp::log_sink::on_message);
			auto logger = py::class_<cp::logger>(m, "logger");
			logger.def_property_readonly_static(
				"current",
				[](py::object) -> cp::logger & {
					return cp::logger::get();
				}
			);
			logger.def(
				"log",
				[](cp::logger &log, cp::log_level lvl, std::u8string message) {
					log.log(lvl) << message; // TODO use proper source location in python
				}
			);
		}

		register_codepad_vec<double>(m, "vec2d");
		register_codepad_vec<int>(m, "vec2i");

		py::class_<cp::rectd>(m, "rectd")
			// constructors
			.def(py::init<>())
			.def(py::init<double, double, double, double>())
				// static constructors
			.def_static("from_xywh", cp::rectd::from_xywh)
			.def_static("from_corners", cp::rectd::from_corners)
			.def_static("from_corner_and_size", cp::rectd::from_corner_and_size)
				// attributes
			.def_readwrite("xmin", &cp::rectd::xmin)
			.def_readwrite("xmax", &cp::rectd::xmax)
			.def_readwrite("ymin", &cp::rectd::ymin)
			.def_readwrite("ymax", &cp::rectd::ymax)
				// properties
			.def_property_readonly("width", &cp::rectd::width)
			.def_property_readonly("height", &cp::rectd::height)
			.def_property_readonly("size", &cp::rectd::size)
			.def_property_readonly("xmin_ymin", &cp::rectd::xmin_ymin)
			.def_property_readonly("xmax_ymin", &cp::rectd::xmax_ymin)
			.def_property_readonly("xmin_ymax", &cp::rectd::xmin_ymax)
			.def_property_readonly("xmax_ymax", &cp::rectd::xmax_ymax)
			.def_property_readonly("centerx", &cp::rectd::centerx)
			.def_property_readonly("centery", &cp::rectd::centery)
			.def_property_readonly("center", &cp::rectd::center)
			.def_property_readonly("has_positive_area", &cp::rectd::has_positive_area)
			.def_property_readonly("has_nonnegative_area", &cp::rectd::has_nonnegative_area)
				// methods
			.def("contains", &cp::rectd::contains)
			.def("fully_contains", &cp::rectd::fully_contains)
			.def("made_positive_average", &cp::rectd::made_positive_average)
			.def("made_positive_min", &cp::rectd::made_positive_min)
			.def("made_positive_max", &cp::rectd::made_positive_max)
			.def("made_positive_swap", &cp::rectd::made_positive_swap)
			.def("translated", &cp::rectd::translated)
			.def("scaled", &cp::rectd::scaled)
			.def("coordinates_scaled", &cp::rectd::coordinates_scaled)
			.def_static("common_part", cp::rectd::common_part)
			.def_static("bounding_box", cp::rectd::bounding_box);

		// TODO mat

		// TODO color

		{
			auto plugin_context = py::class_<cp::plugin_context>(m, "plugin_context")
				.def_readonly("settings", &cp::plugin_context::sett)
				.def_readonly("plugin_manager", &cp::plugin_context::plugin_man)
				.def_readonly("ui_manager", &cp::plugin_context::ui_man)
				.def_readonly("tab_manager", &cp::plugin_context::tab_man);
			plugin_context.def_property_readonly_static(
				"current",
				[](py::object) {
					return context;
				}
			);
		}
		py::class_<cp::plugin, py_plugin, std::shared_ptr<cp::plugin>>(m, "plugin")
			.def(py::init<>())
			.def("initialize", &cp::plugin::initialize)
			.def("finalize", &cp::plugin::finalize)
			.def("enable", &cp::plugin::enable)
			.def("disable", &cp::plugin::disable)
			.def_property_readonly("name", &cp::plugin::get_name)
			.def_property_readonly("is_enabled", &cp::plugin::is_enabled)
			.def_property_readonly("num_dependents", &cp::plugin::get_num_dependents);
		{
			auto plugin_manager = py::class_<cp::plugin_manager>(m, "plugin_manager");
			plugin_manager
				.def("attach", &cp::plugin_manager::attach)
				.def("detach", &cp::plugin_manager::detach)
				.def("find_plugin", &cp::plugin_manager::find_plugin);

			py::class_<cp::plugin_manager::handle>(plugin_manager, "handle")
				.def(py::init<>())
				.def_property_readonly("valid", &cp::plugin_manager::handle::valid)
				.def_property_readonly("name", &cp::plugin_manager::handle::get_name);
		}
	}
}
