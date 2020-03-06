// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs used to control the animations of elements.

#include <tuple>
#include <memory>

#include "../core/misc.h"
#include "animation.h"

namespace codepad::ui {
	class element;

	/// Used to parse a string into \ref builder::member_information.
	namespace animation_path {
		/// A component in an animation path.
		struct component {
			/// Default constructor.
			component() = default;
			/// Initializes \ref property.
			explicit component(std::u8string prop) : property(std::move(prop)) {
			}
			/// Initializes all fields of this struct.
			component(std::u8string t, std::u8string prop) : type(std::move(t)), property(std::move(prop)) {
			}

			std::u8string
				type, ///< The expected type of the current object. Can be empty.
				property; ///< The target property.
			std::optional<std::size_t> index; ///< The index, if this component is a list.

			/// Returns \p true if \ref type is the same as the input or if \ref type is empty.
			[[nodiscard]] bool is_type_or_empty(std::u8string_view ty) const {
				return type.empty() || type == ty;
			}
			/// Returns \p true if \ref property matches the given property name, \ref type is empty or matches the
			/// given type name, and \ref index is empty.
			[[nodiscard]] bool is_similar(std::u8string_view ty, std::u8string_view prop) const {
				return is_type_or_empty(ty) && prop == property && !index.has_value();
			}
		};
		using component_list = std::vector<component>; ///< A list of components.

		/// The parser.
		class parser {
		public:
			// type = name
			// property = name
			// index = '[' number ']'
			//
			// typed_property = type '.' property
			// typed_component = '(' typed_property ')' | '(' typed_property index ')' |
			//                   '(' typed_property ')' index
			// untyped_component = property | property index
			// component = typed_component | untyped_component
			//
			// path = component | path '.' component
			/// The result of parsing a part of the path.
			enum class result {
				completed, ///< Success.
				not_found, ///< The path does not match the format at all.
				error ///< The path matches partially but is not complete.
			};

			/// Splits an animation target path into components. See the comment in <tt>namespace parsing</tt>.
			inline static result parse(std::u8string_view path, component_list &list) {
				if (path.empty()) {
					return result::not_found;
				}
				auto it = path.begin();
				list.emplace_back();
				result res = _parse_component(it, path.end(), list.back());
				if (res != result::completed) {
					return result::error;
				}
				while (it != path.end()) {
					if (*(it++) != '.') {
						return result::error; // technically completed, but it's a single path
					}
					list.emplace_back();
					res = _parse_component(it, path.end(), list.back());
					if (res != result::completed) {
						return result::error;
					}
				}
				return result::completed;
			}
		protected:
			/// Parses a string that contains only a to z, 0 to 9, or underscores.
			inline static result _parse_string(std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end) {
				std::size_t nchars = 0;
				while (it != end) {
					if (*it != '_' && !(*it >= 'a' && *it <= 'z') && !(*it >= 'A' && *it <= 'Z')) {
						break;
					}
					++nchars;
					++it;
				}
				return nchars > 0 ? result::completed : result::not_found;
			}
			/// Parses an index.
			inline static result _parse_index(
				std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, std::size_t &v
			) {
				if (it == end || *it != '[') {
					return result::not_found;
				}
				++it;
				if (it == end || !(*it >= '0' && *it <= '9')) {
					return result::error;
				}
				v = *(it++) - '0';
				while (it != end) {
					if (*it == ']') {
						++it;
						return result::completed;
					}
					if (!(*it >= '0' && *it <= '9')) {
						return result::error;
					}
					v = v * 10 + (*(it++) - '0');
				}
				return result::error; // no closing bracket
			}

			/// Parses a typed component.
			inline static result parse_typed_component(
				std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
			) {
				if (it == end || *it != '(') {
					return result::not_found;
				}
				// type
				auto beg = ++it;
				if (_parse_string(it, end) != result::completed) {
					return result::error;
				}
				v.type = std::u8string(&*beg, it - beg);
				// dot
				if (it == end || *it != '.') {
					return result::error;
				}
				// property
				beg = ++it;
				if (_parse_string(it, end) != result::completed) {
					return result::error;
				}
				v.property = std::u8string(&*beg, it - beg);
				// index & closing parenthesis
				bool closed = false;
				if (it != end && *it == ')') {
					++it;
					closed = true;
				}
				std::size_t id = 0;
				result res = _parse_index(it, end, id);
				if (res == result::error) {
					return result::error;
				}
				if (res == result::completed) {
					v.index.emplace(id);
				}
				if (it != end && *it == ')') {
					if (closed) {
						return result::error;
					}
					++it;
					closed = true;
				}
				return closed ? result::completed : result::error;
			}

