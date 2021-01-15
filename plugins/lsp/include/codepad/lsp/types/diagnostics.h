// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structures for diagnostics-related LSP messages.

#include "common.h"

namespace codepad::lsp::types {
	/// Used by \ref PublishDiagnosticsClientCapabilities.
	struct PublishDiagnosticsTagSupportClientCapabilities : public virtual object {
		array<DiagnosticTag> valueSet;

		void visit_fields(visitor_base&) override;
	};
	struct PublishDiagnosticsClientCapabilities : public virtual object {
		optional<boolean> relatedInformation;
		optional<PublishDiagnosticsTagSupportClientCapabilities> tagSupport;
		optional<boolean> versionSupport;
		optional<boolean> codeDescriptionSupport;
		optional<boolean> dataSupport;

		void visit_fields(visitor_base&) override;
	};

	struct PublishDiagnosticsParams : public virtual object {
		DocumentUri uri;
		optional<uinteger> version;
		array<Diagnostic> diagnostics;

		void visit_fields(visitor_base&) override;
	};
}
