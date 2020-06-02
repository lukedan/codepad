// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Provides context information when parsing complex JSON data. This is the preferred way of parsing JSON in
/// Codepad.

#include <memory>
#include <variant>

#include "misc.h"

namespace codepad::json {
	/// Utility classes for advanced parsing with detailed logging information.
	namespace parsing {
		template <typename> struct value_t;
		template <typename> struct object_t;
		template <typename> struct array_t;

		template <typename ValueType> value_t<ValueType> make_value(ValueType);

		namespace _details {
			/// A node in a JSON hierarchy.
			template <typename ValueType> struct context_node {
				friend object_t<ValueType>;
				friend array_t<ValueType>;
			public:
				using value_type = ValueType; ///< The type of JSON values.
				using array_type = typename value_type::array_type; ///< The type of JSON arrays.
				using object_type = typename value_type::object_type; ///< The type of JSON objects.

				using identifier_t = std::variant<std::size_t, std::u8string>; ///< Used to identify this object in the parent.

				/// Constructs a new context node using the given string as its identifier.
				context_node(identifier_t id, std::shared_ptr<context_node> parent) :
					_id(std::move(id)), _parent(std::move(parent)) {
				}

				/// Returns the identifier of this node.
				const identifier_t &get_id() const {
					return _id;
				}
				/// Returns the parent node.
				const context_node &get_parent() const {
					return *_parent;
				}

				/// Prints the path to the given stream.
				template <typename Out> void print_path(Out &out) {
					std::vector<context_node*> path;
					for (context_node *cur = this; cur; cur = cur->_parent.get()) {
						path.emplace_back(cur);
					}
					for (auto it = path.rbegin(); it != path.rend(); ++it) {
						if (it != path.rbegin()) {
							out << u8".";
						}
						std::visit([&out](auto &&value) {
							out << value;
							}, (*it)->_id);
					}
				}
			protected:
				identifier_t _id; ///< Identifies this node in the parent node.
				std::shared_ptr<context_node> _parent; ///< The parent node.
			};

			/// Creates a child node from the given identifier.
			template <typename ValueType> std::shared_ptr<context_node<ValueType>> spawn_child(
				std::shared_ptr<context_node<ValueType>> parent, std::u8string id
			) {
				return std::make_shared<context_node<ValueType>>(
					typename context_node<ValueType>::identifier_t(std::in_place_type<std::u8string>, std::move(id)),
					std::move(parent)
					);
			}
			/// Creates a child node from the given identifier.
			template <typename ValueType> std::shared_ptr<context_node<ValueType>> spawn_child(
				std::shared_ptr<context_node<ValueType>> parent, std::size_t id
			) {
				return std::make_shared<context_node<ValueType>>(
					typename context_node<ValueType>::identifier_t(std::in_place_type<std::size_t>, id),
					std::move(parent)
					);
			}
		}

