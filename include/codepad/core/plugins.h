// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Plugin manager.

#include <map>
#include <memory>
#include <filesystem>

#include "misc.h"
#include "assert.h"
#include "logging.h"
#include "../apigen_definitions.h"
#include "../os/dynamic_library.h"

#ifdef CP_ENABLE_APIGEN
struct CP_API_STRUCT;
#endif

namespace codepad {
	class plugin_manager;
	class settings;
	namespace ui {
		class manager;
		namespace tabs {
			class tab_manager;
		}
	}

	/// Provides context information to plugins. A pointer to this object is passed to each plugin in
	/// \ref plugin::initialize(), which the plugin can (and probably would want to) hold on to. However, this should
	/// **not** be used after \ref plugin::finalize() has been called.
	struct plugin_context {
		settings *sett = nullptr; ///< Global \ref settings.
		plugin_manager *plugin_man = nullptr; ///< Global \ref plugin_manager.
		ui::manager *ui_man = nullptr; ///< Global \ref ui::manager.
		ui::tabs::tab_manager *tab_man = nullptr; ///< Global \ref ui::tabs::manager.
	};

	/// Abstract interface for plugins.
	class plugin {
		friend plugin_manager;
	public:
		/// Default virtual destructor.
		virtual ~plugin() = default;

		/// Attaches this plugin to the given \ref plugin_manager and loads the plugin. During the lifetime of this
		/// plugin, this function will be called exactly once.
		virtual void initialize(const plugin_context&) = 0;
		/// Finalizes the plugin. \ref disable() will (should) be called first if the plugin is currently enabled.
		virtual void finalize() {
			if (_enabled) {
				disable();
			}
		}

		/// Returns the name of this plugin. Call this only after this plugin has been initialized.
		virtual std::u8string get_name() const = 0;

		/// Enables this plugin.
		void enable() {
			assert_true_logical(!_enabled, "calling enable() on a plugin that is already enabled");
			_enabled = true;
			_on_enabled();
		}
		/// Disables this plugin.
		void disable() {
			assert_true_logical(_enabled, "calling enable() on a plugin that is already disabled");
			_enabled = false;
			_on_disabled();
		}
		/// Returns whether this plugin is currently enabled.
		bool is_enabled() const {
			return _enabled;
		}
	private:
		bool _enabled = false; ///< Whether this plugin is currently enabled.

		/// Called in \ref enable().
		virtual void _on_enabled() = 0;
		/// Called in \ref disable().
		virtual void _on_disabled() = 0;
	};
	class native_plugin;

	/// Plugin manager.
	class APIGEN_EXPORT_RECURSIVE plugin_manager {
		friend native_plugin;
	public:
		/// Mapping from plugin names and plugin objects.
		using mapping_type = std::map<std::u8string, std::shared_ptr<plugin>, std::less<>>;

		/// Initializes \ref _api_table.
		plugin_manager();
		/// Deletes \ref _api_table.
		~plugin_manager();
		plugin_manager(const plugin_manager&) = delete; // HACK apigen
		plugin_manager &operator=(const plugin_manager&) = delete; // HACK apigen

		/// Attaches the given plugin to this manager and initialize it.
		void attach(std::shared_ptr<plugin>);
		/// Disables the plugin if it's enabled, then finalizes the plugin and removes it from the registered list of
		/// plugins. It will then be destroyed if there are no other references to it. Generally this should not be
		/// used with non-apigen native plugins as unloading these plugins early means all memories allocated by them
		/// are quietly invalidated and will cause crashes once the memories are used.
		void detach(mapping_type::const_iterator);

		/// Returns all loaded plugins.
		const mapping_type &get_loaded_plugins() const {
			return _plugins;
		}

		/// Finalizes all plugins, without unloading them.
		void finalize_all();

		const plugin_context *context = nullptr; ///< The context for all plugins.
	protected:
		mapping_type _plugins; ///< All loaded plugins.
#ifdef CP_ENABLE_APIGEN
		/// The table of API function pointers. This is a raw pointer because we don't include apigen host header in
		/// this file, thus \p CP_API_STRUCT is an incomplete type.
		CP_API_STRUCT *_api_table;
#endif
	};

	/// A basic dynamic library plugin.
	class native_plugin : public plugin {
	public:
		/// Function pointer type used to initialize the plugin.
		using initialize_func_t = void (*)(const plugin_context&);
		using finalize_func_t = void (*)(); ///< Function pointer used to finalize the plugin.
		using get_name_func_t = const char8_t *(*)(); ///< Function pointer used to retrieve the name of a plugin.
		using enable_func_t = void (*)(); ///< Function pointer used to enable the plugin.
		using disable_func_t = void (*)(); ///< Function pointer used to disable the plugin.

		/// Loads the dynamic library. It's recommended to call \ref valid() afterwards to check if it has been
		/// successfully loaded.
		explicit native_plugin(const std::filesystem::path &path) : _lib(path) {
			if (_lib.valid()) {
				_init = _lib.find_symbol<initialize_func_t>(u8"initialize");
				_finalize = _lib.find_symbol<finalize_func_t>(u8"finalize");
				_get_name = _lib.find_symbol<get_name_func_t>(u8"get_name");
				_enable = _lib.find_symbol<enable_func_t>(u8"enable");
				_disable = _lib.find_symbol<disable_func_t>(u8"disable");
			}
		}

		/// Returns whether the dynamic library has been successfully loaded and all required symbols have been
		/// found.
		bool valid() const {
			return
				_lib.valid() &&
				_init != nullptr && _finalize != nullptr && _get_name != nullptr &&
				_enable != nullptr && _disable != nullptr;
		}
		/// Logs the reason why this plugin is not valid.
		void diagnose() const {
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

		/// Invokes \ref _init.
		void initialize(const plugin_context&) override;
		/// Invokes \ref _finalize.
		void finalize() override {
			plugin::finalize();
			_finalize();
		}

		/// Invokes \ref _get_name.
		std::u8string get_name() const override {
			return _get_name();
		}
	protected:
		os::dynamic_library _lib; ///< Reference to the dynamic library.
		initialize_func_t _init = nullptr; ///< The initialize function.
		finalize_func_t _finalize = nullptr; ///< Used to finalize the plugin.
		get_name_func_t _get_name = nullptr; ///< Used to get the name of the plugin.
		enable_func_t _enable = nullptr; ///< Used to enable the plugin.
		disable_func_t _disable = nullptr; ///< Used to disable the plugin.

		/// Invokes \ref _enable.
		void _on_enabled() override {
			_enable();
		}
		/// Invokes \ref _disable.
		void _on_disabled() override {
			_disable();
		}
	};
}
