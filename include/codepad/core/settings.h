// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Definition of the settings system.

#include <variant>
#include <vector>
#include <string>
#include <functional>
#include <map>
#include <set>
#include <optional>

#include "json/misc.h"
#include "json/storage.h"
#include "json/default_engine.h"
#include "encodings.h"
#include "misc.h"
#include "event.h"

namespace codepad {
	/// A class that keeps track of all registered settings.
	class settings {
	public:
		/// A particular set of settings that will only be in effect in a certain configuration. Profiles are
		/// separated from the parent profile on demand.
		class profile {
			friend settings;
		public:
			/// Initializes this profile given its parent and override settings.
			profile(const profile *p, const json::storage::object_t &v) : _value(v), _parent(p) {
			}

			/// Tries to find a child profile.
			profile *find_child(std::u8string_view);

			/// Retrieves a setting in this profile given its path. Note that parent profiles are not searched. To
			/// also search in parent profiles, use \ref retrieve().
			template <typename It> std::optional<json::storage::value_t> retrieve_here(It begin, It end) const {
				json::storage::object_t current = _value;
				std::decay_t<It> it = begin;
				while (it != end) {
					if (auto fmem = current.find_member(*it); fmem != current.member_end()) {
						if (++it == end) { // found it, return
							return fmem.value();
						}
						if (auto obj = fmem.value().template cast<json::storage::object_t>()) {
							current = obj.value();
						} else { // not an object
							return std::nullopt;
						}
					} else { // not found
						return std::nullopt;
					}
				}
				return std::nullopt;
			}
			/// Returns a setting in this profile given its path, looking into parent profiles if necessary.
			template <typename It> std::optional<json::storage::value_t> retrieve(It begin, It end) const {
				for (const profile *cur = this; cur; cur = cur->_parent) {
					if (auto res = cur->retrieve_here<It>(begin, end)) {
						return res;
					}
				}
				return std::nullopt;
			}

