// Copyright(c) the Codepad contributors.All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Independent permanent storage for JSON values.

#include <vector>
#include <map>
#include <variant>

#include "../encodings.h"
#include "misc.h"

namespace codepad::json {
	// forward declarations
	namespace storage {
		struct value_t;
		struct object_t;
		struct array_t;
	}

	/// Stores a JSON value.
	struct value_storage {
		using object = std::map<str_t, value_storage, std::less<>>; ///< A JSON object.
		using array = std::vector<value_storage>; ///< A JSON array.
		/// The \p std::variant type used to store any object.
		using storage = std::variant<
			null_t, bool,
			std::int64_t, std::uint64_t, double,
			str_t, object, array
		>;

		/// Default constructor.
		value_storage() = default;
		/// Initializes \ref value with the given arguments.
		template <typename T, typename ...Args> explicit value_storage(std::in_place_type_t<T>, Args &&...args) :
			value(std::in_place_type<T>, std::forward<Args>(args)...) {
		}

		/// Returns a corresponding \ref storage::value_t.
		json::storage::value_t get_value() const;

		storage value; ///< The value.
	};
	/// Constructs a \ref value_storage that holds a value of type \p T, initialized by the given arguments.
	template <typename T, typename ...Args> inline value_storage make_value_storage(Args && ...args) {
		return value_storage(std::in_place_type<T>, std::forward<Args>(args)...);
	}

	/// Returns the \ref value_storage corresponding to the given value.
	///
	/// \todo Rid of recursion?
	template <typename Value> inline value_storage store(const Value &v) {
		if (v.template is<null_t>()) {
			return make_value_storage<null_t>();
		}
		if (v.template is<bool>()) {
			return make_value_storage<bool>(v.get<bool>());
		}
		if (v.template is<std::int64_t>()) {
			return make_value_storage<std::int64_t>(v.get<std::int64_t>());
		}
		if (v.template is<std::uint64_t>()) {
			return make_value_storage<std::uint64_t>(v.get<std::uint64_t>());
		}
		if (v.template is<double>()) {
			return make_value_storage<double>(v.get<double>());
		}
		if (v.template is<str_view_t>()) {
			return make_value_storage<str_t>(v.get<str_view_t>());
		}
		if (v.template is<typename Value::object_t>()) {
			value_storage val(std::in_place_type<value_storage::object>);
			auto &dict = std::get<value_storage::object>(val.value);
			auto &&obj = v.get<typename Value::object_t>();
			for (auto it = obj.member_begin(); it != obj.member_end(); ++it) {
				dict.emplace(it.name(), store(it.value()));
			}
			return val;
		}
		if (v.template is<typename Value::array_t>()) {
			value_storage val(std::in_place_type<value_storage::array>);
			auto &list = std::get<value_storage::array>(val.value);
			for (auto &&n : v.get<typename Value::array_t>()) {
				list.emplace_back(store(n));
			}
			return val;
		}
		assert_true_logical(false, "JSON node with invalid type");
		return value_storage();
	}

	/// JSON interface for values stored in a \ref value_storage.
	namespace storage {
		/// Wrapper arounda a \ref value_storage::storage.
		struct value_t : public _details::value_type_base<value_t> {
			friend object_t;
			friend array_t;
		public:
			using object_t = object_t; ///< The object type.
			using array_t = array_t; ///< The array type.

			/// Default constructor.
			value_t() = default;
			/// Initializes this struct given a \ref value_storage.
			explicit value_t(const value_storage &v) : value_t(&v.value) {
			}

			/// Checks if the object is of the given type.
			template <typename T> bool is() const {
				if constexpr (std::is_same_v<T, null_t>) {
					return std::holds_alternative<null_t>(*_v);
				} else if constexpr (std::is_same_v<T, bool>) {
					return std::holds_alternative<bool>(*_v);
				} else if constexpr (std::is_same_v<T, str_t> || std::is_same_v<T, str_view_t>) {
					return std::holds_alternative<str_t>(*_v);
				} else if constexpr (std::is_same_v<T, object_t>) {
					return std::holds_alternative<value_storage::object>(*_v);
				} else if constexpr (std::is_same_v<T, array_t>) {
					return std::holds_alternative<value_storage::array>(*_v);
				} else if constexpr (std::is_floating_point_v<T>) {
					return
						std::holds_alternative<std::int64_t>(*_v) ||
						std::holds_alternative<std::uint64_t>(*_v) ||
						std::holds_alternative<double>(*_v);
				} else if constexpr (std::is_integral_v<T>) {
					if constexpr (std::is_same_v<T, std::uint64_t>) {
						return std::holds_alternative<std::uint64_t>(*_v);
					} else if constexpr (std::is_signed_v<T>) {
						return std::holds_alternative<std::int64_t>(*_v);
					} else {
						return
							std::holds_alternative<std::uint64_t>(*_v) ||
							std::holds_alternative<std::int64_t>(*_v);
					}
				}
			}
			/// Returns the value as the given type.
			template <typename T> T get() const;
		protected:
			const value_storage::storage *_v = nullptr; ///< The value.

