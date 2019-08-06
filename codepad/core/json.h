// Copyright(c) the Codepad contributors.All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Wrappers and misceallenous functions for JSON libraries.

#include <vector>
#include <map>
#include <string>
#include <variant>
#include <fstream>

#include <rapidjson/document.h>

#include "encodings.h"
#include "misc.h"

namespace codepad::json {
	// JSON engine interface:
	//
	// document_t:
	//   actaully stores all data, movable, not copyable
	//
	//   root()
	//   static parse()
	//
	// value_t:
	//   an reference of the immutable underlying value, unless for storage implementation
	//
	//   is<T>, get<T>
	//     T: null_t, bool, number_t, unsigned, signed, double, array_t, object_t, u8string, u8string_view
	//
	// object_t:
	//   an reference of the immutable underlying value, unless for storage implementation
	//
	//   iterator, const_iterator
	//     name(): u8string_view, value(): value_t
	//   find_member(), member_begin(), member_end()
	//   size()
	//
	// array_t (of value_t):
	//   an reference of the immutable underlying value, unless for storage implementation
	//
	//   iterator
	//     operator*(), operator->(): value_t
	//   begin(), end()
	//   at(), operator[]()
	//   size()

	/// Dummy struct used to represent the value of \p null.
	struct null_t {
	};

	namespace _details {
		/// Wrapper for a bidirectional iterator that supports custom value types.
		template <typename Derived, typename BaseIt> struct bidirectional_iterator_wrapper {
		public:
			/// The difference type retrieved using \p std::iterator_traits.
			using difference_type = typename std::iterator_traits<BaseIt>::difference_type;
			/// The iterator category retrieved using \p std::iterator_traits.
			using iterator_category = typename std::iterator_traits<BaseIt>::iterator_category;

			/// Default constructor.
			bidirectional_iterator_wrapper() = default;
			/// Default virtual constructor, just in case.
			virtual ~bidirectional_iterator_wrapper() = default;

			/// Pre-increment.
			Derived &operator++() {
				++_it;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Post-increment.
			Derived operator++(int) {
				return Derived(_it++);
			}
			/// Pre-increment.
			Derived &operator--() {
				--_it;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Post-increment.
			Derived operator--(int) {
				return Derived(_it--);
			}

			/// Equality.
			friend bool operator==(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) == _get_it(rhs);
			}
			/// Inequality.
			friend bool operator!=(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) != _get_it(rhs);
			}
		protected:
			BaseIt _it{}; ///< The underlying iterator.

			/// Initializes \ref _it directly.
			bidirectional_iterator_wrapper(BaseIt b) : _it(std::move(b)) {
			}
		private:
			/// Returns the base iterator for the given derived object.
			inline static const BaseIt &_get_it(const Derived &d) {
				return d._it;
			}
		};

