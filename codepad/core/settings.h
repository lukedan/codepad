// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the settings system.

#include <variant>
#include <vector>
#include <string>
#include <functional>
#include <map>

#include "encodings.h"
#include "json.h"
#include "misc.h"

namespace codepad {
	/// A class that keeps track of all registered settings.
	class settings {
	public:
		/// Callback function invoked when the associated setting is changed. The first parameter is the old value
		/// and the second parameter is the new one (i.e., the current value).
		using changed_callback_t = std::function<void(const json::value_t&, const json::value_t&)>;
	protected:
		/// Data associated with one setting.
		struct _setting {
			changed_callback_t on_changed; ///< Callback function when the setting is changed.
			json::value_t value; ///< The current value of this setting.
		};
		using _registry_t = std::map<str_t, _setting>; ///< Mapping type for setting entries.
	public:
		/// A token returned after a setting has been successfully registered, which can be used to obtain the
		/// current value of that setting.
		struct token {
			friend settings;
		public:
			/// Default constructor.
			token() = default;

			/// Returns the current value of the setting.
			const json::value_t &value() const {
				return _it->second.value;
			}
			/// Returns whether this token is bound to a setting.
			bool is_empty() const {
				return _it == _it_type();
			}
		protected:
			/// The underlying iterator type.
			using _it_type = _registry_t::const_iterator;

			/// Initializes \ref _it with the given value.
			token(_it_type it) : _it(it) {
			}

			_it_type _it; ///< Iterator to the corresponding entry.
		};

		/// Registers a setting with the given name, parser, callback, and default value.
		///
		/// \param name The name of this setting.
		/// \param callback A \ref changed_callback_t that will be called when this setting is modified.
		/// \param default_value The default value of the setting. In this version of this function, it is only
		///                      moved if \p use_existing is \p false or if no prior value exists.
		/// \param use_existing Indicates whether previously unregistered values of this setting entry should be
		///                     preferred over \p default_value, if there exists one. If this is \p true and there
		///                     doesn't exist a value that has been parsed, this value will be set to \p false to
		///                     indicate that.
		token register_setting(
			str_t name, changed_callback_t callback, json::value_t &&value, bool &use_existing
		) {
			auto it = _register_setting_impl(std::move(name), std::move(callback), use_existing);
			if (it != _registry_t::iterator() && !use_existing) {
				it->second.value = std::move(value);
			}
			return token(it);
		}
		/// \overload
		token register_setting(
			str_t name, changed_callback_t callback, const json::value_t &value, bool &use_existing
		) {
			auto it = _register_setting_impl(std::move(name), std::move(callback), use_existing);
			if (it != _registry_t::iterator() && !use_existing) {
				it->second.value = value;
			}
			return token(it);
		}
		/// Shorthand for \ref register_setting() with \p use_existing set to \p true. This function assigns the
		/// result to the given \ref token, then checks if \p use_existing has been reset and if not, calls
		/// \p callback immediately.
		void register_setting(
			token &tk, str_t name, changed_callback_t callback = nullptr, json::value_t value = json::value_t()
		) {
			bool use_existing = true;
			token tok = register_setting(std::move(name), std::move(callback), std::move(value), use_existing);
			if (!tok.is_empty() && use_existing) { // value is not moved from
				if (tok._it->second.on_changed) {
					tok._it->second.on_changed(value, tok.value());
				}
			}
		}
	protected:
		_registry_t _entries; ///< The list of registered entries.
		/// Entries in the settings file that whose names are not registered.
		std::map<str_t, json::value_t> _untagged_entries;

		/// Implementation of \ref register_existing(), without setting the default value.
		_registry_t::iterator _register_setting_impl(
			str_t &&name, changed_callback_t &&callback, bool &use_existing
		) {
			auto[it, inserted] = _entries.try_emplace(std::move(name));
			if (!inserted) {
				logger::get().log_warning(CP_HERE, "setting already registered: ", it->first);
				return _registry_t::iterator();
			}
			it->second.on_changed = std::move(callback);
			if (use_existing) {
				auto extit = _untagged_entries.find(it->first);
				if (extit == _untagged_entries.end()) {
					use_existing = false;
				} else {
					it->second.value = std::move(extit->second);
					_untagged_entries.erase(extit);
				}
			}
			return it;
		}
	};
}
