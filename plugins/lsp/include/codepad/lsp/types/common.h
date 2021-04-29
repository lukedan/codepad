// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Basic LSP structures.

#include <string>
#include <optional>
#include <variant>
#include <any>

#include "codepad/core/json/misc.h"
#include "codepad/core/json/storage.h"
#include "codepad/core/json/default_engine.h"
#include "codepad/core/misc.h"

/// Typedefs and structs in the LSP specification. These are not documented. These structs may inherit from one
/// another other but no virtual destructors are defined, so they should not be mixed with their parent structs.
namespace codepad::lsp::types {
	namespace _details {
		/// Returns the index of the specified type in the given \p std::tuple. `Equality' is tested using
		/// \p is_base_of, so using base classes work too.
		template <
			typename T, typename Tuple, std::size_t I = 0
		> [[nodiscard]] inline constexpr std::optional<std::size_t> get_index_in_tuple() {
			if constexpr (I >= std::tuple_size_v<Tuple>) {
				return std::nullopt;
			} else {
				constexpr std::optional<std::size_t> _next_id = get_index_in_tuple<T, Tuple, I + 1>();
				using _cur_type = std::tuple_element_t<I, Tuple>;
				if constexpr (std::is_same_v<T, _cur_type> || std::is_base_of_v<T, _cur_type>) {
					static_assert(!_next_id.has_value(), "duplicate types in variant");
					return I;
				} else {
					return _next_id;
				}
			}
		}
	}


	using null = codepad::json::null_t;
	using boolean = bool;
	using integer = std::int32_t;
	using uinteger = std::uint32_t;
	using decimal = double;
	using string = std::u8string;
	using any = json::value_storage;


	class visitor_base;

	/// Base class of compound objects that correspond to JSON objects.
	struct object {
		/// Default virtual destructor.
		virtual ~object() = default;

		/// Invokes \ref visitor_base::visit() for all fields of this object.
		virtual void visit_fields(visitor_base&) = 0;
	};
	/// Type-erased base type of enums that are serialized and deserialized as integers.
	struct numerical_enum_base {
		/// Default virtual destructor.
		virtual ~numerical_enum_base() = default;

		/// Returns the enum value as an integer.
		[[nodiscard]] virtual integer get_value() const = 0;
		/// Sets the enum value.
		virtual void set_value(integer) = 0;
	};
	/// Type-erased base type of enums that are serialized and deserialized as strings.
	struct string_enum_base {
		/// Default virtual destructor.
		virtual ~string_enum_base() = default;

		/// Returns the enum value as a string.
		[[nodiscard]] virtual std::u8string_view get_value() const = 0;
		/// Sets the enum value.
		virtual void set_value(std::u8string_view) = 0;
	};
	/// Type-erased base class of optional objects.
	struct optional_base {
		/// Default virtual destructor.
		virtual ~optional_base() = default;

		/// Returns whether this object has an value.
		[[nodiscard]] virtual bool has_value() const = 0;
		/// Puts a value into this object.
		virtual void emplace_value() = 0;
		/// Clears the value.
		virtual void clear_value() = 0;
		/// Visits the underlying value. This object must not be empty when this is called.
		virtual void visit_value(visitor_base&) = 0;
	};
	/// Type-erased base class of arrays.
	struct array_base {
		/// Default virtual destructor.
		virtual ~array_base() = default;

		/// Returns the length of this array.
		[[nodiscard]] virtual std::size_t get_length() const = 0;
		/// Sets the length of this array.
		virtual void set_length(std::size_t) = 0;
		/// Visits the elements of this array.
		virtual void visit_element_at(std::size_t, visitor_base&) = 0;
	};
	/// Type-erased base class of variants that only contain primitives and thus can be fully distinguished simply by
	/// the type of the value.
	struct primitive_variant_base {
		/// Default virtual destructor.
		virtual ~primitive_variant_base() = default;

		/// Sets this variant to a \ref null.
		virtual void set_null() = 0;
		/// Sets this variant to a \ref boolean.
		virtual void set_boolean(boolean) = 0;
		/// Sets this variant to a \p std::int64_t. Since it's impossible to distinguish between \ref integer and
		/// \ref uinteger, this function handles both.
		virtual void set_int64(std::int64_t) = 0;
		/// Sets this variant to a \ref decimal.
		virtual void set_decimal(decimal) = 0;
		/// Sets this variant to a \ref string.
		virtual void set_string(std::u8string_view) = 0;
		/// Sets this variant to an \ref array_base, and invokes the corresponding \p visit() function.
		virtual void set_array_and_visit(visitor_base&) = 0;
		/// Sets this variant to an \ref object, and invokes the corresponding \p visit() function.
		virtual void set_object_and_visit(visitor_base&) = 0;

