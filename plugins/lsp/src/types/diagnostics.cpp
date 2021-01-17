// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/diagnostics.h"

/// \file
/// Implementation of reflection for LSP structures for diagnostics-related messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, PublishDiagnosticsTagSupportClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, valueSet);
	}


	CP_LSP_VISIT_FUNC(v, PublishDiagnosticsClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, relatedInformation);
		CP_LSP_VISIT_FIELD(v, tagSupport);
		CP_LSP_VISIT_FIELD(v, versionSupport);
		CP_LSP_VISIT_FIELD(v, codeDescriptionSupport);
		CP_LSP_VISIT_FIELD(v, dataSupport);
	}


	CP_LSP_VISIT_FUNC(v, PublishDiagnosticsParams) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, version);
		CP_LSP_VISIT_FIELD(v, diagnostics);
	}
}
