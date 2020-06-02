// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional classes that handle different element classes.

#include <map>

#include "hotkey_registry.h"
#include "arrangements.h"
#include "element_parameters.h"

namespace codepad::ui {
	/// Registry of a certain attribute of each element class.
	template <typename T> class class_registry {
	public:
		std::map<std::u8string, T, std::less<>> mapping; ///< The mapping between class names and attribute values.

		/// Returns the attribute corresponding to the given class name. If noine exists, the default attribute (the
		/// one corresponding to an empty class name) is returned.
		const T &get_or_default(std::u8string_view cls) {
			auto found = mapping.find(cls);
			return found == mapping.end() ? mapping[std::u8string()] : found->second;
		}
		/// Returns the attribute corresponding to the given class name. Returns \p nullptr if none exists.
		const T *get(std::u8string_view cls) {
			auto found = mapping.find(cls);
			return found == mapping.end() ? nullptr : &found->second;
		}
	};
	/// Registry of the arrangements of each element class.
	using class_arrangements_registry = class_registry<class_arrangements>;
	/// Registry of the hotkeys of each element class.
	using class_hotkeys_registry = class_registry<hotkey_group>;
}
