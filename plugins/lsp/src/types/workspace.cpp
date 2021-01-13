// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/lsp/types/workspace.h"

/// \file
/// Implementation of reflection for LSP structures for workspace-related messages.

#include "private.h"

namespace codepad::lsp::types {
	CP_LSP_VISIT_FUNC(v, WorkspaceFoldersServerCapabilities) {
		CP_LSP_VISIT_FIELD(v, supported);
		CP_LSP_VISIT_FIELD(v, changeNotifications);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceFolder) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, name);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceFoldersChangeEvent) {
		CP_LSP_VISIT_FIELD(v, added);
		CP_LSP_VISIT_FIELD(v, removed);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeWorkspaceFoldersParams) {
		CP_LSP_VISIT_FIELD(v, event);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeConfigurationClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeConfigurationParams) {
		CP_LSP_VISIT_FIELD(v, settings);
	}


	CP_LSP_VISIT_FUNC(v, ConfigurationItem) {
		CP_LSP_VISIT_FIELD(v, scopeUri);
		CP_LSP_VISIT_FIELD(v, section);
	}


	CP_LSP_VISIT_FUNC(v, ConfigurationParams) {
		CP_LSP_VISIT_FIELD(v, items);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeWatchedFilesClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
	}


	CP_LSP_VISIT_FUNC(v, FileSystemWatcher) {
		CP_LSP_VISIT_FIELD(v, globPattern);
		CP_LSP_VISIT_FIELD(v, kind);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeWatchedFilesRegistrationOptions) {
		CP_LSP_VISIT_FIELD(v, watchers);
	}


	CP_LSP_VISIT_FUNC(v, FileEvent) {
		CP_LSP_VISIT_FIELD(v, uri);
		CP_LSP_VISIT_FIELD(v, type);
	}


	CP_LSP_VISIT_FUNC(v, DidChangeWatchedFilesParams) {
		CP_LSP_VISIT_FIELD(v, changes);
	}


	CP_LSP_VISIT_FUNC(v, SymbolKindClientCapabilities) {
		// TODO
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceSymbolTagSupportClientCapabilities) {
		// TODO
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceSymbolClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
		CP_LSP_VISIT_FIELD(v, symbolKind);
		CP_LSP_VISIT_FIELD(v, tagSupport);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceSymbolOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceSymbolRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, WorkspaceSymbolOptions);
	}


	CP_LSP_VISIT_FUNC(v, WorkspaceSymbolParams) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_BASE(v, PartialResultParams);
		CP_LSP_VISIT_FIELD(v, query);
	}


	CP_LSP_VISIT_FUNC(v, ExecuteCommandClientCapabilities) {
		CP_LSP_VISIT_FIELD(v, dynamicRegistration);
	}


	CP_LSP_VISIT_FUNC(v, ExecuteCommandOptions) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressOptions);
		CP_LSP_VISIT_FIELD(v, commands);
	}


	CP_LSP_VISIT_FUNC(v, ExecuteCommandRegistrationOptions) {
		CP_LSP_VISIT_BASE(v, ExecuteCommandOptions);
	}


	CP_LSP_VISIT_FUNC(v, ExecuteCommandParams) {
		CP_LSP_VISIT_BASE(v, WorkDoneProgressParams);
		CP_LSP_VISIT_FIELD(v, command);
		CP_LSP_VISIT_FIELD(v, arguments);
	}


	CP_LSP_VISIT_FUNC(v, ApplyWorkspaceEditParams) {
		CP_LSP_VISIT_FIELD(v, label);
		CP_LSP_VISIT_FIELD(v, edit);
	}


	CP_LSP_VISIT_FUNC(v, ApplyWorkspaceEditResponse) {
		CP_LSP_VISIT_FIELD(v, applied);
		CP_LSP_VISIT_FIELD(v, failureReason);
		CP_LSP_VISIT_FIELD(v, failedChange);
	}


	CP_LSP_VISIT_FUNC(v, FileOperationPatternOptions) {
		CP_LSP_VISIT_FIELD(v, ignoreCase);
	}


	CP_LSP_VISIT_FUNC(v, FileOperationPattern) {
		CP_LSP_VISIT_FIELD(v, glob);
		CP_LSP_VISIT_FIELD(v, matches);
		CP_LSP_VISIT_FIELD(v, options);
	}


	CP_LSP_VISIT_FUNC(v, FileOperationFilter) {
		CP_LSP_VISIT_FIELD(v, scheme);
		CP_LSP_VISIT_FIELD(v, pattern);
	}


	CP_LSP_VISIT_FUNC(v, FileOperationRegistrationOptions) {
		CP_LSP_VISIT_FIELD(v, filters);
	}


	CP_LSP_VISIT_FUNC(v, FileCreate) {
		CP_LSP_VISIT_FIELD(v, uri);
	}


	CP_LSP_VISIT_FUNC(v, CreateFilesParams) {
		CP_LSP_VISIT_FIELD(v, files);
	}


	CP_LSP_VISIT_FUNC(v, FileRename) {
		CP_LSP_VISIT_FIELD(v, oldUri);
		CP_LSP_VISIT_FIELD(v, newUri);
	}


	CP_LSP_VISIT_FUNC(v, RenameFilesParams) {
		CP_LSP_VISIT_FIELD(v, files);
	}


	CP_LSP_VISIT_FUNC(v, FileDelete) {
		CP_LSP_VISIT_FIELD(v, uri);
	}


	CP_LSP_VISIT_FUNC(v, DeleteFilesParams) {
		CP_LSP_VISIT_FIELD(v, files);
	}
}
