// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of the \ref codepad::tree_sitter::interpretation_interface class.

#include <tree_sitter/api.h>

#include <codepad/editors/code/interpretation.h>

#include "language_configuration.h"
#include "highlight_iterator.h"

namespace codepad::tree_sitter {
	/// Interface between the editor and \p tree-sitter.
	class interpretation_interface {
	public:
		/// Constructor.
		interpretation_interface(
			codepad::editors::code::interpretation &interp, const language_configuration *config
		) : _interp(&interp), _lang(config) {
			// create parser
			_parser.set(ts_parser_new());

			// this object is owned by the interpretation and will be disposed when it's disposed
			// so no need to unregister
			interp.end_edit_interpret += [this](codepad::editors::buffer::end_edit_info&) {
				_update_highlight();
			};
			_update_highlight();
		}
	protected:
		parser_ptr _parser; ///< The parser.
		codepad::editors::code::interpretation *_interp = nullptr; ///< The associated interpretation.
		const language_configuration *_lang = nullptr; ///< The language configuration.

		/// Contains information used in a \p TSInput.
		struct _payload {
			codepad::editors::byte_string read_buffer;
			codepad::editors::code::interpretation &interpretation; ///< The interpretation.
		};

		/// Updates the highlight of \ref _interp.
		void _update_highlight();
	};
}
