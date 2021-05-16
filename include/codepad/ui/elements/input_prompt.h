// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A text input box with a feedback panel.

#include "codepad/ui/panel.h"
#include "text_edit.h"

namespace codepad::ui {
	/// Base class of input prompts. Derived classes can require additional references to elements that show
	/// additional information, such as options or candidates.
	class input_prompt : public panel {
	public:
		/// Returns the name for \ref _input.
		inline static std::u8string_view get_text_input_name() {
			return u8"input";
		}
	protected:
		text_edit *_input = nullptr; ///< The input \ref text_edit.

		/// Handles the reference to the textbox.
		bool _handle_reference(std::u8string_view name, element *elem) {
			if (name == get_text_input_name()) {
				_reference_cast_to(_input, elem);
				_input->text_changed += [this]() {
					_on_input_changed();
				};
				return true;
			}
			return panel::_handle_reference(name, elem);
		}

		/// Called when the text of \ref _input has changed.
		virtual void _on_input_changed() = 0;
	};
}