			/// Parses a typed component.
			inline static result _parse_untyped_component(
				std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
			) {
				auto beg = it;
				if (_parse_string(it, end) != result::completed) {
					return result::not_found;
				}
				v.property = std::u8string_view(&*beg, it - beg);
				std::size_t id;
				result res = _parse_index(it, end, id);
				if (res == result::error) {
					return result::error;
				}
				if (res == result::completed) {
					v.index.emplace(id);
				}
				return result::completed;
			}

			/// Parses a component.
			inline static result _parse_component(
				std::u8string_view::const_iterator &it, std::u8string_view::const_iterator end, component &v
			) {
				result res = parse_typed_component(it, end, v);
				if (res != result::not_found) {
					return res;
				}
				return _parse_untyped_component(it, end, v);
			}
		};

		/// Builds a \ref typed_animation_subject from a \ref component_list.
		namespace builder {
			template <typename, typename> class typed_member_access;

			/// Indicates the effects an element's property has on its visuals and layout.
			enum class element_property_type {
				visual_only, ///< The property only affects the element's visuals.
				affects_layout ///< The property affects the element's layout.
			};

			/// Used to access members given an object and to create \ref animation_subject_base instances.
			template <typename Source> class member_access {
			public:
				/// Default virtual destructor.
				virtual ~member_access() = default;

				/// Returns a pointer to the member given the object.
				virtual void *get(Source&) const = 0;

				/// Creates an \ref animation_subject_base given the source object. Returns \p nullptr if this object
				/// does not support this operation. The created subject may reference this \ref member_access via a
				/// reference, so this object must outlive the subject.
				virtual std::unique_ptr<animation_subject_base> create_for_source(Source&) const {
					return nullptr;
				}
				/// Creates an \ref animation_subject_base given an \ref element and the \ref typed_member_access
				/// used to retrieve the actual source that's a member of the \ref element. The given callback will
				/// be called whenever a new value has been set. Returns \p nullptr if this object does not support
				/// this operation. The created subject may reference this \ref member_access or the intermediate
				/// \ref typed_member_access via a reference, so these objects must outlive the subject.
				virtual std::unique_ptr<animation_subject_base> create_for_element_with_callback(
					element&, typed_member_access<element, Source>&, std::function<void(element&)>
				) const {
					return nullptr;
				}

				/// Used to determine if two \ref member_access objects are equivalent.
				[[nodiscard]] virtual bool equals(const member_access<Source>&) const = 0;
			};

			/// \ref member_access with type information of the member.
			template <typename Source, typename Member> class typed_member_access : public member_access<Source> {
			public:
				/// Returns the result of \ref get_typed().
				void *get(Source &v) const override {
					return get_typed(v);
				}

				/// Returns a poionter to the typed member given the object.
				virtual Member *get_typed(Source&) const = 0;
			};


			/// An \ref typed_animation_subject that grants access to a value throught a reference to an object and a
			/// \ref typed_member_access. The \ref typed_member_access must outlive this object.
			template <typename Input, typename Output> class member_access_subject :
				public typed_animation_subject<Output> {
			public:
				/// Initializes all fields of this class.
				member_access_subject(const typed_member_access<Input, Output> &m, Input &obj) : _member(m), _object(obj) {
				}

				/// Retrieves the value through \ref _member.
				const Output &get() const override {
					return *_member.get_typed(_object);
				}
				/// Sets the value through \ref _member.
				void set(Output v) override {
					*_member.get_typed(_object) = std::move(v);
				}

				/// Tests the equality between two subjects.
				[[nodiscard]] bool equals(const animation_subject_base &subject) const override {
					if (auto *ptr = dynamic_cast<const member_access_subject<Input, Output>*>(&subject)) {
						return &_object == &ptr->_object && _member.equals(ptr->_member);
					}
					return false;
				}
			protected:
				/// Reference to the \ref typed_member_access object.
				const typed_member_access<Input, Output> &_member;
				Input &_object; ///< The object that the subject belong to.
			};