		/// Visits the currently active value.
		virtual void visit_value(visitor_base&) = 0;
	};
	/// A custom variant type.
	struct custom_variant_base {
		/// Default virtual destructor.
		virtual ~custom_variant_base() = default;

		/// Deduces the type of this variant and visits the value.
		virtual void deduce_type_and_visit(visitor_base&, const json::value_t&) = 0;
		/// Visits the currently active value.
		virtual void visit_value(visitor_base&) = 0;
	};
	/// Type-erased base class of mappings.
	struct map_base {
		/// Default virtual destructor.
		virtual ~map_base() = default;

		/// Inserts an entry with the given key into this mapping, and invokes \p visit() on the value.
		virtual void insert_visit_entry(visitor_base&, std::u8string_view) = 0;
		/// Visits all entries using \p visit_field().
		virtual void visit_entries(visitor_base&) = 0;
	};


	/// Used to visit a struct field by field for serialization.
	class visitor_base {
	public:
		/// Default virtual destructor.
		virtual ~visitor_base() = default;

		/// Visits a \ref null object.
		virtual void visit(null&) = 0;
		/// Visits a \ref boolean.
		virtual void visit(boolean&) = 0;
		/// Visits a \ref integer.
		virtual void visit(integer&) = 0;
		/// Visits a \ref uinteger.
		virtual void visit(uinteger&) = 0;
		/// Visits a \ref decimal.
		virtual void visit(decimal&) = 0;
		/// Visits a \ref string.
		virtual void visit(string&) = 0;
		/// Visits a \ref any.
		virtual void visit(any&) = 0;

		/// Visits a \ref object.
		virtual void visit(object&) = 0;
		/// Visits a \ref numerical_enum_base.
		virtual void visit(numerical_enum_base&) = 0;
		/// Visits a \ref string_enum_base.
		virtual void visit(string_enum_base&) = 0;
		/// Visits an \ref array_base.
		virtual void visit(array_base&) = 0;
		/// Visits a \ref primitive_variant_base.
		virtual void visit(primitive_variant_base&) = 0;
		/// Visits a \ref custom_variant_base.
		virtual void visit(custom_variant_base&) = 0;
		/// Visits a \ref map_base.
		virtual void visit(map_base&) = 0;


		/// Visits a field that is optional.
		virtual void visit_field(std::u8string_view, optional_base&) = 0;
		/// For fields of all other types, simply inovke normal \ref visit().
		template <typename T> std::enable_if_t<!std::is_base_of_v<optional_base, T>> visit_field(
			std::u8string_view name, T &opt
		) {
			_start_field(name);
			visit(opt);
			_end_field();
		}
	protected:
		/// Starts visiting a field.
		virtual void _start_field(std::u8string_view) = 0;
		/// End visiting a field.
		virtual void _end_field() = 0;
	};


