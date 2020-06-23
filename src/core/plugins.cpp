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
	void plugin::add_dependency(plugin &dep) {
		assert_true_usage(&dep != this, "cannot add self as dependency");
		_dependencies.emplace_back(dep.shared_from_this());
		++dep._num_dependents;
	}

	void plugin::add_dependency(plugin_manager::handle h) {
		add_dependency(*h._it.value()->second);
	}


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

	plugin_manager::handle plugin_manager::attach(std::shared_ptr<plugin> p) {
		assert_true_usage(context != nullptr, "context should be set before loading plugins");
		p->initialize(*context);
		auto [it, inserted] = _plugins.try_emplace(p->get_name(), std::move(p));
		if (!inserted) {
			logger::get().log_warning(CP_HERE) <<
				"plugin " << it->first << " already exists";
			return handle();
		} else {
			logger::get().log_info(CP_HERE) << "plugin attached: " << it->second->get_name();
		}
		return handle(it);
	}

	void plugin_manager::detach(handle h) {
		assert_true_usage(h.valid(), "plugin plugin handle");
		mapping_type::const_iterator it = h._it.value();
		if (it->second->get_num_dependents() > 0) { // there are still plugins that depend on this one
			return;
		}

		_finalize_plugin(*it->second);
		_on_plugin_detached(it->second);
		_plugins.erase(it);
	}

	void plugin_manager::shutdown() {
		std::vector<std::shared_ptr<plugin>> unreferenced;
		for (auto it = _plugins.begin(); it != _plugins.end(); ) {
			if (it->second->get_num_dependents() == 0) {
				unreferenced.emplace_back(std::move(it->second));
				it = _plugins.erase(it);
			} else {
				++it;
			}
		}
		// iteratively finalize (and maybe unload) plugins
		while (unreferenced.size() > 0) {
			std::shared_ptr<plugin> ptr = std::move(unreferenced.back());
			unreferenced.pop_back();
			_finalize_plugin(*ptr);
			// update the list of plugins that have no dependents
			for (const auto &dep : ptr->_dependencies) {
				if (dep->get_num_dependents() == 1) { // this is the only plugin that depends on it
					unreferenced.emplace_back(dep);
					// erase the plugin from _plugins
					assert_true_usage(
						_plugins.erase(dep->get_name()) == 1,
						"get_name() of a plugin must return the same value at all times"
					);
				}
			}
			// this function clears plugin::_dependencies,
			// so we update the list of plugin that have no dependnets before this
			_on_plugin_detached(std::move(ptr));
		}

		if (!_plugins.empty()) {
			logger::get().log_warning(CP_HERE) << "cycles in the plugin dependency graph";
			// at this point, if there are still entries in _plugins
			// then there must be cycles in the dependency graph
			// TODO figure out a better way to take apart the cycle
			while (!_plugins.empty()) {
				auto it = _plugins.begin();
				_finalize_plugin(*it->second);
				it->second->_dependencies.clear();
				_plugins.erase(it);
			}
		}
	}

	plugin_manager::handle plugin_manager::find_plugin(std::u8string_view name) const {
		auto it = _plugins.find(name);
		if (it == _plugins.end()) {
			return handle();
		}
		return handle(std::move(it));
	}

	void plugin_manager::_finalize_plugin(plugin &p) const {
		if (p.is_enabled()) {
			p.disable();
		}
		p.finalize();
		logger::get().log_info(CP_HERE) << "plugin finalized: " << p.get_name();
	}

	void plugin_manager::_on_plugin_detached(std::shared_ptr<plugin> ptr) {
		// notify all dependencies
		for (auto &dep : ptr->_dependencies) {
			--dep->_num_dependents;
		}
		ptr->_dependencies.clear(); // clear pointers so that they can be freed properly
		if (!ptr->_is_managed()) { // move it to the detached plugins list
			_detached_plugins.emplace_back(std::move(ptr));
		} // otherwise just discard the pointer
	}


	native_plugin::native_plugin(const std::filesystem::path &path) : _lib(path) {
		if (_lib.valid()) {
			_init = _lib.find_symbol<initialize_func_t>(u8"initialize");
			_finalize = _lib.find_symbol<finalize_func_t>(u8"finalize");
			_get_name = _lib.find_symbol<get_name_func_t>(u8"get_name");
			_enable = _lib.find_symbol<enable_func_t>(u8"enable");
			_disable = _lib.find_symbol<disable_func_t>(u8"disable");
		}
	}

	void native_plugin::diagnose() const {
		if (!_lib.valid()) {
			logger::get().log_warning(CP_HERE) << "failed to load dynamic library";
		} else {
			if (_init == nullptr) {
				logger::get().log_warning(CP_HERE) << "initialize() symbol not found in dynamic library";
			}
			if (_finalize == nullptr) {
				logger::get().log_warning(CP_HERE) << "finalize() symbol not found in dynamic library";
			}
			if (_get_name == nullptr) {
				logger::get().log_warning(CP_HERE) << "get_name() symbol not found in dynamic library";
			}
			if (_enable == nullptr) {
				logger::get().log_warning(CP_HERE) << "enable() symbol not found in dynamic library";
			}
			if (_disable == nullptr) {
				logger::get().log_warning(CP_HERE) << "disable() symbol not found in dynamic library";
			}
		}
	}

	void native_plugin::initialize(const plugin_context &ctx) {
		_init(ctx, *this);
	}
}
