// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Handles operations on individual documents.

#include <codepad/core/event.h>
#include <codepad/editors/buffer.h>
#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

#include "client.h"

namespace codepad::lsp {
	/// Tag struct for \ref editors::code::interpretation used to implement LSP clients.
	class interpretation_tag {
		friend std::any;
	public:
		/// Registers events and sends the \p didOpen event.
		interpretation_tag(editors::code::interpretation&, client&);
		/// Assert in copy constructor.
		interpretation_tag(const interpretation_tag&) {
			assert_true_logical(false, "copy construction is disabled for interpretation_tag");
		}
		/// Assert in copy assignment.
		interpretation_tag &operator=(const interpretation_tag&) {
			assert_true_logical(false, "copy assignment is disabled for interpretation_tag");
			return *this;
		}
		/// Unregisters events and sends the \p didClose event.
		~interpretation_tag() {
			_interp->get_buffer()->begin_edit -= _begin_edit_token;
			_interp->modification_decoded -= _modification_decoded_token;
			_interp->end_modification -= _end_modification_token;
			_interp->get_buffer()->end_edit -= _end_edit_token;

			types::DidCloseTextDocumentParams params;
			params.textDocument.uri = _change_params.textDocument.uri;
			_client->send_notification(u8"textDocument/didClose", params);
		}

		/// Invoked when a new \ref editors::code::interpretation is created, this function creates a tag object if
		/// the interpretation is associated with a file on disk.
		static void on_interpretation_created(
			editors::code::interpretation&, client&, const editors::buffer_manager::interpretation_tag_token&
		);
	protected:
		/// Information about a single semantic token.
		struct _semantic_token {
			types::uinteger
				deltaLine = 0, ///< Delta of line number.
				/// Delta of start character - relative to 0 if the previous token was not on the same line.
				deltaStart = 0,
				length = 0, ///< The length of this token.
				tokenType = 0, ///< Token type.
				tokenModifiers = 0; ///< Token modifiers.

			/// Iterates over the range of numbers and generates \ref semantic_token objects. If the array length is
			/// not a multiple of 5, an error message will be logged and the excess part is discarded.
			template <typename Callback> static void iterate_over_range(
				const std::vector<types::uinteger> &arr, Callback &&cb
			) {
				if (arr.size() % 5 != 0) {
					logger::get().log_error(CP_HERE) << "semantic token array size is not a multiple of 5";
				}
				for (std::size_t i = 0; i + 4 < arr.size(); i += 5) {
					_semantic_token tok;
					tok.deltaLine = arr[i];
					tok.deltaStart = arr[i + 1];
					tok.length = arr[i + 2];
					tok.tokenType = arr[i + 3];
					tok.tokenModifiers = arr[i + 4];
					cb(tok);
				}
			}
		};

		/// Token used to listen to \ref editors::buffer::begin_edit.
		info_event<editors::buffer::begin_edit_info>::token _begin_edit_token;
		/// Token used to listen to \ref editors::code::interpretation::modification_decoded.
		info_event<editors::code::interpretation::modification_decoded_info>::token _modification_decoded_token;
		/// Token used to listen to \ref editors::code::interpretation::end_modification.
		info_event<editors::code::interpretation::end_modification_info>::token _end_modification_token;
		/// Token used to listen to \ref editors::buffer::end_edit.
		info_event<editors::buffer::end_edit_info>::token _end_edit_token;
		/// Stores information about the ongoing change to the document. Some fields of this struct such as document
		/// identifier persist between edits.
		types::DidChangeTextDocumentParams _change_params;
		std::size_t
			/// The number of additional codepoints to report **before** the start of the current modification. This
			/// is used when part of a linebreak is modified.
			_change_start_offset = 0,
			/// The number of additional codepoints to report **after** the end of the current modification. This is
			/// used when part of a linebreak is modified.
			_change_end_offset = 0,
			/// Number of versions of this interpretation that has been queued for highlighting. Highlights should
			/// only be applied when it'll be applied to the newest version.
			_queued_highlight_version = 0;
		editors::code::interpretation *_interp = nullptr; ///< The \ref interpretation this tag is associated with.
		client *_client = nullptr; ///< The client responsible for this document.


		/// Handler for \ref editors::buffer::begin_edit.
		void _on_begin_edit(editors::buffer::begin_edit_info&) {
			_change_params.contentChanges.value.clear();
		}
		/// Handler for \ref editors::code::interpretation::modification_decoded.
		void _on_modification_decoded(editors::code::interpretation::modification_decoded_info&);
		/// Handler for \ref editors::code::interpretation::end_modification.
		void _on_end_modification(editors::code::interpretation::end_modification_info&);
		/// Handler for \ref editors::buffer::end_edit. Sends the \p didChange notification, and also sends the
		/// \p semanticTokens request.
		void _on_end_edit(editors::buffer::end_edit_info&);

		/// Handler for the response of \p semanticTokens.
		void _on_semanticTokens(types::SemanticTokensResponse);
	};
}