	/// Enums that are serialized as integers.
	template <typename T> struct numerical_enum : public numerical_enum_base {
		T value{}; ///< The value of this enum.

		/// Returns \ref value as an integer.
		integer get_value() const override {
			return static_cast<integer>(value);
		}
		/// Sets \ref value.
		void set_value(integer i) override {
			value = static_cast<T>(i);
		}
	};
	/// An enum type represented as strings. The enum values are continuous and starts from 0, so the enum can be
	/// converted to and from strings using only a list of strings. This class uses CRTP and derived classes must
	/// implement \p get_strings() that returns a <tt>const std::vector&</tt> of \p std::u8string_view.
	template <typename Enum, typename Derived> struct contiguous_string_enum : public string_enum_base {
	public:
		Enum value; ///< The underlying enum value.

		/// Returns the value of this enum.
		std::u8string_view get_value() const override {
			return Derived::get_strings()[static_cast<std::size_t>(value)];
		}
		/// Sets \ref value using the result of \ref _get_mapping().
		void set_value(std::u8string_view str) override {
			value = static_cast<Enum>(_get_mapping()[str]);
		}
	protected:
		/// Returns the mapping from \p std::u8string_view to \p int.
		static std::unordered_map<std::u8string_view, std::size_t> _get_mapping() {
			static std::unordered_map<std::u8string_view, std::size_t> _mapping;
			if (_mapping.empty()) {
				auto &strings = Derived::get_strings();
				for (std::size_t i = 0; i < strings.size(); ++i) {
					_mapping.emplace(strings[i], i);
				}
			}

			return _mapping;
		}
	};
	/// Optional objects and fields.
	template <typename T> struct optional : public optional_base {
		std::optional<T> value; ///< The value.

		/// Returns the result of \p std::optional::has_value().
		bool has_value() const override {
			return value.has_value();
		}
		/// Invokes \p std::optional::emplace() with no arguments.
		void emplace_value() override {
			value.emplace();
		}
		/// Invokes \p std::optional::reset().
		void clear_value() override {
			value.reset();
		}
		/// Visits the value. \ref value must not be empty.
		void visit_value(visitor_base &v) override {
			v.visit(value.value());
		}
	};
	/// Arrays.
	template <typename T> struct array : public array_base {
		std::vector<T> value; ///< The array.

		/// Returns the length of \ref value.
		std::size_t get_length() const override {
			return value.size();
		}
		/// Resizes \ref value.
		void set_length(std::size_t len) override {
			value.resize(len);
		}

		/// Visit all elements of the array.
		void visit_element_at(std::size_t i, visitor_base &v) override {
			v.visit(value[i]);
		}
	};
	/// A variant with only primitive types.
	template <typename ...Args> struct primitive_variant : public primitive_variant_base {
		std::variant<Args...> value; ///< The value of this variant.

		/// Sets this variant to a \ref null.
		void set_null() override {
			constexpr auto _id = _details::get_index_in_tuple<null, std::tuple<Args...>>();
			if constexpr (_id) {
				value.template emplace<_id.value()>();
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain a null";
			}
		}
		/// Sets this variant to a \ref boolean.
		void set_boolean(boolean b) override {
			constexpr auto _id = _details::get_index_in_tuple<boolean, std::tuple<Args...>>();
			if constexpr (_id) {
				value.template emplace<_id.value()>(b);
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain a boolean";
			}
		}
		/// Sets this variant to a \ref integer.
		void set_int64(std::int64_t i) override {
			constexpr auto
				_id_int = _details::get_index_in_tuple<integer, std::tuple<Args...>>(),
				_id_uint = _details::get_index_in_tuple<uinteger, std::tuple<Args...>>(),
				_id_enum = _details::get_index_in_tuple<numerical_enum_base, std::tuple<Args...>>();
			constexpr std::size_t _count = (_id_int ? 1 : 0) + (_id_uint ? 1 : 0) + (_id_enum ? 1 : 0);

			if constexpr (_count == 1) {
				if constexpr (_id_enum) {
					value.template emplace<_id_enum.value()>().set_value(static_cast<integer>(i));
				} else if constexpr (_id_int) {
					if (
						i < static_cast<std::int64_t>(std::numeric_limits<integer>::min()) ||
						i > static_cast<std::int64_t>(std::numeric_limits<integer>::max())
					) {
						logger::get().log_error(CP_HERE) << "value out of range of int32";
					} else {
						value.template emplace<_id_int.value()>(static_cast<integer>(i));
					}
				} else {
					if (i < 0 || i > static_cast<std::int64_t>(std::numeric_limits<uinteger>::max())) {
						logger::get().log_error(CP_HERE) << "value out of range of uint32";
					} else {
						value.template emplace<_id_uint.value()>(static_cast<uinteger>(i));
					}
				}
			} else if constexpr (_count > 1) {
				logger::get().log_error(CP_HERE) << "variant contains multiple integral types or enums";
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain any integral types or enums";
			}
		}
		/// Sets this variant to a \ref decimal.
		void set_decimal(decimal d) override {
			constexpr auto _id = _details::get_index_in_tuple<decimal, std::tuple<Args...>>();
			if constexpr (_id) {
				value.emplace<_id.value()>(d);
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain a decimal";
			}
		}
		/// Sets this variant to a \ref string.
		void set_string(std::u8string_view s) override {
			constexpr auto _id_str = _details::get_index_in_tuple<string, std::tuple<Args...>>();
			constexpr auto _id_enum = _details::get_index_in_tuple<string_enum_base, std::tuple<Args...>>();
			constexpr std::size_t _count = (_id_str ? 1 : 0) + (_id_enum ? 1 : 0);

			if constexpr (_count == 1) {
				if constexpr (_id_str) {
					value.template emplace<_id_str.value()>(s);
				} else {
					value.template emplace<_id_enum.value()>().set_value(s);
				}
			} else if constexpr (_count > 1) {
				logger::get().log_error(CP_HERE) << "variant contains multiple string types or enums";
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain any string types or enums";
			}
		}
		/// Sets this variant to an \ref array_base.
		void set_array_and_visit(visitor_base &vis) override {
			constexpr auto _id = _details::get_index_in_tuple<array_base, std::tuple<Args...>>();
			if constexpr (_id) {
				vis.visit(value.template emplace<_id.value()>());
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain an array type";
			}
		}
		/// Sets this variant to an \ref object or a \ref map_base.
		void set_object_and_visit(visitor_base &vis) override {
			constexpr auto _id_obj = _details::get_index_in_tuple<object, std::tuple<Args...>>();
			constexpr auto _id_map = _details::get_index_in_tuple<map_base, std::tuple<Args...>>();
			constexpr std::size_t _count = (_id_obj ? 1 : 0) + (_id_map ? 1 : 0);

			if constexpr (_count == 1) {
				if constexpr (_id_obj) {
					vis.visit(value.template emplace<_id_obj.value()>());
				} else {
					vis.visit(value.template emplace<_id_map.value()>());
				}
			} else if constexpr (_count > 1) {
				logger::get().log_error(CP_HERE) << "variant contains multiple object types or enums";
			} else {
				logger::get().log_error(CP_HERE) << "variant does not contain any object types or enums";
			}
		}

		/// Visits \ref value using \p std::visit().
		void visit_value(visitor_base &v) override {
			std::visit(
				[&v](auto &val) {
					v.visit(val);
				}, value
			);
		}
	};
	/// Maps.
	template <typename Value> struct map : public map_base {
		std::unordered_map<string, Value> entries; ///< The entries.

		/// Inserts and visits a new entry. A entry with the same name must not exist before the call.
		void insert_visit_entry(visitor_base &v, std::u8string_view key) override {
			auto [it, inserted] = entries.emplace(key, Value());
			assert_true_logical(!inserted, "duplicate entries in map");
			v.visit(it->second);
		}
		/// Invokes \p visit_field() for  all entries.
		void visit_entries(visitor_base &v) override {
			for (auto it = entries.begin(); it != entries.end(); ++it) {
				v.visit_field(it->first, it->second);
			}
		}
	};


