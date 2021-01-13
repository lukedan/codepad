// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/general.h"

/// \file
/// Implementation of reflection for LSP structures for general messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, NameVersion) {
		CP_LSP_VISIT_FIELD(v, name);
		CP_LSP_VISIT_FIELD(v, version);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, synchronization);
		CP_LSP_VISIT_FIELD(v, completion);
		CP_LSP_VISIT_FIELD(v, hover);
		// TODO
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceFileOperationsClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, didCreate);
		CP_LSP_VISIT_FIELD(v, willCreate);
		CP_LSP_VISIT_FIELD(v, didRename);
		CP_LSP_VISIT_FIELD(v, willRename);
		CP_LSP_VISIT_FIELD(v, didDelete);
		CP_LSP_VISIT_FIELD(v, willDelete);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, applyEdit);
		CP_LSP_VISIT_FIELD(v, workspaceEdit);
		CP_LSP_VISIT_FIELD(v, didChangeConfiguration);
		CP_LSP_VISIT_FIELD(v, didChangeWatchedFiles);
		CP_LSP_VISIT_FIELD(v, symbol);
		CP_LSP_VISIT_FIELD(v, executeCommand);
		CP_LSP_VISIT_FIELD(v, workspaceFolders);
		CP_LSP_VISIT_FIELD(v, configuration);
		// TODO
		CP_LSP_VISIT_FIELD(v, fileOperations);
	}


	CP_LSP_VISIT_FUNC(v, WindowClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, workDoneProgress);
		CP_LSP_VISIT_FIELD(v, showMessage);
		CP_LSP_VISIT_FIELD(v, showDocument);
	}


	CP_LSP_VISIT_FUNC(v, GeneralClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, regularExpressions);
		CP_LSP_VISIT_FIELD(v, markdown);
	}


	CP_LSP_VISIT_FUNC(v, ClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, workspace);
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, window);
		CP_LSP_VISIT_FIELD(v, general);
		CP_LSP_VISIT_FIELD(v, experimental);
	}


	CP_LSP_VISIT_FUNC(v, InitializeParams) {
		CP_LSP_VISIT_FIELD(v, processId);
		CP_LSP_VISIT_FIELD(v, clientInfo);
		CP_LSP_VISIT_FIELD(v, locale);
		CP_LSP_VISIT_FIELD(v, rootPath);
		CP_LSP_VISIT_FIELD(v, rootUri);
		CP_LSP_VISIT_FIELD(v, initializationOptions);
		CP_LSP_VISIT_FIELD(v, capabilities);
		CP_LSP_VISIT_FIELD(v, trace);
		CP_LSP_VISIT_FIELD(v, workspaceFolders);
	}


	CP_LSP_VISIT_FUNC(v, ServerCapabilities) {
		CP_LSP_VISIT_FIELD(v, textDocumentSync);
		CP_LSP_VISIT_FIELD(v, completionProvider);
		CP_LSP_VISIT_FIELD(v, hoverProvider);
		// TODO
	}


	CP_LSP_VISIT_FUNC(v, InitializeResult) {
		CP_LSP_VISIT_FIELD(v, capabilities);
		CP_LSP_VISIT_FIELD(v, serverInfo);
	}


	CP_LSP_VISIT_FUNC(v, InitializeError) {
		CP_LSP_VISIT_FIELD(v, retry);
	}


	CP_LSP_VISIT_FUNC(v, LogTraceParams) {
		CP_LSP_VISIT_FIELD(v, message);
		CP_LSP_VISIT_FIELD(v, verbose);
	}


	CP_LSP_VISIT_FUNC(v, SetTraceParams) {
		CP_LSP_VISIT_FIELD(v, value);
	}
}
