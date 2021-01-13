// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/language_features.h"

/// \file
/// Implementation of reflection for LSP structures for language-related messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, TagSupportClientCapabilties) {
		CP_LSP_VISIT_FIELD(v, valueSet);
	}


	CP_LSP_VISIT_FUNC(v, ResolveSupportClientCapabilties) {
		CP_LSP_VISIT_FIELD(v, properties);
	}


	CP_LSP_VISIT_FUNC(v, InsertTextModeSupportClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, valueSet);
	}


	CP_LSP_VISIT_FUNC(v, CompletionItemClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, snippetSupport);
		CP_LSP_VISIT_FIELD(v, commitCharactersSupport);
		CP_LSP_VISIT_FIELD(v, documentationFormat);
		CP_LSP_VISIT_FIELD(v, deprecatedSupport);
		CP_LSP_VISIT_FIELD(v, preselectSupport);
		CP_LSP_VISIT_FIELD(v, tagSupport);
		CP_LSP_VISIT_FIELD(v, insertReplaceSupport);
		CP_LSP_VISIT_FIELD(v, resolveSupport);
		CP_LSP_VISIT_FIELD(v, insertTextModeSupport);
	}


	CP_LSP_VISIT_FUNC(v, CompletionItemKindClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, valueSet);
	}


	CP_LSP_VISIT_FUNC(v, CompletionClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, completionItem);
		CP_LSP_VISIT_FIELD(v, completionItemKind);
		CP_LSP_VISIT_FIELD(v, contextSupport);
	}


	CP_LSP_VISIT_FUNC(v, CompletionOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
		CP_LSP_VISIT_FIELD(v, triggerCharacters);
		CP_LSP_VISIT_FIELD(v, allCommitCharacters);
		CP_LSP_VISIT_FIELD(v, resolveProvider);
	}


	CP_LSP_VISIT_FUNC(v, CompletionRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, CompletionOptions);
	}


	CP_LSP_VISIT_FUNC(v, CompletionContext) {
		CP_LSP_VISIT_FIELD(v, triggerKind);
		CP_LSP_VISIT_FIELD(v, triggerCharacter);
	}


	CP_LSP_VISIT_FUNC(v, CompletionParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, context);
	}


	CP_LSP_VISIT_FUNC(v, CompletionItem) {
		CP_LSP_VISIT_FIELD(v, label);
		CP_LSP_VISIT_FIELD(v, kind);
		CP_LSP_VISIT_FIELD(v, tags);
		CP_LSP_VISIT_FIELD(v, detail);
		CP_LSP_VISIT_FIELD(v, documentation);
		CP_LSP_VISIT_FIELD(v, deprecated);
		CP_LSP_VISIT_FIELD(v, preselect);
		CP_LSP_VISIT_FIELD(v, sortText);
		CP_LSP_VISIT_FIELD(v, filterText);
		CP_LSP_VISIT_FIELD(v, insertText);
		CP_LSP_VISIT_FIELD(v, insertTextFormat);
		CP_LSP_VISIT_FIELD(v, insertTextMode);
		// TODO textEdit
		CP_LSP_VISIT_FIELD(v, additionalTextEdits);
		CP_LSP_VISIT_FIELD(v, commitCharacters);
		CP_LSP_VISIT_FIELD(v, command);
		CP_LSP_VISIT_FIELD(v, data);
	}


	CP_LSP_VISIT_FUNC(v, CompletionList) {
		CP_LSP_VISIT_FIELD(v, isIncomplete);
		CP_LSP_VISIT_FIELD(v, items);
	}


	CP_LSP_VISIT_FUNC(v, InsertReplaceEdit) {
		CP_LSP_VISIT_FIELD(v, newText);
		CP_LSP_VISIT_FIELD(v, insert);
		CP_LSP_VISIT_FIELD(v, replace);
	}


	CP_LSP_VISIT_FUNC(v, HoverClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, contentFormat);
	}


	CP_LSP_VISIT_FUNC(v, HoverOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, HoverRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, HoverOptions);
	}


	CP_LSP_VISIT_FUNC(v, HoverParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
	}


	CP_LSP_VISIT_FUNC(v, MarkedStringObject) {
		CP_LSP_VISIT_FIELD(v, language);
		CP_LSP_VISIT_FIELD(v, value);
	}


	CP_LSP_VISIT_FUNC(v, Hover) {
		// TODO
		CP_LSP_VISIT_FIELD(v, range);
	}


	// TODO
}
