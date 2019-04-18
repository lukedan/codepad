// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Additional classes that handle different element classes.

#include <map>

#include "../core/json.h"
#include "hotkey_registry.h"
#include "arrangements.h"
#include "element_parameters.h"

namespace codepad::ui {
	/// Hotkey groups that are used with elements. Each hotkey corresponds to a command name.
	using class_hotkey_group = hotkey_group<str_t>;

	/// Registry of a certain attribute of each element class.
	template <typename T> class class_registry {
	public:
		std::map<str_t, T, std::less<>> mapping; ///< The mapping between class names and attribute values.

		/// Returns the attribute corresponding to the given class name. If noine exists, the default attribute (the
		/// one corresponding to an empty class name) is returned.
		const T &get_or_default(str_view_t cls) {
			auto found = mapping.find(cls);
			return found == mapping.end() ? mapping[str_t()] : found->second;
		}
		/// Returns the attribute corresponding to the given class name. Returns \p nullptr if none exists.
		const T *get(str_view_t cls) {
			auto found = mapping.find(cls);
			return found == mapping.end() ? nullptr : &found->second;
		}
	};
	/// Registry of the arrangements of each element class.
	using class_arrangements_registry = class_registry<class_arrangements>;
	/// Registry of the hotkeys of each element class.
	using class_hotkeys_registry = class_registry<class_hotkey_group>;
}