			/// A \ref member_access_subject whose input is an \ref element. This struct calls
			/// \ref scheduler::invalidate_layout() and \ref scheduler::invalidate_visual() appropriately.
			template <typename Output, element_property_type Type> class element_member_access_subject :
				public member_access_subject<element, Output> {
			public:
				element_member_access_subject(const typed_member_access<element, Output> &m, element &obj) :
					member_access_subject<element, Output>(m, obj) {
				}

				/// Calls \ref member_access_subject::set(), then calls \ref scheduler::invalidate_layout() and/or
				/// \ref scheduler::invalidate_visual().
				void set(Output) override;

				/// Tests the equality between two subjects.
				[[nodiscard]] bool equals(const animation_subject_base &subject) const override {
					if (auto *ptr = dynamic_cast<const element_member_access_subject<Output, Type>*>(&subject)) {
						return &this->_object == &ptr->_object && this->_member.equals(ptr->_member);
					}
					return false;
				}
			};

			/// Animation subjects created by a \ref component_member_access. The subject is accessed through two
			/// layers: one custom layer that retrieves a member from an element, and one predefined layer that
			/// retrieves properties from that member. An optional callback is called whenever the value has been
			/// set.
			template <
				typename Intermediate, typename Target
			> class custom_element_member_subject : public typed_animation_subject<Target> {
			public:
				/// Initializes all fields of this struct.
				custom_element_member_subject(
					const typed_member_access<element, Intermediate> &first,
					const typed_member_access<Intermediate, Target> &second,
					element &obj, std::function<void(element&)> cb
				) :
					typed_animation_subject<Target>(),
					_callback(std::move(cb)), _first(first), _second(second), _source(obj) {
				}

				/// Returns the value.
				const Target &get() const override {
					return *_second.get_typed(*_first.get_typed(_source));
				}
				/// Sets the value, calling \ref scheduler::invalidate_layout() or
				/// \ref scheduler::invalidate_visual() if necessary.
				void set(Target t) override {
					*_second.get_typed(*_first.get_typed(_source)) = std::move(t);
					if (_callback) {
						_callback(_source);
					}
				}

				/// Checks if \ref _first, \ref _second, and \ref _source are the same if the other object is also
				/// of this type. Otherwise returns \p false.
				[[nodiscard]] bool equals(const animation_subject_base &other) const override {
					auto *o = dynamic_cast<const custom_element_member_subject<Intermediate, Target>*>(&other);
					if (o) {
						return o->_first.equals(_first) && o->_second.equals(_second) && &o->_source == &_source;
					}
					return false;
				}
			protected:
				std::function<void(element&)> _callback; ///< The callback that's invoked whenever the value is set.
				const typed_member_access<element, Intermediate> &_first; ///< The first part of the getter.
				const typed_member_access<Intermediate, Target> &_second; ///< The second part of the getter.
				element &_source; ///< The source \ref element.
			};

			/// Generic member access through a getter component.
			template <typename Comp> class component_member_access :
				public typed_member_access<typename Comp::input_type, typename Comp::output_type> {
			public:
				using input_type = typename Comp::input_type; ///< The input type.
				using output_type = typename Comp::output_type; ///< The output type.

				/// Initializes \ref _component.
				explicit component_member_access(Comp comp) : _component(std::move(comp)) {
				}

				/// Retrieves the member through \ref _component.
				output_type *get_typed(input_type &input) const override {
					return _component.get(&input);
				}

				/// Creates a \ref member_access_subject.
				std::unique_ptr<animation_subject_base> create_for_source(input_type &input) const override {
					return std::make_unique<member_access_subject<input_type, output_type>>(*this, input);
				}
				/// Creates a \ref custom_element_member_subject.
				std::unique_ptr<animation_subject_base> create_for_element_with_callback(
					element &elem, typed_member_access<element, input_type> &middle, std::function<void(element&)> callback
				) const override {
					return std::make_unique<custom_element_member_subject<input_type, output_type>>(
						middle, *this, elem, std::move(callback)
						);
				}

				/// Checks if the other \ref member_access is also a \ref component_member_access, and if so, checks
				/// if the components are equivalent.
				bool equals(const member_access<input_type> &other) const override {
					if (auto *acc = dynamic_cast<const component_member_access<Comp>*>(&other)) {
						return _component == acc->_component;
					}
					return false;
				}
			protected:
				Comp _component; ///< The getter component.
			};
			/// Shorthand for constructing \ref component_member_access objects.
			template <typename Comp> std::unique_ptr<
				component_member_access<std::decay_t<Comp>>
			> make_component_member_access(Comp comp) {
				return std::make_unique<component_member_access<std::decay_t<Comp>>>(std::move(comp));
			}


