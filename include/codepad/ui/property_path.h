// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Classes and structs used to access the properties of elements.

#include <variant>
#include <memory>
#include <typeindex>

#include "codepad/core/misc.h"
#include "codepad/core/json/default_engine.h"

namespace codepad::ui::property_path {
	/// A component in an property path.
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

	/// Converts the given property path into a string.
	std::u8string to_string(component_list::const_iterator begin, component_list::const_iterator end);


	/// Stores a \p void pointer and its real type. The pointer must be retrieved using the exact type -
	/// upcasting is not supported.
	struct any_ptr {
	public:
		/// Initializes this pointer to be empty.
		any_ptr() : _type(typeid(void)) {
		}
		/// Initializes this pointer to be empty.
		any_ptr(std::nullptr_t) : _type(typeid(void)) {
		}
		/// Initializes this pointer using the given typed pointer.
		template <typename T> any_ptr(T *ptr) : _type(typeid(T)), _ptr(ptr) {
		}

		/// If the stored pointer is exactly of type \p T, returns the pointer. Otherwise returns \p nullptr.
		template <typename T> [[nodiscard]] T *get() const {
			if (std::type_index(typeid(T)) == _type) {
				return static_cast<T*>(_ptr);
			}
			return nullptr;
		}

		/// Returns whether this pointer is empty.
		[[nodiscard]] bool empty() const {
			return _ptr == nullptr;
		}
		/// Tests whether this pointer is empty.
		[[nodiscard]] explicit operator bool() const {
			return !empty();
		}
	protected:
		std::type_index _type; ///< The type of the pointer.
		void *_ptr = nullptr; ///< The pointer.
	};


	/// Classes used to retrieve a pointer to the desired property from an object.
	namespace address_accessor_components {
		/// Base class of address accessor components.
		class component_base {
		public:
			/// Default virtual destructor.
			virtual ~component_base() = default;

			/// Retrieves a pointer to the property from the object.
			[[nodiscard]] virtual any_ptr get(const any_ptr&) const = 0;

			/// Tests the equality of two components.
			[[nodiscard]] virtual bool equals(const component_base&) const = 0;
		};

		/// A component that retrieves the value through a member pointer.
		template <auto MemberPtr> class member_pointer_component : public component_base {
		public:
			/// Retrieves the value through \p MemberPtr.
			[[nodiscard]] any_ptr get(const any_ptr &p) const override {
				auto *obj = p.get<typename member_pointer_traits<decltype(MemberPtr)>::owner_type>();
				if (obj) {
					return &(obj->*MemberPtr);
				}
				return nullptr;
			}

			/// Tests equality using \p dynamic_cast.
			bool equals(const component_base &rhs) const override {
				return dynamic_cast<const member_pointer_component*>(&rhs) != nullptr;
			}
		};
		/// A component that retrieves the value at the given index from a \p std::vector.
		template <typename T> class array_component : public component_base {
		public:
			/// Initializes \ref index.
			explicit array_component(std::size_t i) : index(i) {
			}

			/// Retrieves the value through \p MemberPtr.
			[[nodiscard]] any_ptr get(const any_ptr &p) const override {
				auto *obj = p.get<std::vector<T>>();
				if (obj && obj->size() > index) {
					return &((*obj)[index]);
				}
				return nullptr;
			}

			/// Tests equality using \p dynamic_cast, then tests \ref index.
			bool equals(const component_base &rhs) const override {
				if (auto comp = dynamic_cast<const array_component*>(&rhs)) {
					return comp->index == index;
				}
				return false;
			}

			std::size_t index = 0; ///< The index.
		};
		/// Used to access an object in a \p std::variant.
		template <typename Target, typename Variant> class variant_component : public component_base {
		public:
			/// Checks if the given \p std::variant holds an object of the desired type. If so, returns a pointer to
			/// it; otherwise returns \p nullptr.
			[[nodiscard]] any_ptr get(const any_ptr &p) const override {
				auto *var = p.get<Variant>();
				if (var && std::holds_alternative<Target>(*var)) {
					return &std::get<Target>(*var);
				}
				return nullptr;
			}

			/// Tests equality using \p dynamic_cast.
			bool equals(const component_base &rhs) const override {
				return dynamic_cast<const variant_component*>(&rhs) != nullptr;
			}
		};
		/// A component used to \p dynamic_cast the pointer to the correct type.
		template <typename Target, typename Source> class dynamic_cast_component : public component_base {
		public:
			/// Casts the pointer. If the cast fails, the returned \ref any_ptr will still hold the type information
			/// of the target type, but the pointer will be \ref nullptr.
			[[nodiscard]] any_ptr get(const any_ptr &p) const override {
				if (auto *var = p.get<Source>()) {
					return dynamic_cast<Target*>(var);
				}
				return nullptr;
			}

			/// Tests equality using \p dynamic_cast.
			bool equals(const component_base &rhs) const override {
				return dynamic_cast<const dynamic_cast_component*>(&rhs) != nullptr;
			}
		};
		/// A component that dereferences a pointer-like object.
		template <typename Ptr> class dereference_component : public component_base {
		public:
			/// Casts the pointer and dereferences it.
			[[nodiscard]] any_ptr get(const any_ptr &p) const override {
				if (auto *var = p.get<Ptr>()) {
					return &**var;
				}
				return nullptr;
			}

			/// Tests equality using \p dynamic_cast.
			bool equals(const component_base &rhs) const override {
				return dynamic_cast<const dereference_component*>(&rhs) != nullptr;
			}
		};
	}


