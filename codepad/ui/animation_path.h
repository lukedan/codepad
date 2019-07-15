// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs used to control the animations of elements.

#include "../core/misc.h"
#include "animation.h"

namespace codepad::ui {
	class element;
	template <typename ValueType> class ui_config_json_parser;

	/// Used to parse strings into \ref typed_animation_target_base.
	namespace animation_path {
		/// A component in an animation path.
		struct component {
			/// Default constructor.
			component() = default;
			/// Initializes \ref property.
			explicit component(str_t prop) : property(std::move(prop)) {
			}
			/// Initializes all fields of this struct.
			component(str_t t, str_t prop) : type(std::move(t)), property(std::move(prop)) {
			}

			str_t
				type, ///< The expected type of the current object. Can be empty.
				property; ///< The target property.
			std::optional<size_t> index; ///< The index, if this component is a list.
		};
		using component_list = std::vector<component>; ///< A list of components.

		/// Basic interface used to create a \ref typed_animation_subject_base given a
		/// \ref element_parameters.
		///
		/// \tparam Source The source of the subject.
		template <typename Source> class subject_creator {
		public:
			/// Default destructor.
			virtual ~subject_creator() = default;

			/// Creates the corresponding \ref animation_subject_base for the given \ref element_parameters.
			virtual std::unique_ptr<animation_subject_base> create_for(Source&) const = 0;
		};

		/// Contains all necessary information for creating animations.
		template <typename Source> struct bootstrapper {
			std::unique_ptr<subject_creator<Source>> subject_creator; ///< Used to create animation subjects.
			std::unique_ptr<animation_value_parser_base> parser; ///< Used to parse animations from JSON.
		};


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
			inline static result parse(str_view_t path, component_list &list) {
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
			inline static result _parse_string(str_view_t::const_iterator &it, str_view_t::const_iterator end) {
				size_t nchars = 0;
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
				str_view_t::const_iterator &it, str_view_t::const_iterator end, size_t &v
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
				str_view_t::const_iterator &it, str_view_t::const_iterator end, component &v
			) {
				if (it == end || *it != '(') {
					return result::not_found;
				}
				// type
				auto beg = ++it;
				if (_parse_string(it, end) != result::completed) {
					return result::error;
				}
				v.type = str_t(&*beg, it - beg);
				// dot
				if (it == end || *it != '.') {
					return result::error;
				}
				// property
				beg = ++it;
				if (_parse_string(it, end) != result::completed) {
					return result::error;
				}
				v.property = str_t(&*beg, it - beg);
				// index & closing parenthesis
				bool closed = false;
				if (it != end && *it == ')') {
					++it;
					closed = true;
				}
				size_t id = 0;
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
				str_view_t::const_iterator &it, str_view_t::const_iterator end, component &v
			) {
				auto beg = it;
				if (_parse_string(it, end) != result::completed) {
					return result::not_found;
				}
				v.property = str_view_t(&*beg, it - beg);
				size_t id;
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
				str_view_t::const_iterator &it, str_view_t::const_iterator end, component &v
			) {
				result res = parse_typed_component(it, end, v);
				if (res != result::not_found) {
					return res;
				}
				return _parse_untyped_component(it, end, v);
			}
		};

		/// Builds a \ref typed_animation_subject_base from a \ref component_list.
		namespace builder {
			template <typename> class member_subject_creator;

			/// Implementation of \ref typed_animation_subject_base that accesses the value through a getter
			/// component.
			template <typename Component> class member_subject :
				public typed_animation_subject_base<typename Component::output_type> {
			public:
				using input_type = typename Component::input_type; ///< The input type of the component.
				using output_type = typename Component::output_type; ///< The tyep of the subject.

				/// Initializes all fields of this struct.
				member_subject(
					input_type &e, const member_subject_creator<Component> &proto
				) : _target(e), _prototype(proto) {
				}

				/// The getter.
				const output_type &get() const override;
				/// The setter. This function also invokes \ref scheduler::invalidate_layout() or
				/// \ref scheduler::invalidate_visuals() if necessary.
				void set(output_type) override;

				/// Uses \p dynamic_cast to check if the given \ref animation_subject_base is also a
				/// \ref member_subject, and if so, checks if the corresponding getter components are equal.
				bool equals(const animation_subject_base&) const override;
			protected:
				input_type &_target; ///< The \ref element_parameters struct whose value will be modified.
				const member_subject_creator<Component> &_prototype; ///< The object that created this.
			};

