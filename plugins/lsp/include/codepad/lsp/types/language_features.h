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


	/// Used by \ref SignatureInformationClientCapabilities.
	struct ParameterInformationClientCapabilities : public virtual object {
		optional<boolean> labelOffsetSupport;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref SignatureHelpClientCapabilities.
	struct SignatureInformationClientCapabilities : public virtual object {
		optional<array<MarkupKind>> documentationFormat;
		optional<ParameterInformationClientCapabilities> parameterInformation;
		optional<boolean> activeParameterSupport;

		void visit_fields(visitor_base&) override;
	};
	struct SignatureHelpClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<SignatureInformationClientCapabilities> signatureInformation;
		optional<boolean> contextSupport;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureHelpOptions : public virtual WorkDoneProgressOptions {
		optional<array<string>> triggerCharacters;
		optional<array<string>> retriggerCharacters;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureHelpRegistrationOptions :
		public virtual TextDocumentRegistrationOptions, public virtual SignatureHelpOptions {

		void visit_fields(visitor_base&) override;
	};

	enum class SignatureHelpTriggerKindEnum : unsigned char {
		Invoked = 1,
		TriggerCharacter = 2,
		ContentChange = 3
	};
	using SignatureHelpTriggerKind = numerical_enum<SignatureHelpTriggerKindEnum>;

	struct ParameterInformation : public virtual object {
		primitive_variant<string, array<uinteger>> label;
		optional<primitive_variant<string, MarkupContent>> documentation;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureInformation : public virtual object {
		string label;
		optional<primitive_variant<string, MarkupContent>> documentation;
		optional<array<ParameterInformation>> parameters;
		optional<uinteger> activeParameter;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureHelp : public virtual object {
		array<SignatureInformation> signatures;
		optional<uinteger> activeSignature;
		optional<uinteger> activeParameter;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureHelpContext : public virtual object {
		SignatureHelpTriggerKind triggerKind;
		optional<string> triggerCharacter;
		boolean isRetrigger = false;
		optional<SignatureHelp> activeSignatureHelp;

		void visit_fields(visitor_base&) override;
	};

	struct SignatureHelpParams : public virtual TextDocumentPositionParams, public virtual WorkDoneProgressParams {
		optional<SignatureHelpContext> context;

		void visit_fields(visitor_base&) override;
	};


	struct DeclarationClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<boolean> linkSupport;

		void visit_fields(visitor_base&) override;
	};

	struct DeclarationOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct DeclarationRegistrationOptions :
		public virtual DeclarationOptions,
		public virtual TextDocumentRegistrationOptions,
		public virtual StaticRegistrationOptions {

		void visit_fields(visitor_base&) override;
	};

	struct DeclarationParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		void visit_fields(visitor_base&) override;
	};


	struct DefinitionClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<boolean> linkSupport;

		void visit_fields(visitor_base&) override;
	};

	struct DefinitionOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct DefinitionRegistrationOptions :
		public virtual TextDocumentRegistrationOptions, public virtual DefinitionOptions {

		void visit_fields(visitor_base&) override;
	};

	struct DefinitionParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		void visit_fields(visitor_base&) override;
	};


	struct TypeDefinitionClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<boolean> linkSupport;

		void visit_fields(visitor_base&) override;
	};

	struct TypeDefinitionOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct TypeDefinitionRegistrationOptions :
		public virtual TextDocumentRegistrationOptions,
		public virtual TypeDefinitionOptions,
		public virtual StaticRegistrationOptions {

		void visit_fields(visitor_base&) override;
	};

	struct TypeDefinitionParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		void visit_fields(visitor_base&) override;
	};


	struct ImplementationClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		optional<boolean> linkSupport;

		void visit_fields(visitor_base&) override;
	};

	struct ImplementationOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct ImplementationRegistrationOptions :
		public virtual TextDocumentRegistrationOptions,
		public virtual ImplementationOptions,
		public virtual StaticRegistrationOptions {

		void visit_fields(visitor_base&) override;
	};

	struct ImplementationParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		void visit_fields(visitor_base&) override;
	};


	struct ReferenceClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;

		void visit_fields(visitor_base&) override;
	};

	struct ReferenceOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct ReferenceRegistrationOptions :
		public virtual TextDocumentRegistrationOptions, public virtual ReferenceOptions {

		void visit_fields(visitor_base&) override;
	};

	struct ReferenceContext : public virtual object {
		boolean includeDeclaration = false;

		void visit_fields(visitor_base&) override;
	};

	struct ReferenceParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		ReferenceContext context;

		void visit_fields(visitor_base&) override;
	};


	struct DocumentHighlightClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;

		void visit_fields(visitor_base&) override;
	};

	struct DocumentHighlightOptions : public virtual WorkDoneProgressOptions {
		void visit_fields(visitor_base&) override;
	};

	struct DocumentHighlightRegistrationOptions :
		public virtual TextDocumentRegistrationOptions, public virtual DocumentHighlightOptions {

		void visit_fields(visitor_base&) override;
	};

	struct DocumentHighlightParams :
		public virtual TextDocumentPositionParams,
		public virtual WorkDoneProgressParams,
		public virtual PartialResultParams {

		void visit_fields(visitor_base&) override;
	};

	enum class DocumentHighlightKindEnum : unsigned char {
		Text = 1,
		Read = 2,
		Write = 3
	};
	using DocumentHighlightKind = numerical_enum<DocumentHighlightKindEnum>;

	struct DocumentHighlight : public virtual object {
		Range range;
		optional<DocumentHighlightKind> kind;

		void visit_fields(visitor_base&) override;
	};


	// TODO document symbols
	// ...
	// TODO call hierarchy outgoing


	enum class SemanticTokenTypesEnum : unsigned char {
		kNamespace,
		kType,
		kClass,
		kEnum,
		kInterface,
		kStruct,
		kTypeParameter,
		kParameter,
		kVariable,
		kProperty,
		kEnumMember,
		kEvent,
		kFunction,
		kMethod,
		kMacro,
		kKeyword,
		kModifier,
		kComment,
		kString,
		kNumber,
		kRegexp,
		kOperator
	};
	struct SemanticTokenTypes : public virtual contiguous_string_enum<SemanticTokenTypesEnum, SemanticTokenTypes> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{
				u8"namespace", u8"type", u8"class", u8"enum", u8"interface", u8"struct",
				u8"typeParameter", u8"parameter", u8"variable", u8"property", u8"enumMember",
				u8"event", u8"function", u8"method", u8"macro", u8"keyword", u8"modifier",
				u8"comment", u8"string", u8"number", u8"regexp", u8"operator"
			};
			return _strings;
		}
	};

	enum class SemanticTokenModifiersEnum : unsigned char {
		kDeclaration,
		kDefinition,
		kReadonly,
		kStatic,
		kDeprecated,
		kAbstract,
		kAsync,
		kModification,
		kDocumentation,
		kDefaultLibrary
	};
	struct SemanticTokenModifiers :
		public virtual contiguous_string_enum<SemanticTokenModifiersEnum, SemanticTokenModifiers> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{
				u8"declaration", u8"definition", u8"readonly", u8"static", u8"deprecated",
				u8"abstract", u8"async", u8"modification", u8"documentation", u8"defaultLibrary"
			};
			return _strings;
		}
	};

	enum class TokenFormatEnum : unsigned char {
		Relative
	};
	struct TokenFormat : public virtual contiguous_string_enum<TokenFormatEnum, TokenFormat> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{ u8"relative" };
			return _strings;
		}
	};

	struct SemanticTokensLegend : public virtual object {
		array<string> tokenTypes;
		array<string> tokenModifiers;

		void visit_fields(visitor_base&) override;
	};

	/// Used by \ref SemanticTokensRequestsClientCapabilities.
	struct SemanticTokensFullRequestsClientCapabilities : public virtual object {
		optional<boolean> delta;

		void visit_fields(visitor_base&) override;
	};
	/// Used by \ref SemanticTokensClientCapabilities.
	struct SemanticTokensRequestsClientCapabilities : public virtual object {
		// TODO range
		optional<primitive_variant<boolean, SemanticTokensFullRequestsClientCapabilities>> full;

		void visit_fields(visitor_base&) override;
	};
	struct SemanticTokensClientCapabilities : public virtual object {
		optional<boolean> dynamicRegistration;
		SemanticTokensRequestsClientCapabilities requests;
		array<string> tokenTypes;
		array<string> tokenModifiers;
		array<TokenFormat> formats;
		optional<boolean> overlappingTokenSupport;
		optional<boolean> multilineTokenSupport;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensOptions : public virtual WorkDoneProgressOptions {
		SemanticTokensLegend legend;
		// TODO range
		optional<primitive_variant<boolean, SemanticTokensFullRequestsClientCapabilities>> full;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensRegistrationOptions :
		public virtual TextDocumentRegistrationOptions,
		public virtual SemanticTokensOptions,
		public virtual StaticRegistrationOptions {

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensParams :
		public virtual WorkDoneProgressParams, public virtual PartialResultParams {

		TextDocumentIdentifier textDocument;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokens : public virtual object {
		optional<string> resultId;
		array<uinteger> data;

		void visit_fields(visitor_base&) override;
	};

	/// Convenience response type for \p textDocument/semanticTokens/full.
	using SemanticTokensResponse = primitive_variant<null, SemanticTokens>;

	struct SemanticTokensPartialResult : public virtual object {
		array<uinteger> data;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensDeltaParams : public virtual WorkDoneProgressParams, public virtual PartialResultParams {
		TextDocumentIdentifier textDocument;
		string previousResultId;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensEdit : public virtual object {
		uinteger start;
		uinteger deleteCount;
		optional<array<uinteger>> data;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensDelta : public virtual object {
		optional<string> resultId;
		array<SemanticTokensEdit> edits;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensDeltaPartialResult : public virtual object {
		array<SemanticTokensEdit> edits;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensRangeParams : public virtual WorkDoneProgressParams, public virtual PartialResultParams {
		TextDocumentIdentifier textDocument;
		Range range;

		void visit_fields(visitor_base&) override;
	};

	struct SemanticTokensWorkspaceClientCapabilities : public virtual object {
		optional<boolean> refreshSupport;

		void visit_fields(visitor_base&) override;
	};


	// TODO
}
