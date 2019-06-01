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
			explicit component(str_view_t prop) : property(prop) {
			}
			/// Initializes all fields of this struct.
			component(str_view_t t, str_view_t prop) : type(t), property(prop) {
			}

			str_view_t
				type, ///< The expected type of the current object. Can be empty.
				property; ///< The target property.
			std::optional<size_t> index; ///< The index, if this component is a list.
		};
		using component_list = std::vector<component>; ///< A list of components.

		/// The type of an animation path.
		enum class type {
			visual_only, ///< The animation only affects the visual of an element.
			affects_layout ///< The animation affects the layout of an element.
		};

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
		template <typename Context> struct bootstrapper {
			using context = Context; ///< The type of the parser context.

			std::unique_ptr<subject_creator<element>> subject_creator; ///< Used to create animation subjects.
			/// Used to parse animations from JSON.
			std::unique_ptr<animation_parser_base<Context>> parser;
		};
		/// \ref bootstrapper with the default JSON engine.
		using default_bootstrapper = bootstrapper<ui_config_json_parser<json::default_engine::value_t>>;


		/// The parser.
		namespace parser {
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
			enum class parse_result {
				completed, ///< Success.
				not_found, ///< The path does not match the format at all.
				error ///< The path matches partially but is not complete.
			};

			/// Misceallneous parser functions.
			namespace _details {
				/// Parses a string that contains only a to z, 0 to 9, or underscores.
				inline parse_result parse_string(str_view_t::const_iterator &it, str_view_t::const_iterator end) {
					size_t nchars = 0;
					while (it != end) {
						if (*it != '_' && !(*it >= 'a' && *it <= 'z') && !(*it >= 'A' && *it <= 'Z')) {
							break;
						}
						++nchars;
						++it;
					}
					return nchars > 0 ? parse_result::completed : parse_result::not_found;
				}

				/// Parses an index.
				inline parse_result parse_index(str_view_t::const_iterator &it, str_view_t::const_iterator end, size_t &v) {
					if (it == end || *it != '[') {
						return parse_result::not_found;
					}
					++it;
					if (it == end || !(*it >= '0' && *it <= '9')) {
						return parse_result::error;
					}
					v = *(it++) - '0';
					while (it != end) {
						if (*it == ']') {
							++it;
							return parse_result::completed;
						}
						if (!(*it >= '0' && *it <= '9')) {
							return parse_result::error;
						}
						v = v * 10 + (*(it++) - '0');
					}
					return parse_result::error; // no closing bracket
				}

				/// Parses a typed component.
				inline parse_result parse_typed_component(
					str_view_t::const_iterator & it, str_view_t::const_iterator end, component & v
				) {
					if (it == end || *it != '(') {
						return parse_result::not_found;
					}
					// type
					auto beg = ++it;
					if (parse_string(it, end) != parse_result::completed) {
						return parse_result::error;
					}
					v.type = str_view_t(&*beg, it - beg);
					// dot
					if (it == end || *it != '.') {
						return parse_result::error;
					}
					// property
					beg = ++it;
					if (parse_string(it, end) != parse_result::completed) {
						return parse_result::error;
					}
					v.property = str_view_t(&*beg, it - beg);
					// index & closing parenthesis
					bool closed = false;
					if (it != end && *it == ')') {
						++it;
						closed = true;
					}
					size_t id = 0;
					parse_result res = parse_index(it, end, id);
					if (res == parse_result::error) {
						return parse_result::error;
					}
					if (res == parse_result::completed) {
						v.index.emplace(id);
					}
					if (it != end && *it == ')') {
						if (closed) {
							return parse_result::error;
						}
						++it;
						closed = true;
					}
					return closed ? parse_result::completed : parse_result::error;
				}

				/// Parses a typed component.
				inline parse_result parse_untyped_component(
					str_view_t::const_iterator & it, str_view_t::const_iterator end, component & v
				) {
					auto beg = it;
					if (parse_string(it, end) != parse_result::completed) {
						return parse_result::not_found;
					}
					v.property = str_view_t(&*beg, it - beg);
					size_t id;
					parse_result res = parse_index(it, end, id);
					if (res == parse_result::error) {
						return parse_result::error;
					}
					if (res == parse_result::completed) {
						v.index.emplace(id);
					}
					return parse_result::completed;
				}

				/// Parses a component.
				inline parse_result parse_component(
					str_view_t::const_iterator & it, str_view_t::const_iterator end, component & v
				) {
					parse_result res = parse_typed_component(it, end, v);
					if (res != parse_result::not_found) {
						return res;
					}
					return parse_untyped_component(it, end, v);
				}
			}

			/// Splits an animation target path into components. See the comment in 
			inline parse_result parse(str_view_t path, component_list &list) {
				if (path.empty()) {
					return parse_result::not_found;
				}
				auto it = path.begin();
				list.emplace_back();
				parse_result res = _details::parse_component(it, path.end(), list.back());
				if (res != parse_result::completed) {
					return parse_result::error;
				}
				while (it != path.end()) {
					if (*(it++) != '.') {
						return parse_result::error; // technically completed, but it's a single path
					}
					list.emplace_back();
					res = _details::parse_component(it, path.end(), list.back());
					if (res != parse_result::completed) {
						return parse_result::error;
					}
				}
				return parse_result::completed;
			}
		}

		/// Builds a \ref typed_animation_subject_base from a \ref component_list.
		namespace builder {
			template <typename, type> class member_subject_creator;

			/// Implementation of \ref typed_animation_subject_base that accesses the value through a getter
			/// component.
			template <typename Component, type Type> class member_subject :
				public typed_animation_subject_base<typename Component::output_type> {
			public:
				constexpr static type subject_type = Type; ///< The \ref type of the subject.

				using input_type = typename Component::input_type; ///< The input type of the component.
				using output_type = typename Component::output_type; ///< The tyep of the subject.

				/// Initializes all fields of this struct.
				member_subject(
					input_type &e, const member_subject_creator<Component, Type> &proto
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
				const member_subject_creator<Component, Type> &_prototype; ///< The object that created this.
			};

			/// Creates instances of \ref member_subject, given an instance of the input.
			template <typename Component, type Type> class member_subject_creator :
				public subject_creator<typename Component::input_type> {
				friend member_subject<Component, Type>;
			public:
				constexpr static type subject_type = Type; ///< The \ref type of the subject.

				using input_type = typename Component::input_type; ///< The input type of the component.

				/// Initializes \ref _comp.
				explicit member_subject_creator(Component comp) : _comp(comp) {
				}

				/// Creates a \ref member_subject for the given \ref element.
				std::unique_ptr<animation_subject_base> create_for(input_type &e) const override {
					return std::make_unique<member_subject<Component, Type>>(e, *this);
				}
			protected:
				Component _comp; ///< The component.
			};
			/// Creates a \ref member_subject_creator from the given getter component.
			template <
				type Type, typename Component
			> inline std::unique_ptr<member_subject_creator<Component, Type>> make_subject_creator(
				Component comp
			) {
				return std::make_unique<member_subject_creator<Component, Type>>(std::move(comp));
			}

			template <typename Component, type Type> inline bool member_subject<Component, Type>::equals(
				const animation_subject_base &other
			) const {
				auto *ptr = dynamic_cast<const member_subject<Component, Type>*>(&other);
				if (ptr) {
					return ptr->_prototype._comp == _prototype._comp;
				}
				return false;
			}

			/// The components used to build the animation subject.
			namespace getter_components {
				struct element_parameters_getter_component;

				/// A component that retrieves values based on member pointers.
				template <auto MemPtr> struct member_component {
					using input_type = typename member_pointer_traits<decltype(MemPtr)>::owner_type; ///< The input type.
					using output_type = typename member_pointer_traits<decltype(MemPtr)>::value_type; ///< The output type.

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

			/// Dispatches the call given the visual property.
			default_bootstrapper get_common_element_property(
				component_list::const_iterator, component_list::const_iterator
			);
		}

		/// Given an animation path, returns a corresponding \ref default_bootstrapper. The caller also has access to
		/// all components in the path.
		default_bootstrapper parse(str_view_t, std::vector<component>&);
		/// Given an animation path, returns a corresponding \ref default_bootstrapper.
		default_bootstrapper parse(str_view_t);
	}

	/// An aggregate of animations. Name borrowed from WPF.
	struct storyboard {
		/// Stores information about a single animation.
		struct entry {
			std::shared_ptr<animation_definition_base> definition; ///< The definition of this animation.
			std::shared_ptr<animation_path::subject_creator<element>> subject; ///< The subject of this animation.

			/// Creates a \ref playing_animation_base for the given \ref element.
			std::unique_ptr<playing_animation_base> start_for(element &e) const {
				return definition->start(subject->create_for(e));
			}
		};

		std::vector<entry> animations; ///< The list of animations.
	};
}