			/// Contains information about a member of an object.
			template <typename Source> struct member_information {
				std::unique_ptr<member_access<Source>> member; ///< The member.
				std::unique_ptr<animation_value_parser_base> parser; ///< Used to parse animations from JSON.
			};


			/// The components used to build the animation subject.
			namespace getter_components {
				/// A component that simply forwards its input.
				template <typename T> struct dummy_component {
					using input_type = T; ///< The input type.
					using output_type = T; ///< The output type.

					/// Returns the input as-is.
					inline static output_type *get(input_type *input) {
						return input;
					}

					/// Two instances are always equal.
					friend bool operator==(const dummy_component&, const dummy_component&) {
						return true;
					}
				};

				/// A component that retrieves values based on member pointers.
				template <auto MemPtr> struct member_component {
					/// The input type.
					using input_type = typename member_pointer_traits<decltype(MemPtr)>::owner_type;
					/// The output type.
					using output_type = typename member_pointer_traits<decltype(MemPtr)>::value_type;

					/// Returns the member.
					inline static output_type *get(input_type *input) {
						return input ? &(input->*MemPtr) : nullptr;
					}

					/// Two instances with the same template arguments are always equal.
					friend bool operator==(const member_component&, const member_component&) {
						return true;
					}
				};
				/// A component that retrieves values in a \p std::vector.
				template <typename T> struct array_component {
					using input_type = std::vector<T>; ///< The input type.
					using output_type = T; ///< The output type.

					/// Default constructor.
					array_component() = default;
					/// Initializes \ref index.
					explicit array_component(std::size_t i) : index(i) {
					}

					/// Returns the object at \ref index.
					output_type *get(input_type *input) const {
						if (input && index < input->size()) {
							return &((*input)[index]);
						}
						logger::get().log_warning(CP_HERE) <<
							demangle(typeid(*this).name()) << "[" << index << "]: index overflow";
						return nullptr;
					}

					std::size_t index = 0; ///< The index of the object containing the subject.

					/// Two instances are equal when their \ref index members are.
					friend bool operator==(const array_component &lhs, const array_component &rhs) {
						return lhs.index == rhs.index;
					}
				};
				/// A component that retrieves a specific type from a \p std::variant.
				template <typename Variant, typename Target> struct variant_component {
					using input_type = Variant; ///< The input type.
					using output_type = Target; ///< The output type.

					/// Returns the corresponding object.
					inline static output_type *get(input_type *input) {
						if (input && std::holds_alternative<Target>(*input)) {
							return &std::get<Target>(*input);
						}
						logger::get().log_warning(CP_HERE) <<
							demangle(typeid(variant_component<Variant, Target>).name()) << ": type mismatch";
						return nullptr;
					}

					/// Two instances with the same template arguments are always equal.
					friend bool operator==(const variant_component&, const variant_component&) {
						return true;
					}
				};
				/// A component used to cast the input pointer.
				template <typename Source, typename Target> struct dynamic_cast_component {
					using input_type = Source; ///< The input type.
					using output_type = Target; ///< The output type.

					/// Returns the cast pointer.
					inline static std::enable_if_t<std::is_base_of_v<input_type, output_type>, output_type*> get(
						input_type *input
					) {
						if (input) {
							if (auto *out = dynamic_cast<output_type*>(input)) {
								return out;
							}
							logger::get().log_warning(CP_HERE) <<
								demangle(typeid(dynamic_cast_component<Source, Target>).name()) << ": failed cast";
						}
						return nullptr;
					}

					/// Two instances with the same template arguments are always equal.
					friend bool operator==(const dynamic_cast_component&, const dynamic_cast_component&) {
						return true;
					}
				};