		/// Wrapper class that provides additional functionalities for parsing.
		template <typename ValueType> struct value_t : public json::_details::value_type_base<value_t<ValueType>> {
			friend object_t<ValueType>;
			friend array_t<ValueType>;
			friend value_t make_value<ValueType>(ValueType);
		public:
			using object_type = object_t<ValueType>; ///< The type of JSON objects.
			using array_type = array_t<ValueType>; ///< The type of JSON arrays.

			/// Default constructor.
			value_t() = default;

			/// Forwards the call to the underlying value.
			template <typename T> bool is() const;
			/// Casts this value into the specified type.
			template <typename T> T get() const;

			/// Creates a new \ref logger::log_entry and logs the path to the current node. This function hides the
			/// implementation in \ref json::_details::value_type_base to provide additional logging.
			template <log_level Level> logger::log_entry log(code_position pos) const {
				logger::log_entry entry = logger::get().log<Level>(std::move(pos));
				entry << u8"at ";
				_node->print_path(entry);
				entry << u8":\n";
				return entry;
			}
		protected:
			using _context_node_t = _details::context_node<ValueType>; ///< Nodes that provide context information.

			/// Initializes all fields of this struct.
			value_t(ValueType v, std::shared_ptr<_context_node_t> node) :
				_value(std::move(v)), _node(std::move(node)) {
			}

			ValueType _value; ///< The value.
			std::shared_ptr<_context_node_t> _node; ///< The associated \ref _context_node_t.
		};
		/// Wrapper class around a JSON object that provides additional functionalities for parsing.
		template <typename ValueType> struct object_t :
			public json::_details::object_type_base<object_t<ValueType>> {
			friend value_t<ValueType>;
		public:
			/// Iterator through the fields of an object. This struct is a wrapper around the underlying iterator
			/// type.
			struct iterator {
				friend object_t;
			public:
				using base_iterator_t = typename ValueType::object_type::iterator; ///< The underlying iterator type.
				/// Difference type.
				using difference_type = typename std::iterator_traits<base_iterator_t>::difference_type;
				/// Iterator category.
				using iterator_category = typename std::iterator_traits<base_iterator_t>::iterator_category;

				/// Default constructor.
				iterator() = default;

				/// Prefix increment.
				iterator &operator++() {
					++_it;
					return *this;
				}
				/// Postfix increment.
				iterator operator++(int) {
					return iterator(_it++, *_obj);
				}
				/// Prefix decrement.
				iterator &operator--() {
					--_it;
					return *this;
				}
				/// Postfix decrement;
				iterator operator--(int) {
					return iterator(_it--, *_obj);
				}

				/// Returns the name of this field.
				std::u8string_view name() const {
					return _it.name();
				}
				/// Returns the value of this field.
				value_t<ValueType> value() const {
					return value_t<ValueType>(_it.value(), _details::spawn_child(_obj->_node, std::u8string(_it.name())));
				}

				/// Equality.
				friend bool operator==(const iterator &lhs, const iterator &rhs) {
					return lhs._it == rhs._it;
				}
				/// Inequality.
				friend bool operator!=(const iterator &lhs, const iterator &rhs) {
					return lhs._it != rhs._it;
				}
			protected:
				/// Initializes all fields of this struct.
				iterator(base_iterator_t it, const object_t &obj) :
					_it(std::move(it)), _obj(&obj) {
				}

				base_iterator_t _it; ///< The wrapped iterator.
				const object_t *_obj = nullptr; ///< The \ref object_t that created this iterator.
			};

			/// Default constructor.
			object_t() = default;

			/// Finds the member with the given name.
			iterator find_member(std::u8string_view name) const {
				return iterator(_object.find_member(name), *this);
			}
			/// Returns an iterator to the first member.
			iterator member_begin() const {
				return iterator(_object.member_begin(), *this);
			}
			/// Returns an iterator past the last member.
			iterator member_end() const {
				return iterator(_object.member_end(), *this);
			}

			/// Creates a new \ref logger::log_entry and logs the path to the current node. This function hides the
			/// implementation in \ref json::_details::value_type_base to provide additional logging.
			template <log_level Level> logger::log_entry log(code_position pos) const {
				logger::log_entry entry = logger::get().log<Level>(std::move(pos));
				entry << u8"at ";
				_node->print_path(entry);
				entry << u8":\n";
				return entry;
			}

			/// Returns the number of entries in this object.
			std::size_t size() const {
				return _object.size();
			}
		protected:
			using _value_object_t = typename ValueType::object_type; ///< The underlying object type.
			using _context_node_t = _details::context_node<ValueType>; ///< Nodes that provide context information.

			/// Initializes all fields of this struct.
			object_t(_value_object_t obj, std::shared_ptr<_context_node_t> node) :
				_object(std::move(obj)), _node(std::move(node)) {
			}

			_value_object_t _object; ///< The value.
			std::shared_ptr<_context_node_t> _node; ///< The associated \ref _context_node_t.
		};
		/// Wrapper class around a JSON array that provides additional functionalities for parsing.
		template <typename ValueType> struct array_t {
			friend value_t<ValueType>;
		public:
			/// Iterator through the elements of this array.
			struct iterator {
				friend array_t;
			public:
				using base_iterator_t = typename ValueType::array_type::iterator; ///< The underlying iterator type.
				/// Difference type.
				using difference_type = typename std::iterator_traits<base_iterator_t>::difference_type;
				/// Iterator category.
				using iterator_category = typename std::iterator_traits<base_iterator_t>::iterator_category;

				/// Used to implement the \p -> operator.
				struct proxy {
					friend iterator;
				public:
					/// Returns a pointer to \ref _v.
					value_t<ValueType> *operator->() {
						return &_v;
					}
				protected:
					value_t<ValueType> _v; ///< The underlying \ref value_t.

					/// Initializes \ref _v directly.
					explicit proxy(value_t<ValueType> v) : _v(v) {
					}
				};

				/// Default constructor.
				iterator() = default;

				/// Returns the underlying value.
				value_t<ValueType> operator*() const {
					return value_t<ValueType>(*_it, _details::spawn_child(_arr->_node, _pos));
				}
				/// Returns a \ref proxy for the underlying value.
				proxy operator->() const {
					return proxy(**this);
				}

				/// Prefix increment.
				iterator &operator++() {
					++_pos;
					++_it;
					return *this;
				}
				/// Postfix increment.
				iterator operator++(int) {
					return iterator(_it++, _pos++, *_arr);
				}
				/// Prefix decrement.
				iterator &operator--() {
					--_pos;
					--_it;
					return *this;
				}
				/// Postfix decrement;
				iterator operator--(int) {
					--_pos;
					return iterator(_it--, _pos++, *_arr);
				}

				/// In-place addition.
				iterator &operator+=(difference_type diff) {
					_it += diff;
					_pos = static_cast<std::size_t>(static_cast<difference_type>(_pos) + diff);
					return *this;
				}
				/// Addition.
				friend iterator operator+(iterator it, difference_type diff) {
					return it += diff;
				}
				/// Addition.
				friend iterator operator+(difference_type diff, iterator it) {
					return it += diff;
				}

				/// In-place subtraction.
				iterator &operator-=(difference_type diff) {
					_it -= diff;
					_pos = static_cast<std::size_t>(static_cast<difference_type>(_pos) - diff);
					return *this;
				}
				/// Subtraction.
				friend iterator operator-(iterator it, difference_type diff) {
					return it -= diff;
				}
				/// Difference calculation.
				friend difference_type operator-(const iterator &lhs, const iterator &rhs) {
					return lhs._it - rhs._it;
				}

				/// Equality.
				friend bool operator==(const iterator &lhs, const iterator &rhs) {
					return lhs._it == rhs._it;
				}
				/// Inequality.
				friend bool operator!=(const iterator &lhs, const iterator &rhs) {
					return lhs._it != rhs._it;
				}

				/// Comparison.
				friend bool operator>(const iterator &lhs, const iterator &rhs) {
					return lhs._it > rhs._it;
				}
				/// Comparison.
				friend bool operator>=(const iterator &lhs, const iterator &rhs) {
					return lhs._it >= rhs._it;
				}
				/// Comparison.
				friend bool operator<(const iterator &lhs, const iterator &rhs) {
					return lhs._it < rhs._it;
				}
				/// Comparison.
				friend bool operator<=(const iterator &lhs, const iterator &rhs) {
					return lhs._it <= rhs._it;
				}
			protected:
				using _context_node_t = _details::context_node<ValueType>; ///< Nodes that provide context information.

				/// Initializes all fields of this struct.
				iterator(base_iterator_t it, std::size_t pos, const array_t &arr) :
					_it(std::move(it)), _pos(pos), _arr(&arr) {
				}

				base_iterator_t _it; ///< The underlying iterator.
				std::size_t _pos = 0; ///< The position of this iterator.
				const array_t *_arr = nullptr; ///< The \ref array_t that created this iterator.
			};

			/// Default constructor.
			array_t() = default;

			/// Returns an iterator to the first object.
			iterator begin() const {
				return iterator(_array.begin(), 0, *this);
			}
			/// Returns an iterator past the last object.
			iterator end() const {
				return iterator(_array.end(), size(), *this);
			}

			/// Returns the element at the given index.
			value_t<ValueType> operator[](std::size_t i) const {
				return value_t(_array[i], _details::spawn_child(_node, i));
			}
			/// Returns the element at the given index.
			value_t<ValueType> at(std::size_t i) const {
				return (*this)[i];
			}

			/// Returns the number of entries in this array.
			std::size_t size() const {
				return _array.size();
			}
		protected:
			using _context_node_t = _details::context_node<ValueType>; ///< Nodes that provide context information.

			/// Initializes all fields of this struct.
			array_t(typename ValueType::array_type arr, std::shared_ptr<_context_node_t> node) :
				_array(std::move(arr)), _node(std::move(node)) {
			}

			typename ValueType::array_type _array; ///< The value.
			std::shared_ptr<_context_node_t> _node; ///< The associated \ref _context_node_t.
		};

		template <typename ValueType> template <typename T> bool value_t<ValueType>::is() const {
			if constexpr (std::is_same_v<T, object_type>) {
				return _value.template is<typename ValueType::object_type>();
			} else if constexpr (std::is_same_v<T, array_type>) {
				return _value.template is<typename ValueType::array_type>();
			} else {
				return _value.template is<T>();
			}
		}

		template <typename ValueType> template <typename T> T value_t<ValueType>::get() const {
			if constexpr (std::is_same_v<T, object_type>) {
				return object_type(_value.template get<typename ValueType::object_type>(), _node);
			} else if constexpr (std::is_same_v<T, array_type>) {
				return array_type(_value.template get<typename ValueType::array_type>(), _node);
			} else {
				return _value.template get<T>();
			}
		}

		/// Creates a new \ref value_t whose context node is the root.
		template <typename ValueType> inline value_t<ValueType> make_value(ValueType v) {
			return value_t<ValueType>(std::move(v), _details::spawn_child<ValueType>(nullptr, u8"<root>"));
		}
	}
}
