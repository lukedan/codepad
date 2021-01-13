// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/text_synchronization.h"

/// \file
/// Implementation of reflection for LSP structures for synchronization-related messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, DidOpenTextDocumentParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentChangeRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_FIELD(v, syncKind);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentContentChangeEvent) {
		CP_LSP_VISIT_FIELD(v, range);
		CP_LSP_VISIT_FIELD(v, rangeLength);
		CP_LSP_VISIT_FIELD(v, text);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeTextDocumentParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, contentChanges);
	}


	CP_LSP_VISIT_FUNC(v, WillSaveTextDocumentParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, reason);
	}


	CP_LSP_VISIT_FUNC(v, SaveOptions) {
		CP_LSP_VISIT_FIELD(v, includeText);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentSaveRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_FIELD(v, includeText);
	}


	CP_LSP_VISIT_FUNC(v, DidSaveTextDocumentParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, text);
	}


	CP_LSP_VISIT_FUNC(v, DidCloseTextDocumentParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentSyncClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, willSave);
		CP_LSP_VISIT_FIELD(v, willSaveWaitUntil);
		CP_LSP_VISIT_FIELD(v, didSave);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentSyncOptions) {
		CP_LSP_VISIT_FIELD(v, openClose);
		CP_LSP_VISIT_FIELD(v, change);
		CP_LSP_VISIT_FIELD(v, willSave);
		CP_LSP_VISIT_FIELD(v, willSaveWaitUntil);
		CP_LSP_VISIT_FIELD(v, save);
	}
}
