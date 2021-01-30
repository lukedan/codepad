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
		/// Creates a new parser, registers to \ref editors::buffer::end_edit, and starts highlighting for this
		/// interpretation.
		interpretation_interface(editors::code::interpretation&, const language_configuration*);
		/// Assert during copy construction.
		interpretation_interface(const interpretation_interface&) {
			assert_true_logical(false, "interpretation_interface cannot be copied");
		}
		/// Assert during copy assignment.
		interpretation_interface &operator=(const interpretation_interface&) {
			assert_true_logical(false, "interpretation_interface cannot be copied");
			return *this;
		}
		/// Unregisters from \ref editors::buffer::end_edit.
		~interpretation_interface() {
			// TODO cancel queued highlight tasks
			_interp->get_buffer()->begin_edit -= _begin_edit_token;
			_interp->get_buffer()->end_edit -= _end_edit_token;
		}

		/// Computes and returns the new highlight for the document. This function does not create a
		/// \ref editors::buffer::async_reader_lock - it is the responsibility of the caller to do so when necessary.
		[[nodiscard]] editors::code::text_theme_data compute_highlight(std::size_t *cancellation_token);
		/// Queues this interpretation for highlighting. Skips if no language is associated with this interpretation.
		void queue_highlight();

		/// Returns the \ref editors::code::interpretation associated with this object.
		[[nodiscard]] editors::code::interpretation &get_interpretation() const {
			return *_interp;
		}
	protected:
		parser_ptr _parser; ///< The parser.
		editors::code::interpretation *_interp = nullptr; ///< The associated interpretation.
		/// Token for \ref editors::buffer::begin_edit.
		info_event<editors::buffer::begin_edit_info>::token _begin_edit_token;
		/// Token for \ref editors::buffer::end_edit.
		info_event<editors::buffer::end_edit_info>::token _end_edit_token;
		const language_configuration *_lang = nullptr; ///< The language configuration.

		/// Contains information used in a \p TSInput.
		struct _payload {
			editors::byte_string read_buffer; ///< Intermediate buffer.
			const editors::code::interpretation &interpretation; ///< Used to read the buffer.
		};
	};
}