	/// Classes used to access an attribute of an object given an \ref any_ptr to the object.
	namespace accessors {
		/// Untyped base class of all accessors.
		class accessor_base {
		public:
			/// Default virtual destructor.
			virtual ~accessor_base() = default;

			/// Tests the equality of two accessors.
			[[nodiscard]] virtual bool equals(const accessor_base&) const = 0;
		};
		/// Base class of typed accessors.
		template <typename T> class typed_accessor : public accessor_base {
		public:
			/// Reads the value.
			[[nodiscard]] virtual std::optional<T> get_value(any_ptr) const = 0;
			/// Writes the value.
			virtual void set_value(any_ptr, T) const = 0;
		};
		/// An accessor that uses an array of \ref address_accessor_components::component_base objects to access the
		/// value.
		template <typename T> class address_accessor : public typed_accessor<T> {
		public:
			/// Initializes \ref components.
			address_accessor(
				std::vector<std::unique_ptr<address_accessor_components::component_base>> comps,
				std::function<void(any_ptr)> mod_callback
			) : components(std::move(comps)), modification_callback(std::move(mod_callback)) {
			}

			/// Walks through all components to find the pointer to the actual value, casts it to the desired type
			/// and returns.
			std::optional<T> get_value(any_ptr ptr) const override {
				for (auto &comp : components) {
					if (!ptr) { // break early
						break;
					}
					ptr = comp->get(ptr);
				}
				if (auto *result = ptr.get<T>()) {
					return *result;
				}
				logger::get().log_error(CP_HERE) << "failed to get value of type " << demangle(typeid(T).name());
				return std::nullopt;
			}
			/// Walks through all components to find the pointer to the actual value and sets it. No-op if the
			/// components failed to find a valid pointer.
			void set_value(any_ptr ptr, T val) const override {
				any_ptr current = ptr;
				for (auto &comp : components) {
					if (!current) { // break early
						break;
					}
					current = comp->get(current);
				}
				if (auto *result = current.get<T>()) {
					*result = std::move(val);
					if (modification_callback) {
						modification_callback(ptr);
					}
				} else {
					logger::get().log_error(CP_HERE) << "failed to set value of type " << demangle(typeid(T).name());
				}
			}

			/// Tests all components for equality. \ref modification_callback is not checked.
			bool equals(const accessor_base &rhs) const override {
				if (auto *typed = dynamic_cast<const address_accessor*>(&rhs)) {
					if (typed->components.size() != components.size()) {
						return false;
					}
					for (std::size_t i = 0; i < components.size(); ++i) {
						if (!typed->components[i]->equals(*components[i])) {
							return false;
						}
					}
					return true;
				}
				return false;
			}

			/// The array of components.
			std::vector<std::unique_ptr<address_accessor_components::component_base>> components;
			/// The callback function that's invoked when \ref set_value() is called.
			std::function<void(any_ptr)> modification_callback;
		};
		/// A \ref typed_accessor that accesses the value via a getter and a setter.
		template <typename T, typename Owner> class getter_setter_accessor : public typed_accessor<T> {
		public:
			/// Initializes the getter and the setter.
			getter_setter_accessor(
				std::function<std::optional<T>(const Owner&)> get, std::function<void(Owner&, T)> set,
				std::u8string_view id
			) : getter(std::move(get)), setter(std::move(set)), identifier(id) {
			}

			/// Returns the value via \ref getter.
			std::optional<T> get_value(any_ptr ptr) const override {
				if (auto *typed_ptr = ptr.get<Owner>()) {
					return getter(*typed_ptr);
				}
				logger::get().log_error(CP_HERE) <<
					"failed to get value of type " << demangle(typeid(T).name()) << " using getter";
				return std::nullopt;
			}
			/// Sets the value via \ref setter.
			void set_value(any_ptr ptr, T val) const override {
				if (auto *typed_ptr = ptr.get<Owner>()) {
					setter(*typed_ptr, std::move(val));
				} else {
					logger::get().log_error(CP_HERE) << "invalid type for getter_setter_accessor";
				}
			}

			/// Checks if the \ref identifier of the two accessors are the same.
			bool equals(const accessor_base &rhs) const override{
				if (auto *typed = dynamic_cast<const getter_setter_accessor*>(&rhs)) {
					return typed->identifier == identifier;
				}
				return false;
			}

			std::function<std::optional<T>(const Owner&)> getter; ///< The getter function.
			std::function<void(Owner&, T)> setter; ///< The setter function.
			std::u8string_view identifier; ///< Identifier used in equality tests.
		};
	}
}
