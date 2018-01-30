#pragma once

#include "commands.h"

namespace codepad::ui::native_commands {
	template <typename Elem> inline std::function<void(element*)> convert_type(std::function<void(Elem*)> f) {
		static_assert(std::is_base_of_v<element, Elem>, "invalid element type");
		return[func = std::move(f)](element *e) {
			Elem *te = dynamic_cast<Elem*>(e);
			if (e != nullptr && te == nullptr) {
				logger::get().log_warning(
					CP_HERE, "callback with invalid element type ",
					demangle(typeid(*e).name()), ", expected ", demangle(typeid(Elem).name())
				);
				return;
			}
			func(te);
		};
	}

	void register_all();
}
