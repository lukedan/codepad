// Copyright(c) the Codepad contributors.All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Wrappers and misceallenous functions for JSON libraries.

#include <fstream>
#include <filesystem>

#include "../encodings.h"
#include "../assert.h"
#include "../profiling.h"

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
	//   array_t, object_t
	//   is<T>, get<T>
	//     T: null_t, bool, number_t, unsigned, signed, double, array_t, object_t, u8string, u8string_view
	//   log(), try_cast(), cast(), try_parse(), parse() (provided by value_type_base)
	//
	// object_t:
	//   an reference of the immutable underlying value, unless for storage implementation
	//
	//   iterator
	//     name(): u8string_view, value(): value_t
	//   find_member(), member_begin(), member_end()
	//   size()
	//   log(), try_parse_member(), parse_member(), parse_optional_member() (provided by object_type_base)
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

	/// The basic parser type that simply uses \ref try_cast() to parse the object. It is recommended to specialize
	/// this type to provide concrete parsing functionalities for user-defined types.
	template <typename T> struct default_parser {
		/// The basic parsing function that simply calls \p cast().
		template <typename Value> std::optional<T> operator()(const Value &v) const {
			return v.template cast<T>();
		}
	};
	/// The parser that parses an array of objects.
	template <typename T, typename Parser = default_parser<T>> struct array_parser {
		/// Default constructor.
		array_parser() = default;
		/// Initializes \ref parser.
		explicit array_parser(Parser p) : parser(std::move(p)) {
		}

		/// Uses the parser to parse all elements of the array.
		template <typename Value> std::optional<std::vector<T>> operator()(const Value &val) const {
			if (auto arr = val.template cast<typename Value::array_t>()) {
				std::vector<T> res;
				for (auto &&elem : arr.value()) {
					if (auto elem_val = elem.template parse<T>(parser)) {
						res.emplace_back(std::move(elem_val.value()));
					}
				}
				return res;
			}
			return std::nullopt;
		}

		Parser parser; ///< The parser for individual objects.
	};

	/// Indicates the result of converting a member of a JSON object.
	enum class convert_member_result : unsigned char {
		success, ///< The member is found and successfully converted. This is the only success state.
		failed, ///< The member is found but could not be converted.
		not_found ///< A member with the given name is not found.
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

			/// Prefix increment.
			Derived &operator++() {
				++_it;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Postfix increment.
			Derived operator++(int) {
				return Derived(_it++);
			}
			/// Prefix decrement.
			Derived &operator--() {
				--_it;
				return *reinterpret_cast<Derived*>(this);
			}
			/// Postfix decrement.
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
			using difference_type = typename bidirectional_iterator_wrapper<Derived, BaseIt>::difference_type;

			/// Default constructor.
			random_iterator_wrapper() = default;

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

		/// Base class of \p value_t implementations that provides some common features.
		template <typename Derived> struct value_type_base {
			/// Default virtual destructor.
			virtual ~value_type_base() = default;

			/// Returns an entry for logging.
			template <log_level Level> logger::log_entry log(code_position pos) const {
				return logger::get().log<Level>(std::move(pos));
			}

			/// Attempts to cast this value into a more specific type.
			template <typename T> std::optional<T> try_cast() const {
				if (_this()->template is<T>()) {
					return _this()->template get<T>();
				}
				return std::nullopt;
			}
			/// Attempts to cast this value into a more specific type. Logs an error message if the cast fails.
			template <typename T> std::optional<T> cast() const {
				if (_this()->template is<T>()) {
					return _this()->template get<T>();
				}
				_this()->template log<log_level::error>(CP_HERE) <<
					u8"cast to " << demangle(typeid(T).name()) << u8" failed";
				return std::nullopt;
			}
			/// Attempts to parse this value.
			template <typename T, typename Parser = default_parser<T>> std::optional<T> try_parse(
				const Parser &parser = Parser{}
			) const {
				return parser(*_this());
			}
			/// Attempts to parse this value. Logs an error message if parsing fails.
			template <typename T, typename Parser = default_parser<T>> std::optional<T> parse(
				const Parser &parser = Parser{}
			) const {
				auto res = parser(*_this());
				if (!res) {
					_this()->template log<log_level::error>(CP_HERE) <<
						u8"parsing of " << demangle(typeid(T).name()) << u8" failed";
				}
				return res;
			}
		private:
			/// Casts \p this to \p Derived.
			const Derived *_this() const {
				return reinterpret_cast<const Derived*>(this);
			}
		};
		/// Base class of \p object_t implementations that provides some common features.
		template <typename Derived> struct object_type_base {
			/// Default virtual destructor.
			virtual ~object_type_base() = default;

			/// Returns an entry for logging.
			template <log_level Level> logger::log_entry log(code_position pos) const {
				return logger::get().log<Level>(std::move(pos));
			}

			/// Attempts to parse the given member.
			template <typename T, typename Parser = default_parser<T>> convert_member_result try_parse_member(
				str_view_t member, T &val, const Parser &parser = Parser{}
			) const {
				auto it = this->find_member(member);
				if (it == this->member_end()) {
					return convert_member_result::not_found;
				}
				if (auto parsed = parser(it.value())) {
					val = std::move(parsed.value());
					return convert_member_result::success;
				}
				return convert_member_result::failed;
			}
			/// Attempts to parse the given member. Logs an error message if the member is not found or if parsing
			/// fails.
			template <typename T, typename Parser = default_parser<T>> std::optional<T> parse_member(
				str_view_t member, const Parser &parser = Parser{}
			) const {
				return _parse_member_impl<T, false, Parser>(member, parser);
			}
			/// Attempts to parse the given member. Logs an error message if parsing fails.
			template <typename T, typename Parser = default_parser<T>> std::optional<T> parse_optional_member(
				str_view_t member, const Parser &parser = Parser{}
			) const {
				return _parse_member_impl<T, true, Parser>(member, parser);
			}
		private:
			/// Used to implement \ref parse_member() and \ref parse_optional_member().
			template <typename T, bool Optional, typename Parser> std::optional<T> _parse_member_impl(
				str_view_t member, const Parser &parser = Parser{}
			) const {
				if (auto it = _this()->find_member(member); it != _this()->member_end()) {
					auto res = parser(it.value());
					if (!res) {
						_this()->template log<log_level::error>(CP_HERE) <<
							u8"failed to parse member into " << demangle(typeid(T).name()) << u8": " << member;
					}
					return res;
				}
				if constexpr (!Optional) {
					_this()->template log<log_level::error>(CP_HERE) << u8"member not found: " << member;
				}
				return std::nullopt;
			}
			/// Casts \p this to \p Derived.
			const Derived *_this() const {
				return reinterpret_cast<const Derived*>(this);
			}
		};
	}

	/// Parses a JSON file.
	template <typename Engine> inline Engine parse_file(const std::filesystem::path &path) {
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
}
