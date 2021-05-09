// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Container that handles a series of references.

#include <type_traits>

#include "codepad/ui/panel.h"

namespace codepad::ui {
	/// A class that stores references and uses them for displaying information.
	class reference_container : public panel {
	public:
		/// A reference.
		template <typename Elem> struct reference {
			static_assert(std::is_base_of_v<element, Elem>, "references to non-element are invalid");
			friend reference_container;
		public:
			/// Default constructor.
			reference() = default;

			/// Returns the referenced element.
			[[nodiscard]] Elem *get() const {
				return _element;
			}
			/// \overload
			[[nodiscard]] Elem *operator->() const {
				return _element;
			}
			/// \overload
			[[nodiscard]] Elem &operator*() const {
				return *_element;
			}

			/// Returns whether this references a valid element.
			[[nodiscard]] bool is_empty() const {
				return _element == nullptr;
			}
			/// Tests if this reference is non-empty.
			[[nodiscard]] explicit operator bool() const {
				return !is_empty();
			}
		protected:
			/// Initializes \ref _element.
			explicit reference(Elem *elem) : _element(elem) {
			}

			Elem *_element = nullptr; ///< The referenced element.
		};

		/// Finds a reference. If no such reference is found or if it fails to cast the element to the correct type,
		/// returns an empty \ref reference.
		template <typename Elem> [[nodiscard]] reference<Elem> try_get_reference(std::u8string_view name) const {
			auto it = _references.find(name);
			if (it != _references.end()) {
				if (auto *elem = dynamic_cast<Elem*>(it->second)) {
					return reference<Elem>(elem);
				}
			}
			return reference<Elem>();
		}
		/// Similar to \ref try_get_reference(), but logs error messages when the reference cannot be obtained.
		template <typename Elem> [[nodiscard]] reference<Elem> get_reference(std::u8string_view name) const {
			auto it = _references.find(name);
			if (it != _references.end()) {
				if (auto *elem = dynamic_cast<Elem*>(it->second)) {
					return reference<Elem>(elem);
				} else {
					logger::get().log_error(CP_HERE) <<
						"failed: reference '" << name << "' exists, but is not of the expected type";
				}
			} else {
				logger::get().log_error(CP_HERE) << "failed: no reference named '" << name << "'";
			}
			return reference<Elem>();
		}

		/// Returns the default class of elements of this type.
		inline static std::u8string_view get_default_class() {
			return u8"reference_container";
		}
	protected:
		std::map<std::u8string, element*, std::less<>> _references; ///< The list of references.

		/// Adds the given reference to \ref _references.
		bool _handle_reference(std::u8string_view ref, element *elem) {
			auto [it, inserted] = _references.emplace(ref, elem);
			if (!inserted) {
				logger::get().log_error(CP_HERE) << "duplicate references: " << ref;
			}
			return true;
		}
	};
}
