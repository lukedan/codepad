// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Shared information.

#include <pybind11/pybind11.h>

#include <codepad/core/plugins.h>

// type casters
// https://pybind11.readthedocs.io/en/stable/advanced/cast/custom.html
namespace pybind11::detail {
	/// \p type_caster for \p std::u8string.
	template <> struct type_caster<std::u8string> {
	public:
		PYBIND11_TYPE_CASTER(std::u8string, _("std::u8string"));

		bool load(handle src, bool) {
			if (!src) {
				return false;
			}
			std::string_view view;
			std::string str;
			try { // first try casting to string_view
				view = src.cast<std::string_view>();
			} catch (const py::cast_error&) {
				// casting to string_view can fail sometimes because the interpreter cannot guarantee that
				// the object will live long enough, so we cast to string instead
				try {
					str = src.cast<std::string>();
				} catch (const py::cast_error&) {
					return false;
				}
				view = str;
			}
			value.resize(view.size());
			std::memcpy(value.data(), view.data(), sizeof(char) * view.size());
			return true;
		}

		static handle cast(std::u8string src, return_value_policy, handle) {
			return py::str(reinterpret_cast<const char*>(src.data()), src.size());
		}
	};

	/// \p type_caster for \p std::u8string_view.
	template <> struct type_caster<std::u8string_view> {
	public:
		PYBIND11_TYPE_CASTER(std::u8string_view, _("std::u8string_view"));

		bool load(handle src, bool) {
			if (!src) {
				return false;
			}
			std::string_view str;
			try {
				str = src.cast<std::string_view>();
			} catch (const py::cast_error&) {
				return false;
			}
			value = std::u8string_view(reinterpret_cast<const char8_t*>(str.data()), str.size());
			return true;
		}

		static handle cast(std::u8string_view src, return_value_policy, handle) {
			return py::str(reinterpret_cast<const char*>(src.data()), src.size());
		}
	};
}

#define TRY_OVERRIDE(RETURN_TYPE, CLASS_NAME, FUNC_NAME, ...)                                        \
	try {                                                                                            \
		PYBIND11_OVERLOAD(RETURN_TYPE, CLASS_NAME, FUNC_NAME, __VA_ARGS__);                          \
	} catch (const ::pybind11::error_already_set &_err) {                                            \
		::codepad::logger::get().log_error(CP_HERE) << _err.what() << ::codepad::logger::stacktrace; \
	}

#define TRY_OVERRIDE_PURE(RETURN_TYPE, CLASS_NAME, FUNC_NAME, ...)                                   \
	try {                                                                                            \
		PYBIND11_OVERLOAD_PURE(RETURN_TYPE, CLASS_NAME, FUNC_NAME, __VA_ARGS__);                     \
	} catch (const ::pybind11::error_already_set &_err) {                                            \
		::codepad::logger::get().log_error(CP_HERE) << _err.what() << ::codepad::logger::stacktrace; \
	}

namespace python_plugin_host_pybind11 {
	extern const codepad::plugin_context *context;
	extern codepad::plugin *this_plugin;

	/// Registers all core codepad classes.
	void register_core_classes(pybind11::module&);
	/// Registers all ui codepad classes.
	void register_ui_classes(pybind11::module&);
}
