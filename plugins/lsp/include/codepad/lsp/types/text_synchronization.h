// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structures for synchronization-related LSP messages.

#include "common.h"

namespace codepad::lsp::types {
	enum class TextDocumentSyncKindEnum : unsigned char {
		None = 0,
		Full = 1,
		Incremental = 2
	};
	using TextDocumentSyncKind = numerical_enum<TextDocumentSyncKindEnum>;

	struct DidOpenTextDocumentParams : public object {
		TextDocumentItem textDocument;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentChangeRegistrationOptions : public TextDocumentRegistrationOptions {
		TextDocumentSyncKind syncKind;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentContentChangeEvent : public object {
		optional<Range> range;
		// if range is empty, rangeLength must also be empty
		optional<uinteger> rangeLength; // deprecated
		string text;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeTextDocumentParams : public object {
		VersionedTextDocumentIdentifier textDocument;
		array<TextDocumentContentChangeEvent> contentChanges;

		void visit_fields(visitor_base&) override;
	};

	enum class TextDocumentSaveReasonEnum : unsigned char {
		Manual = 1,
		AfterDelay = 2,
		FocusOut = 3
	};
	using TextDocumentSaveReason = numerical_enum<TextDocumentSaveReasonEnum>;

	struct WillSaveTextDocumentParams : public object {
		TextDocumentIdentifier textDocument;
		TextDocumentSaveReason reason;

		void visit_fields(visitor_base&) override;
	};

	struct SaveOptions : public object {
		optional<boolean> includeText;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentSaveRegistrationOptions : public TextDocumentRegistrationOptions {
		optional<boolean> includeText;

		void visit_fields(visitor_base&) override;
	};

	struct DidSaveTextDocumentParams : public object {
		TextDocumentIdentifier textDocument;
		optional<string> text;

		void visit_fields(visitor_base&) override;
	};

	struct DidCloseTextDocumentParams : public object {
		TextDocumentIdentifier textDocument;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentSyncClientCapabilities : public object {
		optional<boolean> dynamicRegistration;
		optional<boolean> willSave;
		optional<boolean> willSaveWaitUntil;
		optional<boolean> didSave;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentSyncOptions : public object {
		optional<boolean> openClose;
		optional<TextDocumentSyncKind> change;
		optional<boolean> willSave;
		optional<boolean> willSaveWaitUntil;
		optional<primitive_variant<boolean, SaveOptions>> save;

		void visit_fields(visitor_base&) override;
	};
}
