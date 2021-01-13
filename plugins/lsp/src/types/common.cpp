// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/common.h"

/// \file
/// Implementation of reflection for common LSP structures.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, RegularExpressionClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, engine);
		CP_LSP_VISIT_FIELD(v, version);
	}

	
	CP_LSP_VISIT_FUNC(v, Position) {
		CP_LSP_VISIT_FIELD(v, line);
		CP_LSP_VISIT_FIELD(v, character);
	}


	CP_LSP_VISIT_FUNC(v, Range) {
		CP_LSP_VISIT_FIELD(v, start);
		CP_LSP_VISIT_FIELD(v, end);
	}


	CP_LSP_VISIT_FUNC(v, Location) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, range);
	}


	CP_LSP_VISIT_FUNC(v, LocationLink) {
		CP_LSP_VISIT_FIELD(v, originSelectionRange);
		CP_LSP_VISIT_FIELD(v, targetUri);
		CP_LSP_VISIT_FIELD(v, targetRange);
		CP_LSP_VISIT_FIELD(v, targetSelectionRange);
	}


	CP_LSP_VISIT_FUNC(v, DiagnosticRelatedInformation) {
		CP_LSP_VISIT_FIELD(v, location);
		CP_LSP_VISIT_FIELD(v, message);
	}


	CP_LSP_VISIT_FUNC(v, CodeDescription) {
		CP_LSP_VISIT_FIELD(v, href);
	}


	CP_LSP_VISIT_FUNC(v, Diagnostic) {
		CP_LSP_VISIT_FIELD(v, range);
		CP_LSP_VISIT_FIELD(v, severity);
		CP_LSP_VISIT_FIELD(v, code);
		CP_LSP_VISIT_FIELD(v, codeDescription);
		CP_LSP_VISIT_FIELD(v, source);
		CP_LSP_VISIT_FIELD(v, message);
		CP_LSP_VISIT_FIELD(v, tags);
		CP_LSP_VISIT_FIELD(v, relatedInformation);
		CP_LSP_VISIT_FIELD(v, data);
	}


	CP_LSP_VISIT_FUNC(v, Command) {
		CP_LSP_VISIT_FIELD(v, title);
		CP_LSP_VISIT_FIELD(v, command);
		CP_LSP_VISIT_FIELD(v, arguments);
	}


	CP_LSP_VISIT_FUNC(v, TextEdit) {
		CP_LSP_VISIT_FIELD(v, range);
		CP_LSP_VISIT_FIELD(v, newText);
	}


	CP_LSP_VISIT_FUNC(v, ChangeAnnotation) {
		CP_LSP_VISIT_FIELD(v, label);
		CP_LSP_VISIT_FIELD(v, needsConfirmation);
		CP_LSP_VISIT_FIELD(v, description);
	}


	CP_LSP_VISIT_FUNC(v, AnnotatedTextEdit) {
		CP_LSP_VISIT_BASE(v, TextEdit);
		CP_LSP_VISIT_FIELD(v, annotationId);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentIdentifier) {
		CP_LSP_VISIT_FIELD(v, uri);
	}


	CP_LSP_VISIT_FUNC(v, OptionalVersionedTextDocumentIdentifier) {
		CP_LSP_VISIT_BASE(v, TextDocumentIdentifier);
		CP_LSP_VISIT_FIELD(v, version);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentEdit) {
		CP_LSP_VISIT_FIELD(v, textDocument);
	}


	CP_LSP_VISIT_FUNC(v, CreateFileOptions) {
		CP_LSP_VISIT_FIELD(v, overwrite);
		CP_LSP_VISIT_FIELD(v, ignoreIfExists);
	}


	CP_LSP_VISIT_FUNC(v, CreateFile) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, options);
		CP_LSP_VISIT_FIELD(v, annotationId);
	}


	CP_LSP_VISIT_FUNC(v, RenameFileOptions) {
		CP_LSP_VISIT_FIELD(v, overwrite);
		CP_LSP_VISIT_FIELD(v, ignoreIfExists);
	}


	CP_LSP_VISIT_FUNC(v, RenameFile) {
		CP_LSP_VISIT_FIELD(v, oldUri);
		CP_LSP_VISIT_FIELD(v, newUri);
		CP_LSP_VISIT_FIELD(v, options);
		CP_LSP_VISIT_FIELD(v, annotationId);
	}


	CP_LSP_VISIT_FUNC(v, DeleteFileOptions) {
		CP_LSP_VISIT_FIELD(v, recursive);
		CP_LSP_VISIT_FIELD(v, ignoreIfNotExists);
	}


	CP_LSP_VISIT_FUNC(v, DeleteFile) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, options);
		CP_LSP_VISIT_FIELD(v, annotationId);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceEdit) {
		CP_LSP_VISIT_FIELD(v, changes);
		CP_LSP_VISIT_FIELD(v, changeAnnotations);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceEditClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, documentChanges);
		CP_LSP_VISIT_FIELD(v, normalizesLineEndings);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentItem) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, languageId);
		CP_LSP_VISIT_FIELD(v, version);
		CP_LSP_VISIT_FIELD(v, text);
	}


	CP_LSP_VISIT_FUNC(v, VersionedTextDocumentIdentifier) {
		CP_LSP_VISIT_BASE(v, TextDocumentIdentifier);
		CP_LSP_VISIT_FIELD(v, version);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentPositionParams) {
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, position);
	}


	CP_LSP_VISIT_FUNC(v, DocumentFilter) {
		CP_LSP_VISIT_FIELD(v, language);
		CP_LSP_VISIT_FIELD(v, scheme);
		CP_LSP_VISIT_FIELD(v, pattern);
	}


	CP_LSP_VISIT_FUNC(v, StaticRegistrationOptions) {
		CP_LSP_VISIT_FIELD(v, id);
	}


	CP_LSP_VISIT_FUNC(v, TextDocumentRegistrationOptions) {
		CP_LSP_VISIT_FIELD(v, documentSelector);
	}


	CP_LSP_VISIT_FUNC(v, MarkupContent) {
		CP_LSP_VISIT_FIELD(v, kind);
		CP_LSP_VISIT_FIELD(v, value);
	}


	CP_LSP_VISIT_FUNC(v, MarkdownClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, parser);
		CP_LSP_VISIT_FIELD(v, version);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressBegin) {
		CP_LSP_VISIT_FIELD(v, title);
		CP_LSP_VISIT_FIELD(v, cancellable);
		CP_LSP_VISIT_FIELD(v, message);
		CP_LSP_VISIT_FIELD(v, percentage);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressReport) {
		CP_LSP_VISIT_FIELD(v, cancellable);
		CP_LSP_VISIT_FIELD(v, message);
		CP_LSP_VISIT_FIELD(v, percentage);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressEnd) {
		CP_LSP_VISIT_FIELD(v, message);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressParams) {
		CP_LSP_VISIT_FIELD(v, workDoneToken);
	}


	CP_LSP_VISIT_FUNC(v, WorkDoneProgressOptions) {
		CP_LSP_VISIT_FIELD(v, workDoneProgress);
	}


	CP_LSP_VISIT_FUNC(v, PartialResultParams) {
		CP_LSP_VISIT_FIELD(v, partialResultToken);
	}
}