				/// Pairs the given components.
				///
				/// \todo C++20, [[no_unique_address]] or something.
				template <typename Comp1, typename Comp2> struct paired_component {
					static_assert(
						std::is_same_v<typename Comp1::output_type, typename Comp2::input_type>,
						"input / output types mismatch"
						);

					using input_type = typename Comp1::input_type; ///< The input type.
					using output_type = typename Comp2::output_type; ///< The output type.

					/// Default constructor.
					paired_component() = default;
					/// Initializes both components.
					paired_component(Comp1 c1, Comp2 c2) : comp1(c1), comp2(c2) {
					}

					/// Retrieves the subject using both \ref comp1 and \ref comp2.
					output_type *get(input_type *input) const {
						return comp2.get(comp1.get(input));
					}

					Comp1 comp1; ///< The first component.
					Comp2 comp2; ///< The second component.

					/// Compares both components of the given two pairs.
					friend bool operator==(const paired_component &lhs, const paired_component &rhs) {
						return lhs.comp1 == rhs.comp1 && lhs.comp2 == rhs.comp2;
					}
				};
				/// Constructs a \ref paired_component using the given two components.
				template <typename Comp1, typename Comp2> inline paired_component<
					std::decay_t<Comp1>, std::decay_t<Comp2>
				> pair(Comp1 c1, Comp2 c2) {
					return paired_component<std::decay_t<Comp1>, std::decay_t<Comp2>>(std::move(c1), std::move(c2));
				}
			}


			/// Interprets an animation path and returns the corresponding \ref member_information. Only certain
			/// instantiations of this function exist.
			template <typename T> member_information<T> get_member_subject(
				component_list::const_iterator, component_list::const_iterator
			);
			/// Dispatches the call given the visual property.
			member_information<element> get_common_element_property(
				component_list::const_iterator, component_list::const_iterator
			);
		}
	}

	/// Contains information about the subject of an animation.
	struct animation_subject_information {
		std::unique_ptr<animation_subject_base> subject; ///< The subject to be animated.
		std::unique_ptr<animation_value_parser_base> parser; ///< Used to parse animations from JSON.
		std::any subject_data; ///< Data that need to live as long as \ref subject.

	private:
		/// The callback used by \ref from_member() to invalidate the visual or layout of an element.
		template <
			animation_path::builder::element_property_type Type
		> inline static void _element_subject_callback(element&);
	public:
		/// Creates a \ref animation_subject_information from a
		/// \ref animation_path::builder::member_information<element> by calling
		/// \ref animation_path::builder::member_access::create_for_source().
		inline static animation_subject_information from_element(
			animation_path::builder::member_information<element> member, element &elem
		) {
			animation_subject_information res;
			res.parser = std::move(member.parser);
			res.subject = member.member->create_for_source(elem);
			std::shared_ptr<animation_path::builder::member_access<element>> ptr(std::move(member.member));
			res.subject_data.emplace<std::shared_ptr<animation_path::builder::member_access<element>>>(ptr);
			return res;
		}
		/// Similar to \ref from_element(), but calls
		/// \ref animation_path::builder::member_access::create_for_element_with_callback().
		template <
			typename Intermediate
		> inline static animation_subject_information from_element_custom_with_callback(
			animation_path::builder::member_information<Intermediate>,
			std::unique_ptr<animation_path::builder::typed_member_access<element, Intermediate>>,
			element&, std::function<void(element&)>
		);

		/// Creates a \ref animation_subject_information that retrieves a property using indirect means.
		template <auto Member> inline static animation_subject_information from_member_with_callback(
			element&, std::function<void(element&)>,
			animation_path::component_list::const_iterator, animation_path::component_list::const_iterator
		);
		/// Similar to \ref from_member_with_callback(), but replaces the callback with a simple enumeration
		/// that only invalidates the layout or visuals of the element.
		template <auto Member> inline static animation_subject_information from_member(
			element&, animation_path::builder::element_property_type,
			animation_path::component_list::const_iterator, animation_path::component_list::const_iterator
		);
	};
}
