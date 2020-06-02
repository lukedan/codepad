// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of the plugin manager.

#include "codepad/core/plugins.h"

#ifdef CP_ENABLE_APIGEN
#	include CP_APIGEN_API_H
#	include CP_APIGEN_HOST_H
#endif

namespace codepad {
	plugin_manager::plugin_manager() {
#ifdef CP_ENABLE_APIGEN
		_api_table = new CP_API_STRUCT();
		CP_API_STRUCT_INIT(*_api_table);
#endif
	}

	plugin_manager::~plugin_manager() {
#ifdef CP_ENABLE_APIGEN
		delete _api_table;
#endif
	}

	plugin &plugin_manager::attach(std::unique_ptr<plugin> p) {
		p->initialize(*this);
		auto [it, inserted] = _plugins.try_emplace(p->get_name(), std::move(p));
		if (!inserted) {
			// TODO decide whether to replace the old plugin
			logger::get().log_warning(CP_HERE) <<
				"plugin " << it->first << " already exists, the newly created one is destroyed";
		}
		return *it->second;
	}

	void plugin_manager::finalize_all() {
		for (auto &[name, plug] : _plugins) {
			plug->finalize();
		}
	}


	void native_plugin::initialize(plugin_manager&) {
		_init();
	}


#ifdef CP_ENABLE_APIGEN
	void apigen_plugin::initialize(plugin_manager &manager) {
		_init(manager._api_table);
	}
#endif
}