			/// Initializes \ref _v directly.
			explicit value_t(const value_storage::storage *v) : _v(v) {
			}
		};
		/// Wrapper around a \ref value_storage::object.
		struct object_t : public _details::object_type_base<object_t> {
			friend value_t;
		public:
			/// The iterator type.
			struct iterator :
				public _details::bidirectional_iterator_wrapper<iterator, value_storage::object::const_iterator> {
				friend bidirectional_iterator_wrapper;
				friend object_t;
			public:
				/// Default constructor.
				iterator() = default;

				/// Returns the name of this member.
				str_view_t name() const {
					return _it->first;
				}
				/// Returns the value of this member.
				value_t value() const {
					return value_t(_it->second);
				}
			protected:
				/// Initializes the base class using the base iterator.
				explicit iterator(value_storage::object::const_iterator it) :
					bidirectional_iterator_wrapper(std::move(it)) {
				}
			};

			/// Default constructor.
			object_t() = default;

			/// Returns an iterator to the first element.
			iterator member_begin() const {
				return iterator(_obj->begin());
			}
			/// Returns an iterator past the last element.
			iterator member_end() const {
				return iterator(_obj->end());
			}
			/// Finds the member with the specified name.
			iterator find_member(str_view_t name) const {
				return iterator(_obj->find(name));
			}

			/// Returns the number of members.
			size_t size() const {
				return _obj->size();
			}
		protected:
			const value_storage::object *_obj = nullptr; ///< The object.

			/// Initializes \ref _obj directly.
			explicit object_t(const value_storage::object *o) : _obj(o) {
			}
		};
		/// Wrapper around a \ref value_storage::array.
		struct array_t {
			friend value_t;
		public:
			/// The iterator type.
			struct iterator :
				public _details::random_iterator_wrapper<iterator, value_storage::array::const_iterator> {
				friend random_iterator_wrapper;
				friend random_iterator_wrapper::base_t;
				friend array_t;
			public:
				/// Used to implement the \p -> operator.
				struct proxy {
					friend iterator;
				public:
					/// Returns a pointer to \ref _v.
					value_t *operator->() {
						return &_v;
					}
				protected:
					value_t _v; ///< The underlying value.

					/// Initializes \ref _v directly.
					explicit proxy(value_t v) : _v(std::move(v)) {
					}
				};

				/// Default constructor.
				iterator() = default;

				/// Returns the underlying object.
				value_t operator*() const {
					return value_t(*_it);
				}
				/// Returns a \ref proxy.
				proxy operator->() const {
					return proxy(value_t(*_it));
				}
			protected:
				/// Initializes the base class using the base iterator.
				explicit iterator(value_storage::array::const_iterator it) : random_iterator_wrapper(std::move(it)) {
				}
			};

			/// Default constructor.
			array_t() = default;

			/// Returns an iterator to the first element.
			iterator begin() const {
				return iterator(_arr->begin());
			}
			/// Returns an iterator past the last element.
			iterator end() const {
				return iterator(_arr->end());
			}

			/// Returns the element at the given index.
			value_t at(size_t i) const {
				return value_t(_arr->at(i));
			}
			/// Indexing operator.
			value_t operator[](size_t i) const {
				return at(i);
			}

			/// Returns the number of elements in this array.
			size_t size() const {
				return _arr->size();
			}
		protected:
			const value_storage::array *_arr = nullptr; ///< The array.

			/// Initializes \ref _arr directly.
			explicit array_t(const value_storage::array *a) : _arr(a) {
			}
		};

		template <typename T> T value_t::get() const {
			if constexpr (std::is_same_v<T, null_t>) {
				return std::get<null_t>(*_v);
			} else if constexpr (std::is_same_v<T, bool>) {
				return std::get<bool>(*_v);
			} else if constexpr (std::is_same_v<T, str_t> || std::is_same_v<T, str_view_t>) {
				return std::get<str_t>(*_v);
			} else if constexpr (std::is_same_v<T, object_t>) {
				const auto &obj = std::get<value_storage::object>(*_v); // TODO fuck msvc
				return object_t(&obj);
			} else if constexpr (std::is_same_v<T, array_t>) {
				const auto &arr = std::get<value_storage::array>(*_v);
				return array_t(&arr);
			} else {
				if constexpr (std::is_floating_point_v<T>) {
					if (std::holds_alternative<double>(*_v)) {
						return static_cast<T>(std::get<double>(*_v));
					}
				}
				if (std::holds_alternative<std::uint64_t>(*_v)) {
					return static_cast<T>(std::get<std::uint64_t>(*_v));
				}
				return static_cast<T>(std::get<std::int64_t>(*_v));
			}
		}
	}

	inline json::storage::value_t value_storage::get_value() const {
		return json::storage::value_t(*this);
	}
}
