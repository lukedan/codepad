// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of the python plugin host.

#include <filesystem>

#include <pybind11/embed.h>

namespace py = ::pybind11;

#include<codepad/core/plugins.h>

#include <plugin_defs.h>

#include "shared.h"

namespace cp = ::codepad;

namespace python_plugin_host_pybind11 {
	const cp::plugin_context *context = nullptr;
	cp::plugin *this_plugin = nullptr;

	const char *python_import_module = R"py(
def load_module(mod_name, p):
	import importlib.util
	spec = importlib.util.spec_from_file_location(mod_name, p)
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module

load_module(module_name, path)
	)py";

	void import_module(std::u8string_view module_name, const std::filesystem::path &path) {
		py::dict locals;
		try {
			locals["module_name"] = py::cast(module_name);
			locals["path"] = py::cast(path.c_str());
			py::eval<py::eval_statements>(python_import_module, py::globals(), locals);
		} catch (const py::error_already_set &err) {
			cp::logger::get().log_error() << err.what() << cp::logger::stacktrace;
		}
	}
}

using namespace python_plugin_host_pybind11;

PYBIND11_EMBEDDED_MODULE(pycodepad, m) {
	register_core_classes(m);

	py::module ui = m.def_submodule("ui");
	register_ui_classes(ui);
}

extern "C" {
	PLUGIN_INITIALIZE(ctx, this_plug) {
		context = &ctx;
		this_plugin = &this_plug;
		py::initialize_interpreter();

		/*import_module(u8"test_module", "test.py");*/
	}

	PLUGIN_FINALIZE() {
		py::finalize_interpreter();
		context = nullptr;
		this_plugin = nullptr;
	}

	PLUGIN_GET_NAME() {
		return "python_plugin_host_pybind11";
	}

	// enable/disable is not meaningful for plugin hosts
	PLUGIN_ENABLE() {
		// ignored
	}
	PLUGIN_DISABLE() {
		// ignored
	}
}