	enum class ErrorCodesEnum : integer {
		ParseError = -32700,
		InvalidRequest = -32600,
		MethodNotFound = -32601,
		InvalidParams = -32602,
		InternalError = -32603,

		ServerNotInitialized = -32099,
		UnknownError = -32001,

		ContentModified = -32801,
		RequestCancelled = -32800
	};
	using ErrorCodes = numerical_enum<ErrorCodesEnum>;

	/// Ranges of reserved error codes.
	namespace reserved_errors {
		/// Error codes reserved by JSON-RPC.
		namespace jsonrpc {
			constexpr ErrorCodesEnum
				start = static_cast<ErrorCodesEnum>(-32099), ///< Start of the range of error codes reserved by JSON-RPC.
				end = static_cast<ErrorCodesEnum>(-32000); ///< End of the range of error codes reserved by JSON-RPC.
		}
		/// Error codes reserved by LSP.
		namespace lsp {
			constexpr ErrorCodesEnum
				start = static_cast<ErrorCodesEnum>(-32899), ///< Start of the range of error codes reserved by LSP.
				end = static_cast<ErrorCodesEnum>(-32800); ///< End of the range of error codes reserved by LSP.
		}
	}

	using DocumentUri = string;
	using URI = string;

	using ProgressToken = primitive_variant<integer, string>;
	template <typename T> struct ProgressParams : public virtual object {
		ProgressToken token;
		T value;

		void visit_fields(visitor_base &v) override {
			v.visit_field(u8"token", token);
			v.visit_field(u8"value", value);
		}
	};

	struct RegularExpressionClientCapabilities : public virtual object {
		string engine;
		optional<string> version;

		void visit_fields(visitor_base&) override;
	};

	struct Position : public virtual object {
		/// Default constructor.
		Position() = default;
		/// Convenience constructor.
		Position(std::size_t li, std::size_t ch) :
			object(), line(static_cast<uinteger>(li)), character(static_cast<uinteger>(ch)) {
		}

		uinteger line = 0;
		uinteger character = 0;

		void visit_fields(visitor_base&) override;
	};

	struct Range : public virtual object {
		Position start;
		Position end;

		void visit_fields(visitor_base&) override;
	};

	struct Location : public virtual object {
		DocumentUri uri;
		Range range;

		void visit_fields(visitor_base&) override;
	};

