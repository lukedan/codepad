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
	struct TagSupportClientCapabilties : public object {
		array<CompletionItemTag> valueSet;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionItemClientCapabilities.
	struct ResolveSupportClientCapabilties : public object {
		array<string> properties;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionItemClientCapabilities.
	struct InsertTextModeSupportClientCapabilities : public object {
		array<InsertTextMode> valueSet;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref CompletionClientCapabilities.
	struct CompletionItemClientCapabilities : public object {
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
	struct CompletionItemKindClientCapabilities : public object {
		optional<array<CompletionItemKind>> valueSet;

		void visit_fields(visitor_base&) override;
	};
	struct CompletionClientCapabilities : public object {
		optional<boolean> dynamicRegistration;
		optional<CompletionItemClientCapabilities> completionItem;
		optional<CompletionItemKindClientCapabilities> completionItemKind;
		optional<boolean> contextSupport;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionOptions : public WorkDoneProgressOptions {
		optional<array<string>> triggerCharacters;
		optional<array<string>> allCommitCharacters;
		optional<boolean> resolveProvider;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionRegistrationOptions : public TextDocumentRegistrationOptions, public CompletionOptions {
		void visit_fields(visitor_base&) override;
	};

	enum class CompletionTriggerKindEnum : unsigned char {
		Invoked = 1,
		TriggerCharacter = 2,
		TriggerForIncompleteCompletions = 3
	};
	using CompletionTriggerKind = numerical_enum<CompletionTriggerKindEnum>;

	struct CompletionContext : public object {
		CompletionTriggerKind triggerKind;
		optional<string> triggerCharacter;

		void visit_fields(visitor_base&) override;
	};

	struct CompletionParams :
		public TextDocumentPositionParams, public WorkDoneProgressParams, public PartialResultParams {
		optional<CompletionContext> context;

		void visit_fields(visitor_base&) override;
	};

	enum class InsertTextFormatEnum : unsigned char {
		PlainText = 1,
		Snippet = 2
	};
	using InsertTextFormat = numerical_enum<InsertTextFormatEnum>;

	struct CompletionItem : public object {
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

	struct CompletionList : public object {
		boolean isIncomplete;
		array<CompletionItem> items;

		void visit_fields(visitor_base&) override;
	};

	struct InsertReplaceEdit : public object {
		string newText;
		Range insert;
		Range replace;

		void visit_fields(visitor_base&) override;
	};

	struct HoverClientCapabilities : public object {
		optional<boolean> dynamicRegistration;
		optional<array<MarkupKind>> contentFormat;

		void visit_fields(visitor_base&) override;
	};

	struct HoverOptions : public WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct HoverRegistrationOptions : public TextDocumentRegistrationOptions, public HoverOptions {
		void visit_fields(visitor_base&) override;
	};

	struct HoverParams : public TextDocumentPositionParams, public WorkDoneProgressParams {
		void visit_fields(visitor_base&) override;
	};

	/// Used by \ref MarkedString.
	struct MarkedStringObject : public object {
		string language;
		string value;

		void visit_fields(visitor_base&) override;
	};
	using MarkedString = primitive_variant<string, MarkedStringObject>;

	struct Hover : public object {
		/*primitive_variant<MarkedString, array<MarkedString>, MarkupContent> contents;*/
		optional<Range> range;

		void visit_fields(visitor_base&) override;
	};
}
