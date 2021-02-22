// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Wrapper for the RapidJSON library.

#include <rapidjson/document.h>

#include "../encodings.h"
#include "misc.h"

namespace codepad::json::rapidjson {
	struct object_t;
	struct array_t;
	struct document_t;

	/// Retrieves a \ref std::u8string_view from a \p rapidjson::GenericValue.
	template <typename Byte, typename Allocator> [[nodiscard]] inline std::u8string_view get_string_view(
		const ::rapidjson::GenericValue<::rapidjson::UTF8<Byte>, Allocator> &v
	) {
		static_assert(sizeof(Byte) == sizeof(char), "invalid word type for utf-8");
		return std::u8string_view(reinterpret_cast<const char8_t*>(v.GetString()), v.GetStringLength());
	}

	/// Default encoding used by RapidJSON.
	using encoding = ::rapidjson::UTF8<char>;

	/// RapidJSON type that holds a JSON value.
	using rapidjson_value_t = ::rapidjson::GenericValue<encoding>;
	/// RapidJSON type used to parse a document.
	using rapidjson_document_t = ::rapidjson::GenericDocument<encoding>;

	/// Wrapper around a \ref rapidjson_value_t.
	struct value_t : public _details::value_type_base<value_t> {
		friend object_t;
		friend array_t;
		friend document_t;
	public:
		using object_type = object_t; ///< The object type.
		using array_type = array_t; ///< The array type.

		/// Default constructor.
		value_t() = default;
		/// Initializes \ref _val directly.
		explicit value_t(const rapidjson_value_t *v) : _val(v) {
		}

		/// Determines if \ref _val holds a value of the given type.
		template <typename T> bool is() const {
			if constexpr (std::is_same_v<T, null_t>) {
				return _val->IsNull();
			} else if constexpr (std::is_same_v<T, object_type>) {
				return _val->IsObject();
			} else if constexpr (std::is_same_v<T, array_type>) {
				return _val->IsArray();
			} else if constexpr (std::is_same_v<T, std::u8string> || std::is_same_v<T, std::u8string_view>) {
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

		/// Returns whether \ref _val is empty.
		bool empty() const {
			return _val == nullptr;
		}
	protected:
		const rapidjson_value_t *_val = nullptr; ///< The value.
	};
	/// Wrapper around a \ref rapidjson_value_t that is knwon to be an object.
	struct object_t : public _details::object_type_base<object_t> {
		friend value_t;
	public:
		/// Iterators.
		struct iterator :
			public _details::bidirectional_iterator_wrapper<iterator, rapidjson_value_t::ConstMemberIterator> {
			friend bidirectional_iterator_wrapper;
			friend object_t;
		public:
			using value_type = void; ///< Invalid value type.
			using pointer = void; ///< Invalid pointer type.
			using reference = void; ///< Invalid reference type.

			/// Default constructor.
			iterator() = default;

			/// Returns the name of this member.
			std::u8string_view name() const {
				return get_string_view(_it->name);
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
		iterator find_member(std::u8string_view name) const {
			return iterator(_obj->FindMember(reinterpret_cast<const char*>(name.data())));
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
		std::size_t size() const {
			return _obj->MemberCount();
		}

		/// Returns whether \ref _obj is empty.
		bool empty() const {
			return _obj == nullptr;
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

			using value_type = value_t; ///< Iterator value type.
			using pointer = const value_t*; ///< Iterator pointer type.
			using reference = proxy; ///< Iterator reference type.

			/// Default constructor.
			iterator() = default;

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
		value_t operator[](std::size_t i) const {
			return value_t(&(*_arr)[static_cast<::rapidjson::SizeType>(i)]);
		}
		/// Returns the element at the given index.
		value_t at(std::size_t i) const {
			return (*this)[i];
		}

		/// Returns the length of this array.
		std::size_t size() const {
			return _arr->Size();
		}

		/// Returns whether \ref _arr is empty.
		bool empty() const {
			return _arr == nullptr;
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
		} else if constexpr (std::is_same_v<T, object_type>) {
			return object_type(_val);
		} else if constexpr (std::is_same_v<T, array_type>) {
			return array_type(_val);
		} else if constexpr (std::is_same_v<T, std::u8string>) {
			return std::u8string(get_string_view(*_val));
		} else if constexpr (std::is_same_v<T, std::u8string_view>) {
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
		/// Default move constructor.
		document_t(document_t&&) noexcept = default;
		/// Default move assignment.
		document_t &operator=(document_t&&) = default;

		/// Returns a reference to \ref _doc.
		value_t root() const {
			return value_t(&_doc);
		}

		/// Parses the given document.
		inline static document_t parse(std::u8string_view data) {
			document_t res;
			res._doc.Parse<
				::rapidjson::kParseCommentsFlag |
				::rapidjson::kParseTrailingCommasFlag
			>(reinterpret_cast<const char*>(data.data()), data.size());
			return res;
		}
	protected:
		rapidjson_document_t _doc; ///< The document.
	};
}
