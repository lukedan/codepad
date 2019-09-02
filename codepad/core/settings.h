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
#include <set>
#include <optional>

#include "json/misc.h"
#include "json/storage.h"
#include "json/rapidjson.h"
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
			profile *find_child(str_view_t name) {
				// return the already separated profile
				if (auto it = _children.find(name); it != _children.end()) {
					return it->second.get();
				}
				// try to find & separate the profile
				str_t key;
				key.reserve(name.length() + 2);
				key += '[';
				key += name;
				key += ']';
				if (auto fmem = _value.find_member(key); fmem != _value.member_end()) { // yes
					if (auto obj = fmem.value().cast<json::storage::object_t>()) {
						auto result = _children.emplace(str_t(name), std::make_unique<profile>(this, obj.value()));
						assert_true_logical(result.second, "map insert should have succeeded");
						return result.first->second.get();
					}
				}
				return nullptr; // nope
			}

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
			std::map<str_t, std::unique_ptr<profile>, std::less<void>> _children; ///< Children profiles.
			json::storage::object_t _value; ///< Override settings of this profile.
			const profile *_parent = nullptr; ///< The parent profile, or \p nullptr if this is the root profile.
		};
		/// Used to retrieve and interpret settings. Only the key, the parser and the mapping between profile names
		/// and profile getters. Instances of this struct are intended to be used as global variables.
		template <typename T> class retriever_parser {
		public:
			/// Used to parse JSON values into C++ structures. The default value should be used when the setting
			/// entry does not exist or when parsing fails. This fail state should be encoded either in this function
			/// or in the desired type \p T of this setting.
			using value_parser = std::function<T(std::optional<json::storage::value_t>)>;
			/// The value for a specific \ref profile.
			class profile_value {
			public:
				/// Initializes this \ref profile_value with the corresponding \ref retriever_parser.
				profile_value(retriever_parser &b, profile_value *p, str_t key) :
					_key(std::move(key)), _parent(p), _base(b) {
				}

				/// Returns the value, re-parsing it if necessary.
				const T &get_value() {
					settings &value = _base._parent;
					if (_timestamp != value._timestamp) {
						assert_true_logical(
							(this != &_base.get_main_profile()) == (_parent != nullptr), "invalid parent"
						);
						std::optional<json::storage::value_t> jsonval;
						if (_parent) { // not the main profile
							// gather the (reversed) key
							std::vector<str_view_t> key;
							for (const profile_value *current = this; current->_parent; current = current->_parent) {
								key.emplace_back(current->_key);
							}
							profile &prof = value.find_deepest_profile(key.rbegin(), key.rend());
							jsonval = prof.retrieve(_base._key.begin(), _base._key.end());
						} else { // just a small optimization to skip key collection & profile searching
							jsonval = value.get_main_profile().retrieve(_base._key.begin(), _base._key.end());
						}
						T newval = _base.parser(jsonval);
						if (newval != _value) {
							_value = std::move(newval);
						}
						// update timestamp
						_timestamp = value._timestamp;
					}
					return _value;
				}

				/// Finds or creates a child with the given key.
				profile_value &get_child_profile(str_view_t key) {
					auto it = _children.find(key);
					if (it != _children.end()) {
						return **it;
					}
					auto res = _children.emplace(std::make_unique<profile_value>(_base, this, str_t(key)));
					assert_true_logical(res.second, "insertion should succeed");
					return **res.first;
				}
			protected:
				/// Used to compare two \ref profile_value instances.
				template <typename Cmp = std::less<void>> struct _profile_value_ptr_compare {
					/// Allows \p std::set::find() to take \ref str_view_t as the key.
					using is_transparent = std::true_type;

					/// Overload for \p std::unique_ptr to \ref profile_value.
					inline static str_view_t get_key(const std::unique_ptr<profile_value> &ptr) {
						assert_true_logical(ptr != nullptr, "empty pointer to settings profile value");
						return ptr->_key;
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

				str_t _key; ///< The key of this profile, relative to its parent.
				/// Getters for children profiles.
				std::set<std::unique_ptr<profile_value>, _profile_value_ptr_compare<>> _children;
				T _value; ///< The value.
				/// The timestamp. This is set to 0 so that a fresh value will always be fetched after construction.
				size_t _timestamp = 0;
				profile_value *_parent; ///< The parent \ref profile_value.
				retriever_parser &_base; ///< The associated \ref retriever_parser.
			};

			/// Constructs this \ref retriever_parser using the given setting, key, and parser.
			retriever_parser(settings &s, std::vector<str_t> k, value_parser p) :
				parser(std::move(p)), _key(std::move(k)), _main(*this, nullptr, ""), _parent(s) {
			}

			/// Retrieves the \ref profile_value for the given profile name.
			template <typename It> profile_value &get_profile(It begin, It end) {
				profile_value *current = &_main;
				for (std::decay_t<It> it = begin; it != end; ++it) {
					current = &current->get_child_profile(*it);
				}
				return *current;
			}
			/// Returns the main \ref profile_value.
			profile_value &get_main_profile() {
				return _main;
			}

			/// Used to parse the value. Note that the result should *not* hold onto any resource that belong to the
			/// input JSON value (e.g., string views).
			value_parser parser;
		protected:
			profile_value _main; ///< The value corresponding to the main profile.
			std::vector<str_t> _key; ///< The key of this setting.
			settings &_parent; ///< The associated \ref settings object.
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
		profile &get_main_profile() {
			if (!_main_profile) {
				if (!_storage.get_value().is<json::storage::object_t>()) {
					_storage.value.emplace<json::value_storage::object>();
				}
				_main_profile = std::make_unique<profile>(
					nullptr, _storage.get_value().get<json::storage::object_t>()
					);
			}
			return *_main_profile;
		}

		/// Returns a \ref retriever_parser for the given setting.
		template <typename T> retriever_parser<T> create_retriever_parser(
			std::vector<str_t> key, typename retriever_parser<T>::value_parser parser
		) {
			return retriever_parser<T>(*this, std::move(key), std::move(parser));
		}

		/// Updates the value of all settings.
		void set(json::value_storage val) {
			++_timestamp;
			_main_profile = nullptr;
			_storage = std::move(val);
			changed.invoke();
		}
		/// Loads all settings from the given file.
		void load(const std::filesystem::path &path) {
			set(json::store(json::parse_file<json::rapidjson::document_t>(path).root()));
		}

		/// Returns the global \ref settings object.
		static settings &get();

		/// Invoked whenever the settings have been changed. Objects that are eager to detect settings changes can
		/// register for this event.
		info_event<void> changed;
	protected:
		json::value_storage _storage; ///< Stores all values.
		std::unique_ptr<profile> _main_profile; ///< The main profile.
		size_t _timestamp = 1; ///< The version of loaded settings.
	};
}