	struct LocationLink : public virtual object {
		optional<Range> originSelectionRange;
		DocumentUri targetUri;
		Range targetRange;
		Range targetSelectionRange;

		void visit_fields(visitor_base&) override;
	};

	enum class DiagnosticSeverityEnum : unsigned char {
		Error = 1,
		Warning = 2,
		Information = 3,
		Hint = 4
	};
	using DiagnosticSeverity = numerical_enum<DiagnosticSeverityEnum>;

	enum class DiagnosticTagEnum : unsigned char {
		Unnecessary = 1,
		Deprecated = 2
	};
	using DiagnosticTag = numerical_enum<DiagnosticTagEnum>;

	struct DiagnosticRelatedInformation : public virtual object {
		Location location;
		string message;

		void visit_fields(visitor_base&) override;
	};

	struct CodeDescription : public virtual object {
		URI href;

		void visit_fields(visitor_base&) override;
	};

	struct Diagnostic : public virtual object {
		Range range;
		optional<DiagnosticSeverity> severity;
		optional<primitive_variant<integer, string>> code;
		optional<CodeDescription> codeDescription;
		optional<string> source;
		string message;
		optional<array<DiagnosticTag>> tags;
		optional<array<DiagnosticRelatedInformation>> relatedInformation;
		optional<any> data;

		void visit_fields(visitor_base&) override;
	};

	struct Command : public virtual object {
		string title;
		string command;
		optional<array<any>> arguments;

		void visit_fields(visitor_base&) override;
	};

	struct TextEdit : public virtual object {
		Range range;
		string newText;

		void visit_fields(visitor_base&) override;
	};

	struct ChangeAnnotation : public virtual object {
		string label;
		optional<boolean> needsConfirmation;
		optional<string> description;

		void visit_fields(visitor_base&) override;
	};

	using ChangeAnnotationIdentifier = string;

	struct AnnotatedTextEdit : public virtual TextEdit {
		ChangeAnnotationIdentifier annotationId;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentIdentifier : public virtual object {
		DocumentUri uri;

		void visit_fields(visitor_base&) override;
	};

	struct OptionalVersionedTextDocumentIdentifier : public virtual TextDocumentIdentifier {
		primitive_variant<null, integer> version;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentEdit : public virtual object {
		OptionalVersionedTextDocumentIdentifier textDocument;
		/*array<std::variant<TextEdit, AnnotatedTextEdit>> edits;*/ // TODO

		void visit_fields(visitor_base&) override;
	};

	struct CreateFileOptions : public virtual object {
		optional<boolean> overwrite;
		optional<boolean> ignoreIfExists;

		void visit_fields(visitor_base&) override;
	};

	struct CreateFile : public virtual object {
		// kind = 'create'
		DocumentUri uri;
		optional<CreateFileOptions> options;
		optional<ChangeAnnotationIdentifier> annotationId;

		void visit_fields(visitor_base&) override;
	};

	struct RenameFileOptions : public virtual object {
		optional<boolean> overwrite;
		optional<boolean> ignoreIfExists;

		void visit_fields(visitor_base&) override;
	};

	struct RenameFile : public virtual object {
		// kind = 'rename'
		DocumentUri oldUri;
		DocumentUri newUri;
		optional<RenameFileOptions> options;
		optional<ChangeAnnotationIdentifier> annotationId;

		void visit_fields(visitor_base&) override;
	};

	struct DeleteFileOptions : public virtual object {
		optional<boolean> recursive;
		optional<boolean> ignoreIfNotExists;

		void visit_fields(visitor_base&) override;
	};

	struct DeleteFile : public virtual object {
		// kind = 'delete'
		DocumentUri uri;
		optional<DeleteFileOptions> options;
		optional<ChangeAnnotationIdentifier> annotationId;

		void visit_fields(visitor_base&) override;
	};

	/// This struct is added so that visitors have a easy way to detect these variants. This object does not have
	/// a \p visit() function to force visitors to handle this explicitly.
	struct DocumentChange {
		std::variant<TextDocumentEdit, CreateFile, RenameFile, DeleteFile> value;
	};

	struct WorkspaceEdit : public virtual object {
		optional<map<array<TextEdit>>> changes;
		/*optional<DocumentChange> documentChanges;*/ // TODO
		optional<map<ChangeAnnotation>> changeAnnotations;

		void visit_fields(visitor_base&) override;
	};

	enum class ResourceOperationKind : unsigned char {
		create,
		rename,
		kDelete // renamed to avoid keyword
	};

