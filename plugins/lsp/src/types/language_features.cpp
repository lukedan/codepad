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


	CP_LSP_VISIT_FUNC(v, ParameterInformationClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, labelOffsetSupport);
	}


	CP_LSP_VISIT_FUNC(v, SignatureInformationClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, documentationFormat);
		CP_LSP_VISIT_FIELD(v, parameterInformation);
		CP_LSP_VISIT_FIELD(v, activeParameterSupport);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelpClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, signatureInformation);
		CP_LSP_VISIT_FIELD(v, contextSupport);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelpOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
		CP_LSP_VISIT_FIELD(v, triggerCharacters);
		CP_LSP_VISIT_FIELD(v, retriggerCharacters);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelpRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, SignatureHelpOptions);
	}


	CP_LSP_VISIT_FUNC(v, ParameterInformation) {
		CP_LSP_VISIT_FIELD(v, label);
		CP_LSP_VISIT_FIELD(v, documentation);
	}


	CP_LSP_VISIT_FUNC(v, SignatureInformation) {
		CP_LSP_VISIT_FIELD(v, label);
		CP_LSP_VISIT_FIELD(v, documentation);
		CP_LSP_VISIT_FIELD(v, parameters);
		CP_LSP_VISIT_FIELD(v, activeParameter);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelp) {
		CP_LSP_VISIT_FIELD(v, signatures);
		CP_LSP_VISIT_FIELD(v, activeSignature);
		CP_LSP_VISIT_FIELD(v, activeParameter);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelpContext) {
		CP_LSP_VISIT_FIELD(v, triggerKind);
		CP_LSP_VISIT_FIELD(v, triggerCharacter);
		CP_LSP_VISIT_FIELD(v, isRetrigger);
		CP_LSP_VISIT_FIELD(v, activeSignatureHelp);
	}


	CP_LSP_VISIT_FUNC(v, SignatureHelpParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_FIELD(v, context);
	}


	CP_LSP_VISIT_FUNC(v, DeclarationClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, linkSupport);
	}


	CP_LSP_VISIT_FUNC(v, DeclarationOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, DeclarationRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, DeclarationOptions);
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, StaticRegistrationOptions);
	}


	CP_LSP_VISIT_FUNC(v, DeclarationParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
	}


	CP_LSP_VISIT_FUNC(v, DefinitionClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, linkSupport);
	}


	CP_LSP_VISIT_FUNC(v, DefinitionOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, DefinitionRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, DefinitionOptions);
	}


	CP_LSP_VISIT_FUNC(v, DefinitionParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
	}


	CP_LSP_VISIT_FUNC(v, TypeDefinitionClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, linkSupport);
	}


	CP_LSP_VISIT_FUNC(v, TypeDefinitionOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, TypeDefinitionRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, TypeDefinitionOptions);
		CP_LSP_VISIT_BASE(v, StaticRegistrationOptions);
	}


	CP_LSP_VISIT_FUNC(v, TypeDefinitionParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
	}


	CP_LSP_VISIT_FUNC(v, ImplementationClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, linkSupport);
	}


	CP_LSP_VISIT_FUNC(v, ImplementationOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, ImplementationRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, ImplementationOptions);
		CP_LSP_VISIT_BASE(v, StaticRegistrationOptions);
	}


	CP_LSP_VISIT_FUNC(v, ImplementationParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
	}


	CP_LSP_VISIT_FUNC(v, ReferenceClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
	}


	CP_LSP_VISIT_FUNC(v, ReferenceOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, ReferenceRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, ReferenceOptions);
	}


	CP_LSP_VISIT_FUNC(v, ReferenceContext) {
		CP_LSP_VISIT_FIELD(v, includeDeclaration);
	}


	CP_LSP_VISIT_FUNC(v, ReferenceParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, context);
	}


	CP_LSP_VISIT_FUNC(v, DocumentHighlightClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
	}


	CP_LSP_VISIT_FUNC(v, DocumentHighlightOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, DocumentHighlightRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, DocumentHighlightOptions);
	}


	CP_LSP_VISIT_FUNC(v, DocumentHighlightParams) {
		CP_LSP_VISIT_BASE(v, TextDocumentPositionParams);
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
	}


	CP_LSP_VISIT_FUNC(v, DocumentHighlight) {
		CP_LSP_VISIT_FIELD(v, range);
		CP_LSP_VISIT_FIELD(v, kind);
	}


	// TODO


	CP_LSP_VISIT_FUNC(v, SemanticTokensLegend) {
		CP_LSP_VISIT_FIELD(v, tokenTypes);
		CP_LSP_VISIT_FIELD(v, tokenModifiers);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensFullRequestsClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, delta);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensRequestsClientCapabilities) {
		// TODO range
		CP_LSP_VISIT_FIELD(v, full);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, requests);
		CP_LSP_VISIT_FIELD(v, tokenTypes);
		CP_LSP_VISIT_FIELD(v, tokenModifiers);
		CP_LSP_VISIT_FIELD(v, formats);
		CP_LSP_VISIT_FIELD(v, overlappingTokenSupport);
		CP_LSP_VISIT_FIELD(v, multilineTokenSupport);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
		CP_LSP_VISIT_FIELD(v, legend);
		// TODO range
		CP_LSP_VISIT_FIELD(v, full);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, TextDocumentRegistrationOptions);
		CP_LSP_VISIT_BASE(v, SemanticTokensOptions);
		CP_LSP_VISIT_BASE(v, StaticRegistrationOptions);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensParams) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, textDocument);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokens) {
		CP_LSP_VISIT_FIELD(v, resultId);
		CP_LSP_VISIT_FIELD(v, data);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensPartialResult) {
		CP_LSP_VISIT_FIELD(v, data);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensDeltaParams) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, previousResultId);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensEdit) {
		CP_LSP_VISIT_FIELD(v, start);
		CP_LSP_VISIT_FIELD(v, deleteCount);
		CP_LSP_VISIT_FIELD(v, data);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensDelta) {
		CP_LSP_VISIT_FIELD(v, resultId);
		CP_LSP_VISIT_FIELD(v, edits);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensDeltaPartialResult) {
		CP_LSP_VISIT_FIELD(v, edits);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensRangeParams) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, textDocument);
		CP_LSP_VISIT_FIELD(v, range);
	}


	CP_LSP_VISIT_FUNC(v, SemanticTokensWorkspaceClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, refreshSupport);
	}
}
