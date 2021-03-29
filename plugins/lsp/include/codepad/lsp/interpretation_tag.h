// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Handles operations on individual documents.

#include <codepad/core/event.h>
#include <codepad/ui/elements/label.h>
#include <codepad/editors/buffer.h>
#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

#include "client.h"
#include "types/diagnostics.h"

namespace codepad::lsp {
	class interpretation_tag;

	/// A tooltip that contains only text.
	class hover_tooltip : public editors::code::tooltip {
	public:
		/// Initializes \ref _parent and \ref _label, and sends a \p textDocument/hover request.
		hover_tooltip(interpretation_tag&, std::size_t pos);
		/// Cancels the handling of the hover request if necessary.
		~hover_tooltip() {
			if (!_token.empty()) {
				_token.cancel_handler();
			}
		}

		/// Returns \ref _label.
		ui::element *get_element() override {
			return _label;
		}
	protected:
		/// Token of the request, used for cancelling the request when the tooltip is destroyed early.
		client::request_token _token;
		ui::label *_label = nullptr; ///< The \ref ui::label used for displaying text.
		interpretation_tag *_parent = nullptr; ///< The \ref interpretation_tag that created this object.

		/// Handles the response to the request sent in the constructor.
		void _handle_reply(types::HoverResponse);
	};
	/// Provides hover tooltips for an \ref editors::code::interpretation.
	class hover_tooltip_provider : public editors::code::tooltip_provider {
	public:
		/// Initializes \ref _parent.
		explicit hover_tooltip_provider(interpretation_tag &p) : _parent(&p) {
		}

		/// Creates a \ref text_tooltip.
		std::unique_ptr<editors::code::tooltip> request_tooltip(std::size_t) override;
	protected:
		interpretation_tag *_parent = nullptr; ///< The \ref interpretation_tag that created this object.
	};

	/// Provides tooltips for diagnostics.
	class diagnostic_tooltip_provider : public editors::code::tooltip_provider {
	public:
		/// Initializes \ref _parent.
		explicit diagnostic_tooltip_provider(interpretation_tag &p) : _parent(&p) {
		}

		/// Checks for diagnostics at the given position and returns a \ref editors::code::simple_tooltip for those
		/// data if necessary.
		std::unique_ptr<editors::code::tooltip> request_tooltip(std::size_t) override;
	protected:
		interpretation_tag *_parent = nullptr; ///< The \ref interpretation_tag that created this object.
	};

	/// Tag struct for \ref editors::code::interpretation used to implement LSP clients.
	class interpretation_tag {
		friend std::any;
	public:
		/// Registers events, creates a decoration provider, and sends the \p didOpen event.
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
		/// Unregisters events, removes the decoration provider, and sends the \p didClose event.
		~interpretation_tag() {
			_interp->get_buffer().begin_edit -= _begin_edit_token;
			_interp->modification_decoded -= _modification_decoded_token;
			_interp->end_modification -= _end_modification_token;
			_interp->get_buffer().end_edit -= _end_edit_token;
			_interp->remove_decoration_provider(_diagnostic_decoration_token);
			_interp->remove_tooltip_provider(_hover_tooltip_token);
			_interp->remove_tooltip_provider(_diagnostic_tooltip_token);
			_interp->get_theme_providers().remove_provider(_theme_token);

			types::DidCloseTextDocumentParams params;
			params.textDocument.uri = _change_params.textDocument.uri;
			_client->send_notification(u8"textDocument/didClose", params);
		}

		/// Returns the identifier of the associated document.
		[[nodiscard]] const types::VersionedTextDocumentIdentifier &get_document_identifier() const {
			return _change_params.textDocument;
		}

		/// Returns the \ref editors::decoration_provider for diagnostics.
		[[nodiscard]] const editors::decoration_provider &get_diagnostic_decorations_readonly() const {
			return _diagnostic_decoration_token.get_readonly();
		}
		/// Returns the message for the given diagnostic.
		[[nodiscard]] std::u8string_view get_message_for_diagnostic(std::int32_t cookie) const {
			return _diagnostic_messages[cookie];
		}

		/// Returns the \ref interpretation associated with this object.
		[[nodiscard]] editors::code::interpretation &get_interpretation() const {
			return *_interp;
		}
		/// Returns the \ref client responsible for this interpretation.
		[[nodiscard]] client &get_client() const {
			return *_client;
		}

		/// Handles the \p textDocument/publishDiagnostics notification.
		static void on_publishDiagnostics(
			editors::code::interpretation&, types::PublishDiagnosticsParams,
			const editors::buffer_manager::interpretation_tag_token&
		);

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
		/// Token for the \ref editors::decoration_provider.
		editors::code::interpretation::decoration_provider_token _diagnostic_decoration_token;
		editors::code::interpretation::tooltip_provider_token
			_hover_tooltip_token, ///< Token for the tooltip provider for hover messages such as type information.
			_diagnostic_tooltip_token; ///< Token for the tooltip provider for diagnostic messages.
		editors::code::document_theme_provider_registry::token _theme_token; ///< Token for the theme provider.

		std::vector<std::u8string> _diagnostic_messages; ///< The list of diagnostic messages.

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


		/// Converts a line/column position to a character position.
		[[nodiscard]] std::size_t _position_to_character(types::Position pos) const {
			auto line_info = _interp->get_linebreaks().get_line_info(pos.line);
			return line_info.first_char + pos.character;
		}


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