			/// Returns the parent profile.
			const profile *get_parent() const {
				return _parent;
			}
		protected:
			std::map<std::u8string, std::unique_ptr<profile>, std::less<void>> _children; ///< Children profiles.
			json::storage::object_t _value; ///< Override settings of this profile.
			const profile *_parent = nullptr; ///< The parent profile, or \p nullptr if this is the root profile.
		};
		/// Used to retrieve and interpret settings. Use \ref get_profile() to retrieve a \ref profile_value object
		/// that than retrieves a concrete value for the setting using \ref profile_value::get_value(). Since the
		/// creation of both \ref retriever_parser and \ref profile_value are somewhat heavy, and \ref profile_value
		/// provides caching for the retrieved setting value, consider storing these objects instead of crating new
		/// ones every time a setting value is needed. Alternatively, register to the \ref changed event and update
		/// these values manually.
		template <typename T> class retriever_parser {
		public:
			/// Used to parse JSON values into C++ structures. The default value should be used when the setting
			/// entry does not exist or when parsing fails. This fail state should be encoded either in this function
			/// or in the desired type \p T of this setting.
			using value_parser = std::function<T(std::optional<json::storage::value_t>)>;
			/// Stores the key and parser for a \ref retriever_parser.
			struct context {
				/// Default constructor.
				context() = default;
				/// Initializes all fields of this struct.
				context(std::vector<std::u8string> k, value_parser p) : key(std::move(k)), parser(std::move(p)) {
				}

				std::vector<std::u8string> key; ///< The key.
				value_parser parser; ///< The parser.
			};
			/// The value for a specific \ref profile.
			class profile_value {
			public:
				/// Initializes this \ref profile_value with the corresponding \ref retriever_parser.
				profile_value(retriever_parser &parser, profile_value *parent, std::u8string prof_key) :
					_profile_key(std::move(prof_key)), _parent(parent), _parser(parser) {
				}

				/// Returns the value, re-parsing it if necessary.
				const T &get_value() {
					if (_timestamp != _parser._settings._timestamp) {
						std::vector<std::u8string_view> profile;
						for (const profile_value *current = this; current->_parent; current = current->_parent) {
							profile.emplace_back(current->_profile_key);
						}
						_value = _parser.get_value(profile.rbegin(), profile.rend());
						// update timestamp
						_timestamp = _parser._settings._timestamp;
					}
					return _value;
				}

				/// Finds or creates a child with the given key.
				[[nodiscard]] profile_value &get_child_profile(std::u8string_view key) {
					auto it = _children.find(key);
					if (it != _children.end()) {
						return **it;
					}
					auto res = _children.emplace(std::make_unique<profile_value>(_parser, this, std::u8string(key)));
					assert_true_logical(res.second, "insertion should succeed");
					return **res.first;
				}
			protected:
				/// Used to compare two \ref profile_value instances.
				template <typename Cmp = std::less<>> struct _profile_value_ptr_compare {
					/// Allows \p std::set::find() to take \ref std::u8string_view as the key.
					using is_transparent = void;

					/// Overload for \p std::unique_ptr to \ref profile_value.
					inline static std::u8string_view get_key(const std::unique_ptr<profile_value> &ptr) {
						assert_true_logical(ptr != nullptr, "empty pointer to settings profile value");
						return ptr->_profile_key;
					}
					/// Overload for other string types.
					template <typename Str> inline static const Str &get_key(const Str &str) {
						return str;
					}

					/// Uses \p Cmp to compare the keys.
					template <typename Lhs, typename Rhs> bool operator()(const Lhs &lhs, const Rhs &rhs) const {
						Cmp cmp;
						return cmp(get_key(lhs), get_key(rhs));
					}
				};

				std::u8string _profile_key; ///< The key of this profile, relative to its parent.
				/// Getters for children profiles. Using set instead of unordered_set to save space.
				std::set<std::unique_ptr<profile_value>, _profile_value_ptr_compare<>> _children;
				T _value; ///< The value.
				/// The timestamp. This is set to 0 so that a fresh value will always be fetched after construction.
				std::size_t _timestamp = 0;
				profile_value *_parent = nullptr; ///< The \ref profile_value associated with the parent profile.
				retriever_parser &_parser; ///< The \ref retriever_parser that owns this object.
			};

			/// Constructs this \ref retriever_parser using the given setting, key, and parser.
			retriever_parser(settings &s, std::shared_ptr<context> ctx) : _context(std::move(ctx)), _settings(s) {
				_main = std::make_unique<profile_value>(*this, nullptr, u8"");
			}
			/// No copy construction.
			retriever_parser(const retriever_parser&) = delete;
			/// No copy assignment.
			retriever_parser &operator=(const retriever_parser&) = delete;

			/// Retrieves the \ref profile_value for the given profile name.
			template <typename It> profile_value &get_profile(It begin, It end) {
				profile_value *current = _main.get();
				for (std::decay_t<It> it = begin; it != end; ++it) {
					current = &current->get_child_profile(*it);
				}
				return *current;
			}
			/// Returns the main \ref profile_value.
			profile_value &get_main_profile() {
				return *_main;
			}

			/// Retrieves and parses the setting value directly without caching, for the deepest existing profile
			/// corresponding to the given profile name.
			template <typename It> T get_value(It begin, It end) {
				profile &prof = _settings.find_deepest_profile(begin, end);
				std::optional<json::storage::value_t> jsonval = prof.retrieve(
					_context->key.begin(), _context->key.end()
				);
				return _context->parser(jsonval);
			}
		protected:
			std::shared_ptr<context> _context; ///< The context of this \ref retriever_parser.
			std::unique_ptr<profile_value> _main; ///< The value corresponding to the main profile.
			settings &_settings; ///< The associated \ref settings object.
		};

		/// A thin wrapper around a \ref retriever_parser::profile_value.
		template <typename T> struct getter {
		public:
			/// The associated \ref retriever_parser::profile_value type.
			using profile_value = typename retriever_parser<T>::profile_value;

			/// Initializes \ref _value.
			getter(profile_value &pv) : _value(pv) {
			}

			/// Returns the associated \ref profile_value.
			profile_value &get_profile_value() const {
				return _value;
			}

			/// Retrieves the value.
			const T &get() const {
				return _value.get_value();
			}
			/// Shorthand for \ref get().
			const T &operator*() const {
				return get();
			}
			/// Shorthand for \ref get().
			const T *operator->() const {
				return &get();
			}
		protected:
			/// The associated \ref profile_value, which is used to retrieve the value of this setting.
			profile_value &_value;
		};

