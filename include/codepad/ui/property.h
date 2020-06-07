// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Properties that are used for element arrangements and animations.

#include <map>

#include "../core/json/storage.h"
#include "../core/misc.h"
#include "animation.h"
#include "animation_path.h"

namespace codepad::ui {
	class element;

	/// A property of an \ref element.
	class property {
	public:
		/// Default virtual destructor.
		virtual ~property() = default;

		/// Produces a \ref animation_object_information object associated with the given \ref element by parsing the
		/// given animation path. Note that the animation path input is the complete path that includes the first
		/// component used to identify this property.
		[[nodiscard]] virtual animation_subject_information parse_animation_path(
			element&, const animation_path::component_list&
		) const = 0;
		/// Sets the value of this property for the given \ref element.
		virtual void set_value(element&, const json::value_storage&) const = 0;
	};
	/// Mapping between property names and \ref property objects.
	using property_mapping = std::map<std::u8string, std::shared_ptr<property>>;


	/// A property of an \ref element that is accessed through a member pointer. The member must not be an array.
	template <auto MemberPtr> class member_pointer_property : public property {
	public:
		using owner_type = typename member_pointer_traits<decltype(MemberPtr)>::owner_type; ///< The type of the \ref element.
		using value_type = typename member_pointer_traits<decltype(MemberPtr)>::value_type; ///< The value type.

		/// Default constructor.
		member_pointer_property() = default;
		/// Initializes \ref modify_callback.
		explicit member_pointer_property(std::function<void(owner_type&)> callback) :
			modify_callback(std::move(callback)) {
		}

		/// Returns the result of \ref animation_subject_information::from_member_with_callback().
		[[nodiscard]] animation_subject_information parse_animation_path(
			element &elem, const animation_path::component_list &list
		) const override {
			if (list.begin()->index.has_value()) {
				logger::get().log_warning(CP_HERE) << "array properties are not yet supported";
			}
			if (owner_type *ptr = _get_owner_from(elem)) {
				std::function<void()> mod_callback;
				if (modify_callback) {
					mod_callback = [ptr, cb = modify_callback]() {
						cb(*ptr);
					};
				}
				return animation_subject_information::from_member_with_callback<MemberPtr>(
					elem, std::move(mod_callback), ++list.begin(), list.end()
				);
			}
			return animation_subject_information();
		}

		/// Sets the value, then calls \ref modify_callback.
		void set_value(element&, const json::value_storage&) const override;

		/// The callback function invoked whenever the value's modified.
		std::function<void(owner_type&)> modify_callback;
	protected:
		/// Tries to cast the generic \ref element into the owner type, and logs a warning message when it fails.
		[[nodiscard]] static owner_type *_get_owner_from(element&);
	};


	/// A property that is accessed through a getter function and a setter function.
	///
	/// \tparam Elem Element type. Should be \ref element or a class derived from \ref element.
	/// \tparam T Value type of this property
	template <typename Elem, typename T> class getter_setter_property : public property {
	public:
		/// Initializes all fields of this struct.
		getter_setter_property(
			std::u8string_view id, std::function<T(Elem&)> get, std::function<void(Elem&, T)> set
		) : getter(std::move(get)), setter(std::move(set)), identifier(id) {
		}

		/// Calls \ref animation_subject_information::from_getter_setter() to construct the animation subject.
		[[nodiscard]] animation_subject_information parse_animation_path(
			element &elem, const animation_path::component_list &list
		) const override {
			if (list.begin()->index.has_value()) {
				logger::get().log_warning(CP_HERE) << "array properties are not yet supported";
			}
			if (list.size() > 1) {
				logger::get().log_warning(CP_HERE) <<
					"only top-level properties are supported for get/set properties";
			}
			if constexpr (std::is_same_v<Elem, element>) {
				return animation_subject_information::from_getter_setter<T>(elem, identifier, getter, setter);
			} else {
				return animation_subject_information::from_getter_setter<T>(
					elem, identifier,
					[get = getter](element &generic) {
						if (Elem *elem = dynamic_cast<Elem*>(&generic)) {
							return get(*elem);
						}
						return T{};
					},
					[set = setter](element &generic, T value) {
						if (Elem *elem = dynamic_cast<Elem*>(&generic)) {
							set(*elem, std::move(value));
						}
					}
					);
			}
		}

		/// Sets the value through \ref setter.
		void set_value(element&, const json::value_storage&) const override;

		std::function<T(Elem&)> getter; ///< The getter function.
		std::function<void(Elem&, T)> setter; ///< The setter function.
		std::u8string_view identifier; ///< The identifier of this property.
	};
}
