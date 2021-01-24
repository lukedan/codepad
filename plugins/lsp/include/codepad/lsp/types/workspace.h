// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structures for workspace-related LSP messages.

#include "common.h"

namespace codepad::lsp::types {
	struct WorkspaceFoldersServerCapabilities : public virtual object {
		optional<boolean> supported;
		optional<primitive_variant<boolean, string>> changeNotifications;

		void visit_fields(visitor_base&) override;
	};

	struct WorkspaceFolder : public virtual object {
		DocumentUri uri;
		string name;

		void visit_fields(visitor_base&) override;
	};

	struct WorkspaceFoldersChangeEvent : public virtual object {
		array<WorkspaceFolder> added;
		array<WorkspaceFolder> removed;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeWorkspaceFoldersParams : public virtual object {
		WorkspaceFoldersChangeEvent event;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeConfigurationClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeConfigurationParams : public virtual object {
		any settings;

		void visit_fields(visitor_base&) override;
	};

	struct ConfigurationItem : public virtual object {
		optional<DocumentUri> scopeUri;
		optional<string> section;

		void visit_fields(visitor_base&) override;
	};

	struct ConfigurationParams : public virtual object {
		array<ConfigurationItem> items;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeWatchedFilesClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;

		void visit_fields(visitor_base&) override;
	};

	enum class WatchKindEnum : unsigned char {
		Create = 1,
		Change = 2,
		Delete = 4
	};
	using WatchKind = numerical_enum<WatchKindEnum>;

	struct FileSystemWatcher : public virtual object {
		string globPattern;
		optional<WatchKind> kind;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeWatchedFilesRegistrationOptions : public virtual object {
		array<FileSystemWatcher> watchers;

		void visit_fields(visitor_base&) override;
	};

	enum class FileChangeTypeEnum : unsigned char {
		Created = 1,
		Changed = 2,
		Deleted = 3
	};
	using FileChangeType = numerical_enum<FileChangeTypeEnum>;

	struct FileEvent : public virtual object {
		DocumentUri uri;
		FileChangeType type;

		void visit_fields(visitor_base&) override;
	};

	struct DidChangeWatchedFilesParams : public virtual object {
		array<FileEvent> changes;

		void visit_fields(visitor_base&) override;
	};

	/// Used by \ref WorkspaceSymbolClientCapabilities.
	struct SymbolKindClientCapabilities : public virtual object {
		/*optional<array<SymbolKind>> valueSet;*/

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref WorkspaceSymbolClientCapabilities.
	struct WorkspaceSymbolTagSupportClientCapabilities : public virtual object {
		/*array<SymbolTag> valueSet;*/

		void visit_fields(visitor_base&) override;
	};
	struct WorkspaceSymbolClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<SymbolKindClientCapabilities> symbolKind;
		optional<WorkspaceSymbolTagSupportClientCapabilities> tagSupport;

		void visit_fields(visitor_base&) override;
	};

	struct WorkspaceSymbolOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct WorkspaceSymbolRegistrationOptions : public virtual WorkspaceSymbolOptions {
		void visit_fields(visitor_base&) override;
	};

	struct WorkspaceSymbolParams : public virtual WorkDoneProgressParams, public virtual PartialResultParams {
		string query;

		void visit_fields(visitor_base&) override;
	};

	struct ExecuteCommandClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;

		void visit_fields(visitor_base&) override;
	};

	struct ExecuteCommandOptions : public virtual WorkDoneProgressOptions {
		array<string> commands;

		void visit_fields(visitor_base&) override;
	};

	struct ExecuteCommandRegistrationOptions : public virtual ExecuteCommandOptions {
		void visit_fields(visitor_base&) override;
	};

	struct ExecuteCommandParams : public virtual WorkDoneProgressParams {
		string command;
		optional<array<any>> arguments;

		void visit_fields(visitor_base&) override;
	};

	struct ApplyWorkspaceEditParams : public virtual object {
		optional<string> label;
		WorkspaceEdit edit;

		void visit_fields(visitor_base&) override;
	};

	struct ApplyWorkspaceEditResponse : public virtual object {
		boolean applied = false;
		optional<string> failureReason;
		optional<uinteger> failedChange;

		void visit_fields(visitor_base&) override;
	};

	enum class FileOperationPatternKindEnum : unsigned char {
		file,
		folder
	};
	struct FileOperationPatternKind :
		public virtual contiguous_string_enum<FileOperationPatternKindEnum, FileOperationPatternKind> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{ u8"file", u8"folder" };
			return _strings;
		}
	};

	struct FileOperationPatternOptions : public virtual object {
		optional<boolean> ignoreCase;

		void visit_fields(visitor_base&) override;
	};

	struct FileOperationPattern : public virtual object {
		string glob;
		optional<FileOperationPatternKind> matches;
		optional<FileOperationPatternOptions> options;

		void visit_fields(visitor_base&) override;
	};

	struct FileOperationFilter : public virtual object {
		optional<string> scheme;
		FileOperationPattern pattern;

		void visit_fields(visitor_base&) override;
	};

	struct FileOperationRegistrationOptions : public virtual object {
		array<FileOperationFilter> filters;

		void visit_fields(visitor_base&) override;
	};

	struct FileCreate : public virtual object {
		string uri;

		void visit_fields(visitor_base&) override;
	};

	struct CreateFilesParams : public virtual object {
		array<FileCreate> files;

		void visit_fields(visitor_base&) override;
	};

	struct FileRename : public virtual object {
		string oldUri;
		string newUri;

		void visit_fields(visitor_base&) override;
	};

	struct RenameFilesParams : public virtual object {
		array<FileRename> files;

		void visit_fields(visitor_base&) override;
	};

	struct FileDelete : public virtual object {
		string uri;

		void visit_fields(visitor_base&) override;
	};

	struct DeleteFilesParams : public virtual object {
		array<FileDelete> files;

		void visit_fields(visitor_base&) override;
	};
}