	enum class FailureHandlingKind : unsigned char {
		abort,
		transactional,
		undo,
		textOnlyTransactional
	};

	struct WorkspaceEditClientCapabilities : public virtual object {
		optional<boolean> documentChanges;
		/*optional<array<ResourceOperationKind>> resourceOperations;*/ // TODO
		/*optional<FailureHandlingKind> failureHandling;*/ // TODO
		optional<boolean> normalizesLineEndings;
		// TODO changeAnnotationSupport

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentItem : public virtual object {
		DocumentUri uri;
		string languageId;
		integer version = 0;
		string text;

		void visit_fields(visitor_base&) override;
	};

	struct VersionedTextDocumentIdentifier : public virtual TextDocumentIdentifier {
		integer version = 0;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentPositionParams : public virtual object {
		TextDocumentIdentifier textDocument;
		Position position;

		void visit_fields(visitor_base&) override;
	};

	struct DocumentFilter : public virtual object {
		optional<string> language;
		optional<string> scheme;
		optional<string> pattern;

		void visit_fields(visitor_base&) override;
	};

	using DocumentSelector = array<DocumentFilter>;

	struct StaticRegistrationOptions : public virtual object {
		optional<string> id;

		void visit_fields(visitor_base&) override;
	};

	struct TextDocumentRegistrationOptions : public virtual object {
		primitive_variant<null, DocumentSelector> documentSelector;

		void visit_fields(visitor_base&) override;
	};

	/// A variant that decides whether an object is derived from \ref TextDocumentRegistrationOptions or not. The
	/// type that's not derived from \ref TextDocumentRegistrationOptions is put before the one that is and is
	/// initialized by default.
	template <
		typename Other, typename RegistrationOptions
	> struct TextDocumentRegistrationOptions_variant : public custom_variant_base {
		static_assert(
			std::is_base_of_v<TextDocumentRegistrationOptions, RegistrationOptions>,
			"the second template parameter must be derived from TextDocumentRegistrationOptions"
		);

		std::variant<Other, RegistrationOptions> value; ///< The value.

		/// If the value is an object and contains a field named \p documentSelector, the value is a
		/// \p RegistrationOptions. Otherwise use the other type.
		void deduce_type_and_visit(visitor_base &vis, const json::value_t &val) override {
			if (auto obj = val.try_cast<json::object_t>()) {
				if (auto member = obj->find_member(u8"documentSelector"); member != obj->member_end()) {
					vis.visit(value.template emplace<RegistrationOptions>());
					return;
				}
			}
			vis.visit(value.template emplace<Other>());
		}
		/// Visits the underlying value.
		void visit_value(visitor_base &vis) override {
			std::visit(
				[&vis](auto &&val) {
					vis.visit(val);
				}, value
			);
		}
	};

	enum class MarkupKindEnum : unsigned char {
		plaintext,
		markdown
	};
	struct MarkupKind : public virtual contiguous_string_enum<MarkupKindEnum, MarkupKind> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{ u8"plaintext", u8"markdown" };
			return _strings;
		}
	};

	struct MarkupContent : public virtual object {
		MarkupKind kind;
		string value;

		void visit_fields(visitor_base&) override;
	};

	struct MarkdownClientCapabilities : public virtual object {
		string parser;
		optional<string> version;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressBegin : public virtual object {
		// kind = 'begin'
		string title;
		optional<boolean> cancellable;
		optional<string> message;
		optional<uinteger> percentage;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressReport : public virtual object {
		// kind = 'report'
		optional<boolean> cancellable;
		optional<string> message;
		optional<uinteger> percentage;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressEnd : public virtual object {
		// kind = 'end'
		optional<string> message;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressParams : public virtual object {
		optional<ProgressToken> workDoneToken;

		void visit_fields(visitor_base&) override;
	};

	struct WorkDoneProgressOptions : public virtual object {
		optional<boolean> workDoneProgress;

		void visit_fields(visitor_base&) override;
	};

	struct PartialResultParams : public virtual object {
		optional<ProgressToken> partialResultToken;

		void visit_fields(visitor_base&) override;
	};

	enum class TraceValueEnum : unsigned char {
		off,
		message,
		verbose
	};
	struct TraceValue : contiguous_string_enum<TraceValueEnum, TraceValue> {
		friend contiguous_string_enum;

		inline static const std::vector<std::u8string_view> &get_strings() {
			static std::vector<std::u8string_view> _strings{ u8"off", u8"message", u8"verbose" };
			return _strings;
		}
	};
}
