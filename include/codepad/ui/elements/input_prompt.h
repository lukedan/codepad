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
		/// Called when the user finishes entering input. This should normally be bound to the user pressing enter -
		/// additional comands such as ctrl+enter can be bound if multi-line input is expected.
		virtual void on_confirm() = 0;

		/// Returns the name for \ref _input.
		inline static std::u8string_view get_text_input_name() {
			return u8"input_textbox";
		}
	protected:
		textbox *_input = nullptr; ///< The input \ref textbox.

		/// Handles the reference to the textbox.
		bool _handle_reference(std::u8string_view name, element *elem) {
			if (name == get_text_input_name()) {
				_reference_cast_to(_input, elem);
				return true;
			}
			return panel::_handle_reference(name, elem);
		}
		/// Registers for \ref text_edit::text_changed.
		void _on_hierarchy_constructed() override {
			_input->get_text_edit()->text_changed += [this]() {
				_on_input_changed();
			};
			panel::_on_hierarchy_constructed();
		}

		/// Called when the text of \ref _input has changed.
		virtual void _on_input_changed() = 0;
	};
}
