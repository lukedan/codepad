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
	class settings;
	class plugin_manager;
	class plugin;
	class native_plugin;
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


	/// Plugin manager.
	class APIGEN_EXPORT_RECURSIVE plugin_manager {
		friend native_plugin;
	public:
		/// Mapping from plugin names and plugin objects.
		using mapping_type = std::map<std::u8string, std::shared_ptr<plugin>, std::less<>>;
		/// A handle to a plugin. Do not keep objects of this type around - this is only intended for referencing
		/// plugins temporarily.
		struct handle {
			friend plugin;
			friend plugin_manager;
		public:
			/// Default constructor.
			handle() = default;

			/// Returns \p true if this handle has an associated \ref plugin.
			bool valid() const {
				return _it.has_value();
			}
			/// Returns the name of the underlying plugin. This handle must be valid.
			const std::u8string &get_name() const {
				assert_true_usage(valid(), "plugin handle must be valid for get_name()");
				return _it.value()->first;
			}
		private:
			std::optional<mapping_type::const_iterator> _it; ///< Iterator to this plugin in the manager.

			/// Initializes \ref _it.
			explicit handle(mapping_type::const_iterator it) : _it(std::move(it)) {
			}
		};

		/// Initializes \ref _api_table.
		plugin_manager();
		/// Deletes \ref _api_table.
		~plugin_manager();

		/// Attaches the given plugin to this manager and initialize it. Does nothing if a plugin with the name
		/// already exists.
		handle attach(std::shared_ptr<plugin>);
		/// Detaches the given plugin. This has no effect if any other plugin depends on it. This method first
		/// disables the plugin if it's enabled, then finalizes the plugin. For managed plugins, this method simply
		/// unloads that plugin. For native plugins, this function moves it to \ref _detached_plugins.
		void detach(handle);

		/// Finalizes and unloads all plugins.
		void shutdown();

		/// Finds the plugin with the given name. If no such plugin exists, returns an empty handle.
		handle find_plugin(std::u8string_view) const;

		const plugin_context *context = nullptr; ///< The context for all plugins.
	protected:
		mapping_type _plugins; ///< All loaded plugins.
		std::vector<std::shared_ptr<plugin>> _detached_plugins; ///< The list of detached plugins.
#ifdef CP_ENABLE_APIGEN
		/// The table of API function pointers. This is a raw pointer because we don't include apigen host header in
		/// this file, thus \p CP_API_STRUCT is an incomplete type.
		CP_API_STRUCT *_api_table;
#endif

		/// Disables and finalizes the plugin.
		void _finalize_plugin(plugin&) const;
		/// Called to when a plugin is detached. This function updates and clears (\ref plugin::_dependencies) the
		/// dependencies of the given plugin, and moves it to \ref _detached_plugins if necessary.
		void _on_plugin_detached(std::shared_ptr<plugin>);
	};


	/// Abstract interface for plugins.
	class plugin : public std::enable_shared_from_this<plugin> {
		friend plugin_manager;
	public:
		/// Default virtual destructor.
		virtual ~plugin() = default;

		/// Attaches this plugin to the given \ref plugin_manager and loads the plugin. During the lifetime of this
		/// plugin, this function will be called exactly once.
		virtual void initialize(const plugin_context&) = 0;
		/// Finalizes the plugin. In this function, the plugin should release all references to objects in the main
		/// program or other plugins it depends on.
		virtual void finalize() {
		}

		/// Returns the name of this plugin. Call this only after this plugin has been initialized.
		virtual std::u8string get_name() const = 0;

		/// Enables this plugin.
		void enable() {
			assert_true_usage(!_enabled, "calling enable() on a plugin that is enabled");
			_enabled = true;
			_on_enabled();
		}
		/// Disables this plugin.
		void disable() {
			assert_true_usage(_enabled, "calling disable() on a plugin that is already disabled");
			_enabled = false;
			_on_disabled();
		}

		/// Returns whether this plugin is currently enabled.
		bool is_enabled() const {
			return _enabled;
		}
		/// Returns the number of plugins that depends on this one.
		std::size_t get_num_dependents() const {
			return _num_dependents;
		}

		/// Adds a dependency to the other plugin.
		void add_dependency(plugin&);
		/// \overload
		void add_dependency(plugin_manager::handle);
	protected:
		/// Called in \ref enable().
		virtual void _on_enabled() = 0;
		/// Called in \ref disable().
		virtual void _on_disabled() = 0;
	private:
		std::vector<std::shared_ptr<plugin>> _dependencies; ///< Plugins that this plugin depend on.
		/// The number of plugins that depend on this plugin.
		std::size_t _num_dependents = 0;
		bool _enabled = false; ///< Whether this plugin is currently enabled.

		/// Returns \p true if this extension is managed, i.e., that it depends entirely on another plugin for its
		/// execution, and that it's safe to destroy this plugin object early because the memory is managed by that
		/// plugin. All other plugins will only be unloaded when the program is shutting down.
		virtual bool _is_managed() const {
			return false;
		}
	};

	/// A basic dynamic library plugin.
	class native_plugin : public plugin {
	public:
		/// Function pointer type used to initialize the plugin.
		using initialize_func_t = void (*)(const plugin_context&, plugin&);
		using finalize_func_t = void (*)(); ///< Function pointer used to finalize the plugin.
		using get_name_func_t = const char8_t *(*)(); ///< Function pointer used to retrieve the name of a plugin.
		using enable_func_t = void (*)(); ///< Function pointer used to enable the plugin.
		using disable_func_t = void (*)(); ///< Function pointer used to disable the plugin.

		/// Loads the dynamic library. It's recommended to call \ref valid() afterwards to check if it has been
		/// successfully loaded.
		explicit native_plugin(const std::filesystem::path&);

		/// Returns whether the dynamic library has been successfully loaded and all required symbols have been
		/// found.
		bool valid() const {
			return
				_lib.valid() &&
				_init != nullptr && _finalize != nullptr && _get_name != nullptr &&
				_enable != nullptr && _disable != nullptr;
		}
		/// Logs the reason why this plugin is not valid.
		void diagnose() const;

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