		/// Wrapper for a random access iterator that supports custom value types.
		template <typename Derived, typename BaseIt> struct random_iterator_wrapper :
			public bidirectional_iterator_wrapper<Derived, BaseIt> {
		public:
			using base_t = bidirectional_iterator_wrapper<Derived, BaseIt>; ///< The base type for this struct.
			/// \sa bidirectional_iterator_wrapper::difference_type
			using difference_type = bidirectional_iterator_wrapper<Derived, BaseIt>::difference_type;

			/// In-place addition.
			Derived &operator+=(difference_type diff) {
				base_t::_it += diff;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Addition.
			friend Derived operator+(const Derived &it, difference_type diff) {
				return Derived(_get_it(it) + diff);
			}
			/// Addition.
			friend Derived operator+(difference_type diff, const Derived &it) {
				return Derived(diff + _get_it(it));
			}

			/// In-place subtraction.
			Derived &operator-=(difference_type diff) {
				base_t::_it -= diff;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Subtraction.
			friend Derived operator-(const Derived &it, difference_type diff) {
				return _make_it(_get_it(it) - diff);
			}
			/// Subtraction.
			friend Derived operator-(difference_type diff, const Derived &it) {
				return _make_it(diff - _get_it(it));
			}
			/// Difference calculation.
			friend difference_type operator-(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) - _get_it(rhs);
			}

			/// Comparison.
			friend bool operator>(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) > _get_it(rhs);
			}
			/// Comparison.
			friend bool operator>=(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) >= _get_it(rhs);
			}
			/// Comparison.
			friend bool operator<(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) < _get_it(rhs);
			}
			/// Comparison.
			friend bool operator<=(const Derived &lhs, const Derived &rhs) {
				return _get_it(lhs) <= _get_it(rhs);
			}
		protected:
			/// Iniitalizes the base struct with the base iterator.
			random_iterator_wrapper(BaseIt it) : base_t(std::move(it)) {
			}
		private:
			/// Returns the base iterator for the given derived object.
			inline static const BaseIt &_get_it(const Derived &d) {
				return d._it;
			}
			/// Creates a derived object given a base iterator.
			inline static Derived _make_it(BaseIt it) {
				return Derived(std::move(it));
			}
		};
	}

	/// Wrapper for RapidJSON.
	namespace rapidjson {
		struct object_t;
		struct array_t;
		struct document_t;

		/// Default encoding used by RapidJSON.
		using encoding = ::rapidjson::UTF8<char>;

		/// RapidJSON type that holds a JSON value.
		using rapidjson_value_t = ::rapidjson::GenericValue<encoding>;
		/// RapidJSON type used to parse a document.
		using rapidjson_document_t = ::rapidjson::GenericDocument<encoding>;

		/// Wrapper around a \ref rapidjson_value_t.
		struct value_t {
			friend object_t;
			friend array_t;
			friend document_t;
		public:
			using object_t = object_t; ///< The object type.
			using array_t = array_t; ///< The array type.

			/// Default constructor.
			value_t() = default;

			/// Determines if \ref _val holds a value of the given type.
			template <typename T> bool is() const {
				if constexpr (std::is_same_v<T, null_t>) {
					return _val->IsNull();
				} else if constexpr (std::is_same_v<T, object_t>) {
					return _val->IsObject();
				} else if constexpr (std::is_same_v<T, array_t>) {
					return _val->IsArray();
				} else if constexpr (std::is_same_v<T, str_t> || std::is_same_v<T, str_view_t>) {
					return _val->IsString();
				} else if constexpr (std::is_same_v<T, bool>) {
					return _val->IsBool();
				} else if constexpr (std::is_floating_point_v<T>) {
					return _val->IsNumber();
				} else if constexpr (std::is_integral_v<T>) {
					if constexpr (std::is_same_v<T, std::uint64_t>) {
						return _val->IsUint64();
					} else if constexpr (std::is_signed_v<T>) {
						return _val->IsInt() || _val->IsInt64();
					} else {
						return _val->IsUint() || _val->IsUint64();
					}
				} else {
					return false;
				}
			}
			/// Returns the value as the specified type.
			template <typename T> T get() const;

			/// Retrieves a \ref str_view_t from a \ref rapidjson_value_t.
			inline static str_view_t get_string_view(const rapidjson_value_t &v) {
				return str_view_t(v.GetString(), v.GetStringLength());
			}
		protected:
			const rapidjson_value_t *_val = nullptr; ///< The value.

			/// Initializes \ref _val directly.
			explicit value_t(const rapidjson_value_t *v) : _val(v) {
			}
		};
		/// Wrapper around a \ref rapidjson_value_t that is knwon to be an object.
		struct object_t {
			friend value_t;
		public:
			/// Iterators.
			struct iterator :
				public _details::bidirectional_iterator_wrapper<iterator, rapidjson_value_t::ConstMemberIterator> {
				friend bidirectional_iterator_wrapper;
				friend object_t;
			public:
				/// Default constructor.
				iterator() = default;

				/// Returns the name of this member.
				str_view_t name() const {
					return value_t::get_string_view(_it->name);
				}
				/// Returns the value of this member.
				value_t value() const {
					return value_t(&_it->value);
				}
			protected:
				/// Initializes the base class with the base iterator.
				explicit iterator(rapidjson_value_t::ConstMemberIterator it) :
					bidirectional_iterator_wrapper(std::move(it)) {
				}
			};

			/// Default constructor.
			object_t() = default;

			/// Finds the member with the given name.
			iterator find_member(str_view_t name) const {
				return iterator(_obj->FindMember(name.data()));
			}
			/// Returns an iterator to the first member.
			iterator member_begin() const {
				return iterator(_obj->MemberBegin());
			}
			/// Returns an iterator past the last member.
			iterator member_end() const {
				return iterator(_obj->MemberEnd());
			}

			/// Returns the number of members this object has.
			size_t size() const {
				return _obj->MemberCount();
			}
		protected:
			const rapidjson_value_t *_obj = nullptr; ///< The value.

			/// Initializes \ref _obj directly.
			explicit object_t(const rapidjson_value_t *v) : _obj(v) {
			}
		};
		/// Wrapper around a \ref rapidjson_value_t that is knwon to be an array.
		struct array_t {
			friend value_t;
		public:
			/// Iterators.
			struct iterator :
				public _details::random_iterator_wrapper<iterator, rapidjson_value_t::ConstValueIterator> {
				friend random_iterator_wrapper;
				friend random_iterator_wrapper::base_t;
				friend array_t;
			public:
				/// Default constructor.
				iterator() = default;

				/// Used to implement the \p -> operator.
				struct proxy {
					friend iterator;
				public:
					/// Returns a pointer to \ref _v.
					value_t *operator->() {
						return &_v;
					}
				protected:
					value_t _v; ///< The underlying \ref value_t.

					/// Initializes \ref _v directly.
					explicit proxy(value_t v) : _v(v) {
					}
				};

				/// Returns the underlying value.
				value_t operator*() const {
					return value_t(&*_it);
				}
				/// Returns a \ref proxy for the underlying value.
				proxy operator->() const {
					return proxy(**this);
				}
			protected:
				/// Initializes this iterator with a base iterator.
				explicit iterator(rapidjson_value_t::ConstValueIterator it) :
					random_iterator_wrapper(std::move(it)) {
				}
			};

			/// Default constructor.
			array_t() = default;

			/// Returns an iterator to the first element.
			iterator begin() const {
				return iterator(_arr->Begin());
			}
			/// Returns an iterator past the last element.
			iterator end() const {
				return iterator(_arr->End());
			}

			/// Indexing.
			value_t operator[](size_t i) const {
				return value_t(&(*_arr)[static_cast<::rapidjson::SizeType>(i)]);
			}
			/// Returns the element at the given index.
			value_t at(size_t i) const {
				return (*this)[i];
			}

			/// Returns the length of this array.
			size_t size() const {
				return _arr->Size();
			}
		protected:
			const rapidjson_value_t *_arr = nullptr; ///< The value.

			/// Initializes \ref _arr directly.
			explicit array_t(const rapidjson_value_t *v) : _arr(v) {
			}
		};

		template <typename T> T value_t::get() const {
			if constexpr (std::is_same_v<T, null_t>) {
				return null_t();
			} else if constexpr (std::is_same_v<T, object_t>) {
				return object_t(_val);
			} else if constexpr (std::is_same_v<T, array_t>) {
				return array_t(_val);
			} else if constexpr (std::is_same_v<T, str_t>) {
				return str_t(get_string_view(*_val));
			} else if constexpr (std::is_same_v<T, str_view_t>) {
				return get_string_view(*_val);
			} else {
				return _val->Get<T>();
			}
		}

		/// Wrapper around a \ref rapidjson_document_t.
		struct document_t {
		public:
			/// Default constructor.
			document_t() = default;
			/// Move constructor.
			document_t(document_t &&src) noexcept : _doc(std::move(src._doc)) {
			}
			/// Move assignment.
			document_t &operator=(document_t &&src) noexcept {
				_doc = std::move(src._doc);
				return *this;
			}

			/// Returns a reference to \ref _doc.
			value_t root() const {
				return value_t(&_doc);
			}

			/// Parses the given document.
			inline static document_t parse(str_view_t data) {
				document_t res;
				res._doc.Parse<
					::rapidjson::kParseCommentsFlag |
					::rapidjson::kParseTrailingCommasFlag
				>(data.data(), data.size());
				return res;
			}
		protected:
			rapidjson_document_t _doc; ///< The document.
		};
	}

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
	template <typename Value> inline value_storage store(const Value & v) {
		if (v.is<null_t>()) {
			return make_value_storage<null_t>();
		}
		if (v.is<bool>()) {
			return make_value_storage<bool>(v.get<bool>());
		}
		if (v.is<std::int64_t>()) {
			return make_value_storage<std::int64_t>(v.get<std::int64_t>());
		}
		if (v.is<std::uint64_t>()) {
			return make_value_storage<std::uint64_t>(v.get<std::uint64_t>());
		}
		if (v.is<double>()) {
			return make_value_storage<double>(v.get<double>());
		}
		if (v.is<str_view_t>()) {
			return make_value_storage<str_t>(v.get<str_view_t>());
		}
		if (v.is<typename Value::object_t>()) {
			value_storage val(std::in_place_type<value_storage::object>);
			auto &dict = std::get<value_storage::object>(val.value);
			auto &&obj = v.get<typename Value::object_t>();
			for (auto it = obj.member_begin(); it != obj.member_end(); ++it) {
				dict.emplace(it.name(), store(it.value()));
			}
			return val;
		}
		if (v.is<typename Value::array_t>()) {
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
		struct value_t {
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
		struct object_t {
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

	/// Parses a JSON file.
	template <typename Engine> inline Engine parse_file(const std::filesystem::path & path) {
		std::ifstream fin(path, std::ios::binary | std::ios::ate);
		std::ifstream::pos_type sz = fin.tellg();
		fin.seekg(0, std::ios::beg);
		char *mem = static_cast<char*>(std::malloc(sz));
		fin.read(mem, sz);
		assert_true_sys(fin.good(), "cannot load JSON file");
		Engine doc = Engine::parse(str_view_t(mem, sz));
		std::free(mem);
		return doc;
	}

	/// Checks if the given node is of type \p T, then returns its value if the type's correct.
	template <typename T, typename Value> inline bool try_cast(const Value & val, T & ret) {
		if (val.is<T>()) {
			ret = val.get<T>();
			return true;
		}
		return false;
	}
	/// \ref try_cast() with a default value.
	template <typename T, typename Value> inline T cast_or_default(const Value & val, const T & def) {
		T v;
		if (try_cast(val, v)) {
			return v;
		}
		return def;
	}

	/// Checks if the object has a member with a certain name which is of type \p T, then returns the value of the
	/// member if there is.
	template <typename T, typename Object> bool try_cast_member(const Object & val, str_view_t name, T & ret) {
		auto found = val.find_member(name);
		if (found != val.member_end()) {
			return try_cast(found.value(), ret);
		}
		return false;
	}
	/// \ref try_cast_member() with a default value.
	template <typename T, typename Object> inline T cast_member_or_default(
		const Object & v, const str_t & s, const T & def
	) {
		T res;
		if (try_cast_member(v, s, res)) {
			return res;
		}
		return def;
	}

	/// Contains structs used to parse objects of various types from JSON values.
	namespace object_parsers {
		/// The basic parser type that simply uses \ref try_cast() to parse the object.
		template <typename T> struct parser {
			/// The basic parsing function. Simply calls \ref try_cast().
			template <typename Value> bool operator()(const Value &v, T &res) const {
				return try_cast(v, res);
			}
		};
		/// Shorthand for \ref parser::operator()().
		template <typename T, typename Value, typename Parser = parser<T>> bool try_parse(
			const Value & val, T & res, const Parser & p = Parser{}
		) {
			return p(val, res);
		}
		/// \ref try_parse() with a default value.
		template <typename T, typename Value, typename Parser = parser<T>> T parse_or_default(
			const Value & val, const T &def, const Parser & p = Parser{}
		) {
			T res;
			if (try_parse(val, res, p)) {
				return res;
			}
			return def;
		}

		/// Indicates the result of a call to \ref try_parse_member().
		enum class parse_member_result : unsigned char {
			success, ///< The member is found and successfully parsed. This is the only success state.
			parsing_failed, ///< The member is found but could not be parsed.
			not_found ///< A member with the given name is not found.
		};
		/// Tries to find a member in a JSON object and parse it using the specified parser.
		template <typename T, typename Object, typename Parser = parser<T>> parse_member_result try_parse_member(
			const Object & val, str_view_t name, T & res, const Parser & p = Parser{}
		) {
			if (auto fmem = val.find_member(name); fmem != val.member_end()) {
				if (p(fmem.value(), res)) {
					return parse_member_result::success;
				}
				return parse_member_result::parsing_failed;
			}
			return parse_member_result::not_found;
		}
		/// \ref try_parse_member() with a default value if parsing failed.
		template <typename T, typename Object, typename Parser = parser<T>> T parse_member_or_default(
			const Object & val, str_view_t name, const T &def, const Parser & p = Parser{}
		) {
			T res;
			if (try_parse_member(val, name, res, p) == parse_member_result::success) {
				return res;
			}
			return def;
		}
	}

	/// The default JSON engine.
	namespace default_engine {
		using namespace codepad::json::rapidjson;
	}
}
