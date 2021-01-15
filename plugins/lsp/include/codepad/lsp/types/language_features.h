// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Structures for language-related LSP messages.

#include "common.h"

namespace codepad::lsp::types {
	enum class InsertTextModeEnum : unsigned char {
		asIs = 1,
		adjustIndentation = 2
	};
	using InsertTextMode = numerical_enum<InsertTextModeEnum>;

	enum class CompletionItemKindEnum : unsigned char {
		Text = 1,
		Method = 2,
		Function = 3,
		Constructor = 4,
		Field = 5,
		Variable = 6,
		Class = 7,
		Interface = 8,
		Module = 9,
		Property = 10,
		Unit = 11,
		Value = 12,
		Enum = 13,
		Keyword = 14,
		Snippet = 15,
		Color = 16,
		File = 17,
		Reference = 18,
		Folder = 19,
		EnumNumber = 20,
		Constant = 21,
		Struct = 22,
		Event = 23,
		Operator = 24,
		TypeParameter = 25
	};
	using CompletionItemKind = numerical_enum<CompletionItemKindEnum>;

	enum class CompletionItemTagEnum : unsigned char {
		Deprecated = 1
	};
	using CompletionItemTag = numerical_enum<CompletionItemTagEnum>;

	/// Used by \ref CompletionItemClientCapabilities.
	struct TagSupportClientCapabilties : public virtual object {
		array<CompletionItemTag> valueSet;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionItemClientCapabilities.
	struct ResolveSupportClientCapabilties : public virtual object {
		array<string> properties;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionItemClientCapabilities.
	struct InsertTextModeSupportClientCapabilities : public virtual object {
		array<InsertTextMode> valueSet;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionClientCapabilities.
	struct CompletionItemClientCapabilities : public virtual object {
		optional<boolean> snippetSupport;
		optional<boolean> commitCharactersSupport;
		optional<array<MarkupKind>> documentationFormat;
		optional<boolean> deprecatedSupport;
		optional<boolean> preselectSupport;
		optional<TagSupportClientCapabilties> tagSupport;
		optional<boolean> insertReplaceSupport;
		optional<ResolveSupportClientCapabilties> resolveSupport;
		optional<InsertTextModeSupportClientCapabilities> insertTextModeSupport;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionClientCapabilities.
	struct CompletionItemKindClientCapabilities : public virtual object {
		optional<array<CompletionItemKind>> valueSet;

		void visit_fields(visitor_base&) override;
	};
	struct CompletionClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<CompletionItemClientCapabilities> completionItem;
		optional<CompletionItemKindClientCapabilities> completionItemKind;
		optional<boolean> contextSupport;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionOptions : public virtual WorkDoneProgressOptions {
		optional<array<string>> triggerCharacters;
		optional<array<string>> allCommitCharacters;
		optional<boolean> resolveProvider;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionRegistrationOptions :
		public virtual TextDocumentRegistrationOptions, public virtual CompletionOptions {

		void visit_fields(visitor_base&) override;
	};

	enum class CompletionTriggerKindEnum : unsigned char {
		Invoked = 1,
		TriggerCharacter = 2,
		TriggerForIncompleteCompletions = 3
	};
	using CompletionTriggerKind = numerical_enum<CompletionTriggerKindEnum>;

	struct CompletionContext : public virtual object {
		CompletionTriggerKind triggerKind;
		optional<string> triggerCharacter;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		optional<CompletionContext> context;

		void visit_fields(visitor_base&) override;
	};

	enum class InsertTextFormatEnum : unsigned char {
		PlainText = 1,
		Snippet = 2
	};
	using InsertTextFormat = numerical_enum<InsertTextFormatEnum>;

	struct CompletionItem : public virtual object {
		string label;
		optional<CompletionItemKind> kind;
		optional<array<CompletionItemTag>> tags;
		optional<string> detail;
		optional<primitive_variant<string, MarkupContent>> documentation;
		optional<boolean> deprecated;
		optional<boolean> preselect;
		optional<string> sortText;
		optional<string> filterText;
		optional<string> insertText;
		optional<InsertTextFormat> insertTextFormat;
		optional<InsertTextMode> insertTextMode;
		// TODO textEdit
		optional<array<TextEdit>> additionalTextEdits;
		optional<array<string>> commitCharacters;
		optional<Command> command;
		optional<any> data;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionList : public virtual object {
		boolean isIncomplete = false;
		array<CompletionItem> items;

		void visit_fields(visitor_base&) override;
	};

	/// Convenience response type for \p textDocument/completion.
	using CompletionResponse = primitive_variant<null, array<CompletionItem>, CompletionList>;

	struct InsertReplaceEdit : public virtual object {
		string newText;
		Range insert;
		Range replace;

		void visit_fields(visitor_base&) override;
	};

	struct HoverClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<array<MarkupKind>> contentFormat;

		void visit_fields(visitor_base&) override;
	};

	struct HoverOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct HoverRegistrationOptions : public virtual TextDocumentRegistrationOptions, public virtual HoverOptions {
		void visit_fields(visitor_base&) override;
	};

	struct HoverParams : public virtual TextDocumentPositionParams, public virtual WorkDoneProgressParams {
		void visit_fields(visitor_base&) override;
	};

	/// Used by \ref MarkedString.
	struct MarkedStringObject : public virtual object {
		string language;
		string value;

		void visit_fields(visitor_base&) override;
	};
	using MarkedString = primitive_variant<string, MarkedStringObject>;

	struct Hover : public virtual object {
		/*primitive_variant<MarkedString, array<MarkedString>, MarkupContent> contents;*/
		optional<Range> range;

		void visit_fields(visitor_base&) override;
	};
}