		/// Contains basic parsers for settings.
		struct basic_parsers {
			/// Parses the value by simply calling \p value_t::parse(). If the parse fails or if the setting is not
			/// present, the specified default value is returned.
			template <
				typename T, typename Parser = json::default_parser<T>
			> inline static typename retriever_parser<T>::value_parser basic_type_with_default(
				T def, Parser parser = Parser{}
			) {
				return [d = std::move(def), p = std::move(parser)](std::optional<json::storage::value_t> v) {
					if (v) {
						return v.value().parse<T>(p).value_or(d);
					}
					return d;
				};
			}
		};


		/// Default constructor.
		settings() = default;
		/// No move construction.
		settings(settings&&) = delete;
		/// No copy construction.
		settings(const settings&) = delete;
		/// No move assignment.
		settings &operator=(settings&&) = delete;
		/// No copy assignment.
		settings &operator=(const settings&) = delete;

		/// Finds the profile corresponding to the given key. Returns \p nullptr if no such profile is found.
		template <typename It> profile *find_profile_exact(It begin, It end) {
			profile *current = &get_main_profile();
			for (std::decay_t<It> it = begin; current && it != end; ++it) {
				current = current->find_child(*it);
			}
			return current;
		}
		/// Tries to find the deepest existing profile that matches the given key.
		template <typename It> profile &find_deepest_profile(It begin, It end) {
			profile *current = &get_main_profile();
			for (std::decay_t<It> it = begin; it != end; ++it) {
				profile *next = current->find_child(*it);
				if (next == nullptr) {
					break;
				}
				current = next;
			}
			return *current;
		}
		/// Returns the main profile.
		profile &get_main_profile();

		/// Returns a \ref retriever_parser for the given setting.
		template <typename T> [[nodiscard]] std::unique_ptr<retriever_parser<T>> create_retriever_parser(
			std::shared_ptr<typename retriever_parser<T>::context> ctx
		) {
			return std::make_unique<retriever_parser<T>>(*this, std::move(ctx));
		}
		/// Returns a \ref retriever_parser for the given setting. Use the other overload that accepts a
		/// \ref retriever_parser<T>::context whenever possible.
		template <typename T> [[nodiscard]] std::unique_ptr<retriever_parser<T>> create_retriever_parser(
			std::vector<std::u8string> key, typename retriever_parser<T>::value_parser p
		) {
			return create_retriever_parser<T>(std::make_shared<typename retriever_parser<T>::context>(
				std::move(key), std::move(p)
			));
		}

		/// Updates the value of all settings.
		void set(json::value_storage);
		/// Loads all settings from the given file.
		void load(const std::filesystem::path&);

		/// Invoked whenever the settings have been changed. Objects that are eager to detect settings changes can
		/// register for this event.
		info_event<void> changed;
	protected:
		json::value_storage _storage; ///< Stores all values.
		std::unique_ptr<profile> _main_profile; ///< The main profile.
		std::size_t _timestamp = 1; ///< The version of loaded settings.
	};

	/// A utility class used to access settings that handles the case of multiple \ref settings objects. Note that
	/// actually using mutliple \ref setting objects can degrade performance.
	template <typename T> struct setting {
	public:
		using parser_type = typename settings::retriever_parser<T>::value_parser; ///< Parser type.

		/// Initializes this setting given the key and the parser.
		setting(std::vector<std::u8string> k, parser_type p) :
			_context(std::make_shared<typename settings::retriever_parser<T>::context>(std::move(k), std::move(p))) {
		}

		/// Returns the \ref settings::retriever_parser associated with the given \ref settings object.
		settings::retriever_parser<T> &get(settings &s) {
			if (&s != _settings) { // slow path
				if (_settings) { // put the current retriever back onto the shelf
					_slow_map.emplace(_settings, std::move(_retriever));
				}
				_settings = &s;
				auto it = _slow_map.find(_settings);
				if (it == _slow_map.end()) { // create new retriever
					_retriever = _settings->create_retriever_parser<T>(_context);
				} else { // take the retriever out & erase the entry
					_retriever = std::move(it->second);
					_slow_map.erase(it);
				}
			}
			return *_retriever;
		}
	private:
		std::shared_ptr<typename settings::retriever_parser<T>::context> _context; ///< The context of this setting.
		/// A mapping for \ref settings objects that are not currently active.
		std::map<settings*, std::unique_ptr<settings::retriever_parser<T>>> _slow_map;
		std::unique_ptr<settings::retriever_parser<T>> _retriever; ///< The \ref settings::retriever_parser.
		settings *_settings = nullptr; ///< The current \ref settings object.
	};
}
