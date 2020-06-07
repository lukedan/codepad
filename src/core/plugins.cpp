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

	void plugin_manager::attach(std::shared_ptr<plugin> p) {
		assert_true_usage(context != nullptr, "context should be set before loading plugins");
		p->initialize(*context);
		auto [it, inserted] = _plugins.try_emplace(p->get_name(), std::move(p));
		if (!inserted) {
			logger::get().log_warning(CP_HERE) <<
				"plugin " << it->first << " already exists";
		} else {
			logger::get().log_info(CP_HERE) << "plugin attached: " << it->second->get_name();
		}
	}

	void plugin_manager::detach(mapping_type::const_iterator it) {
		std::u8string name = it->second->get_name();
		if (it->second->is_enabled()) {
			it->second->disable();
		}
		it->second->finalize();
		logger::get().log_info(CP_HERE) << "plugin detached: " << name;
		_plugins.erase(it);
	}

	void plugin_manager::finalize_all() {
		for (auto &[name, plug] : _plugins) {
			if (plug->is_enabled()) {
				plug->disable();
			}
		}
		while (!_plugins.empty()) {
			auto it = _plugins.begin();
			it->second->finalize();
			_plugins.erase(it);
		}
	}


	void native_plugin::initialize(const plugin_context &ctx) {
		_init(ctx);
	}


#ifdef CP_ENABLE_APIGEN
	void apigen_plugin::initialize(plugin_manager &manager) {
		_init(manager._api_table);
	}
#endif
}
