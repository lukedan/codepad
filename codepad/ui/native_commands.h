// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Commands that are natively supported.

#include "commands.h"

namespace codepad::ui::native_commands {
	/// Wraps a function that accepts a certain type of element into a function that accepts a \ref element.
	template <typename Elem> inline std::function<void(element*)> convert_type(std::function<void(Elem*)> f) {
		static_assert(std::is_base_of_v<element, Elem>, "invalid element type");
		return[func = std::move(f)](element *e) {
			Elem *te = dynamic_cast<Elem*>(e);
			if (e != nullptr && te == nullptr) { // not the right type
				logger::get().log_warning(
					CP_HERE, "callback with invalid element type ",
					demangle(typeid(*e).name()), ", expected ", demangle(typeid(Elem).name())
				);
				return;
			}
			func(te);
		};
	}

	/// Registers all native commands.
	void register_all(command_registry&);
}
