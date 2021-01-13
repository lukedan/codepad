// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/window.h"

/// \file
/// Implementation of reflection for LSP structures for window-related messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, ShowMessageParams) {
		CP_LSP_VISIT_FIELD(v, type);
		CP_LSP_VISIT_FIELD(v, message);
	}


	CP_LSP_VISIT_FUNC(v, MessageActionItemClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, additionalPropertiesSupport);
	}


	CP_LSP_VISIT_FUNC(v, ShowMessageRequestClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, messageActionItem);
	}


	CP_LSP_VISIT_FUNC(v, MessageActionItem) {
		CP_LSP_VISIT_FIELD(v, title);
	}


	CP_LSP_VISIT_FUNC(v, ShowMessageRequestParams) {
		CP_LSP_VISIT_FIELD(v, type);
		CP_LSP_VISIT_FIELD(v, message);
		CP_LSP_VISIT_FIELD(v, actions);
	}


	CP_LSP_VISIT_FUNC(v, ShowDocumentClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, support);
	}


	CP_LSP_VISIT_FUNC(v, ShowDocumentParams) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, external);
		CP_LSP_VISIT_FIELD(v, takeFocus);
		CP_LSP_VISIT_FIELD(v, selection);
	}


	CP_LSP_VISIT_FUNC(v, ShowDocumentResult) {
		CP_LSP_VISIT_FIELD(v, success);
	}


	CP_LSP_VISIT_FUNC(v, LogMessageParams) {
		CP_LSP_VISIT_FIELD(v, type);
		CP_LSP_VISIT_FIELD(v, message);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressCreateParams) {
		CP_LSP_VISIT_FIELD(v, token);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressCancelParams) {
		CP_LSP_VISIT_FIELD(v, token);
	}
}
