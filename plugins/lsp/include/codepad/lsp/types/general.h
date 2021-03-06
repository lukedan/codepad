// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// LSP structures for general messages.

#include "common.h"
#include "window.h"
#include "workspace.h"
#include "text_synchronization.h"
#include "language_features.h"

namespace codepad::lsp::types {
	/// Used by \ref InitializeParams and \ref InitializeResult.
	struct NameVersion : public virtual object {
		string name;
		optional<string> version;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentClientCapabilities : public virtual object {
		optional<TextDocumentSyncClientCapabilities> synchronization;
		optional<CompletionClientCapabilities> completion;
		optional<HoverClientCapabilities> hover;
		optional<SignatureHelpClientCapabilities> signatureHelp;
		optional<DeclarationClientCapabilities> declaration;
		optional<DefinitionClientCapabilities> definition;
		optional<TypeDefinitionClientCapabilities> typeDefinition;
		/*optional<ImplementationClientCapabilities> implementation;
		optional<ReferenceClientCapabilities> references;
		optional<DocumentHighlightClientCapabilities> documentHighlight;
		optional<DocumentSymbolClientCapabilities> documentSymbol;
		optional<CodeActionClientCapabilities> codeAction;
		optional<CodeLensClientCapabilities> codeLens;
		optional<DocumentLinkClientCapabilities> documentLink;
		optional<DocumentColorClientCapabilities> colorProvider;
		optional<DocumentFormattingClientCapabilities> formatting;
		optional<DocumentRangeFormattingClientCapabilities> rangeFormatting;
		optional<DocumentOnTypeFormattingClientCapabilities> onTypeFormatting;
		optional<RenameClientCapabilities> rename;
		optional<PublishDiagnosticsClientCapabilities> publishDiagnostics;
		optional<FoldingRangeClientCapabilities> foldingRange;
		optional<SelectionRangeClientCapabilities> selectionRange;
		optional<LinkedEditingRangeClientCapabilities> linkedEditingRange;
		optional<CallHierarchyClientCapabilities> callHierarchy;*/
		optional<SemanticTokensClientCapabilities> semanticTokens;
		/*optional<MonikerClientCapabilities> moniker;*/

		void visit_fields(visitor_base&) override;
	};

	/// Used by \ref WorkspaceClientCapabilities.
	struct WorkspaceFileOperationsClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<boolean> didCreate;
		optional<boolean> willCreate;
		optional<boolean> didRename;
		optional<boolean> willRename;
		optional<boolean> didDelete;
		optional<boolean> willDelete;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref ClientCapabilities.
	struct WorkspaceClientCapabilities : public virtual object {
		optional<boolean> applyEdit;
		optional<WorkspaceEditClientCapabilities> workspaceEdit;
		optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration;
		optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles;
		optional<WorkspaceSymbolClientCapabilities> symbol;
		optional<ExecuteCommandClientCapabilities> executeCommand;
		optional<boolean> workspaceFolders;
		optional<boolean> configuration;
		optional<SemanticTokensWorkspaceClientCapabilities> semanticTokens;
		/*optional<CodeLensWorkspaceClientCapabilities> codeLens;*/
		optional<WorkspaceFileOperationsClientCapabilities> fileOperations;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref ClientCapabilities.
	struct WindowClientCapabilities : public virtual object {
		optional<boolean> workDoneProgress;
		optional<ShowMessageRequestClientCapabilities> showMessage;
		optional<ShowDocumentClientCapabilities> showDocument;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref ClientCapabilities.
	struct GeneralClientCapabilities : public virtual object {
		optional<RegularExpressionClientCapabilities> regularExpressions;
		optional<MarkdownClientCapabilities> markdown;

		void visit_fields(visitor_base&) override;
	};
	struct ClientCapabilities : public virtual object {
		optional<WorkspaceClientCapabilities> workspace;
		optional<TextDocumentClientCapabilities> textDocument;
		optional<WindowClientCapabilities> window;
		optional<GeneralClientCapabilities> general;
		optional<any> experimental;

		// clangd extension
		optional<array<string>> offsetEncoding;

		void visit_fields(visitor_base&) override;
	};

	struct InitializeParams : public virtual WorkDoneProgressParams {
		primitive_variant<null, integer> processId;
		optional<NameVersion> clientInfo;
		optional<string> locale;
		optional<primitive_variant<null, string>> rootPath;
		primitive_variant<null, DocumentUri> rootUri;
		optional<any> initializationOptions;
		ClientCapabilities capabilities;
		optional<TraceValue> trace;
		optional<primitive_variant<null, array<WorkspaceFolder>>> workspaceFolders;

		void visit_fields(visitor_base&) override;
	};

	struct ServerCapabilities : public virtual object {
		optional<primitive_variant<TextDocumentSyncKind, TextDocumentSyncOptions>> textDocumentSync;
		optional<CompletionOptions> completionProvider;
		optional<primitive_variant<boolean, HoverOptions>> hoverProvider;
		optional<SignatureHelpOptions> signatureHelpProvider;
		// TODO declarationProvider
		optional<primitive_variant<boolean, DefinitionOptions>> definitionProvider;
		// TODO typeDefinitionProvider
		// TODO implementationProvider
		optional<primitive_variant<boolean, ReferenceOptions>> referencesProvider;
		optional<primitive_variant<boolean, DocumentHighlightOptions>> documentHighlightProvider;
		/*optional<primitive_variant<boolean, DocumentSymbolOptions>> documentSymbolProvider;*/
		// TODO codeActionProvider
		// ...
		// TODO callHierarchyProvider
		optional<TextDocumentRegistrationOptions_variant<
			SemanticTokensOptions, SemanticTokensRegistrationOptions
		>> semanticTokensProvider;

		void visit_fields(visitor_base&) override;
	};

	struct InitializeResult : public virtual object {
		ServerCapabilities capabilities;
		optional<NameVersion> serverInfo;

		// clangd extension
		optional<string> offsetEncoding;

		void visit_fields(visitor_base&) override;
	};

	struct InitializeError : public virtual object {
		boolean retry = false;

		void visit_fields(visitor_base&) override;
	};

	struct InitializedParams : public virtual object {
		void visit_fields(visitor_base&) override;
	};

	struct LogTraceParams : public virtual object {
		string message;
		optional<string> verbose;

		void visit_fields(visitor_base&) override;
	};

	struct SetTraceParams : public virtual object {
		TraceValue value;

		void visit_fields(visitor_base&) override;
	};
}
