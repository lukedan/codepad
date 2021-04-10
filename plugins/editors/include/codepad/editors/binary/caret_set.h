// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Caret set for the binary editor.

#include "codepad/editors/caret_set.h"

namespace codepad::editors::binary {
	/// No additional data associated with a caret.
	struct caret_data {
	};

	/// A set of carets.
	class caret_set : public caret_set_base<caret_data, caret_set> {
	public:
		using position = std::size_t; ///< The position of a caret is identified by a single \p std::size_t.
		/// The selection is a pair of positions, i.e., no additional data associated with the caret.
		using selection = ui::caret_selection;

		/// Wrapper around \ref caret_selection::move_caret().
		inline static void set_caret_position(selection &s, position pos) {
			s.move_caret(pos);
		}
		/// Wrapper around \ref caret_selection::get_caret_position().
		[[nodiscard]] inline static position get_caret_position(selection s) {
			return s.get_caret_position();
		}

		/// Returns \ref iterator_position::get_caret_selection().
		[[nodiscard]] inline static selection get_caret_selection(iterator_position it) {
			return it.get_caret_selection();
		}
	};
}
