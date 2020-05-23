// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Implementation of the plugin manager.

#include "plugins.h"

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


	void native_plugin::initialize(plugin_manager&) {
		_init();
	}


#ifdef CP_ENABLE_APIGEN
	void apigen_plugin::initialize(plugin_manager &manager) {
		_init(manager._api_table);
	}
#endif
}