			/// Creates instances of \ref member_subject, given an instance of the input.
			template <typename Component> class member_subject_creator :
				public subject_creator<typename Component::input_type> {
				friend member_subject<Component>;
			public:
				using input_type = typename Component::input_type; ///< The input type of the component.

				/// Initializes \ref _comp.
				explicit member_subject_creator(Component comp) : _comp(std::move(comp)) {
				}

				/// Creates a \ref member_subject for the given input.
				std::unique_ptr<animation_subject_base> create_for(input_type &e) const override {
					return std::make_unique<member_subject<Component>>(e, *this);
				}
			protected:
				Component _comp; ///< The component.
			};

			template <typename Component> inline bool member_subject<Component>::equals(
				const animation_subject_base &other
			) const {
				auto *ptr = dynamic_cast<const member_subject<Component>*>(&other);
				if (ptr) {
					return ptr->_prototype._comp == _prototype._comp;
				}
				return false;
			}

			template <
				typename Component
			> inline auto member_subject<Component>::get() const -> const output_type& {
				return *_prototype._comp.get(&_target);
			}

			template <typename Component> inline void member_subject<Component>::set(output_type v) {
				output_type *ptr = _prototype._comp.get(&_target);
				if (ptr) {
					*ptr = std::move(v);
				}
			}


			/// Indicates the effects an element's property has on its visuals and layout.
			enum class element_property_type {
				visual_only, ///< The property only affects the element's visuals.
				affects_layout ///< The property affects the element's layout.
			};

			/// A member subject that also invalidates the visuals or layout of an element when set. If the property
			/// affects neither, use \ref member_subject.
			template <typename Comp, element_property_type Type> class element_member_subject :
				public member_subject<Comp> {
			public:
				using input_type = typename member_subject<Comp>::input_type; ///< The input type.
				using output_type = typename member_subject<Comp>::output_type; ///< The output type.

				static_assert(std::is_same_v<input_type, element>, "input type must be element");

				/// Forwarding constructor.
				element_member_subject(input_type &e, const member_subject_creator<Comp> &proto) :
					member_subject<Comp>(e, proto) {
				}

				/// Sets the value and calls \ref scheduler::invalidate_visual() or
				/// \ref scheduler::invalidate_layout() accordingly.
				void set(output_type) override;
			};

			/// Subject creators used with \ref element_member_subject.
			template <typename Comp, element_property_type Type> class element_member_subject_creator :
				public member_subject_creator<Comp> {
			public:
				using input_type = typename member_subject_creator<Comp>::input_type; ///< The input type.

				/// Forwarding constructor.
				explicit element_member_subject_creator(Comp comp) : member_subject_creator<Comp>(std::move(comp)) {
				}

				/// Creates a \ref element_member_subject for the given \ref element.
				std::unique_ptr<animation_subject_base> create_for(input_type &e) const override {
					return std::make_unique<element_member_subject<Comp, Type>>(e, *this);
				}
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
					explicit array_component(size_t i) : index(i) {
					}

					/// Returns the object at \ref index.
					output_type *get(input_type *input) const {
						if (input && index < input->size()) {
							return &((*input)[index]);
						}
						return nullptr;
					}

					size_t index = 0; ///< The index of the object containing the subject.

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
						return nullptr;
					}

					/// Two instances with the same template arguments are always equal.
					friend bool operator==(const variant_component&, const variant_component&) {
						return true;
					}
				};

				/// Pairs the given components.
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
				template <typename Comp1, typename Comp2> inline paired_component<Comp1, Comp2> pair(
					Comp1 c1, Comp2 c2
				) {
					return paired_component<Comp1, Comp2>(std::move(c1), std::move(c2));
				}
			}

			/// Interprets an animation path and returns the corresponding \ref subject_creator. Only certain
			/// instantiations of this function exist.
			template <typename T> bootstrapper<T> get_property(
				component_list::const_iterator, component_list::const_iterator
			);
			/// Dispatches the call given the visual property.
			bootstrapper<element> get_common_element_property(
				component_list::const_iterator, component_list::const_iterator
			);
		}
	}
}
