// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Parser for regular expressions.

#include <string>
#include <utility>
#include <limits>
#include <vector>
#include <stack>
#include <variant>
#include <algorithm>

#include "codepad/core/encodings.h"
#include "codepad/core/assert.h"
#include "codepad/core/text.h"
#include "codepad/core/unicode/database.h"
#include "misc.h"
#include "ast.h"

namespace codepad::regex {
	namespace _details {
		/// Information about an escaped sequence.
		struct escaped_sequence {
		public:
			using error = ast_nodes::error; ///< Indicates that an error occurred when parsing this sequence.
			/// Overrides the start of the match.
			using match_start_override = ast_nodes::match_start_override;
			/// An escaped character class.
			struct character_class {
				codepoint_range_list ranges; ///< Codepoint ranges of this character class.
				bool is_negate = false; ///< Whether to match all characters **not** in \ref ranges.
			};
			/// A numbered backreference.
			struct numbered_backreference {
				/// Default constructor.
				numbered_backreference() = default;
				/// Initializes \ref index.
				explicit numbered_backreference(std::size_t i) : index(i) {
				}

				std::size_t index = 0; ///< Index of the referenced group.
			};
			/// A named backreference.
			struct named_backreference {
				/// Default constructor.
				named_backreference() = default;
				/// Initializes \ref name.
				explicit named_backreference(std::u8string s) : name(std::move(s)) {
				}

				std::u8string name; ///< The name of the group.
			};
			/// A simple assertion.
			using simple_assertion = ast_nodes::simple_assertion;
			/// A character class assertion.
			struct character_class_assertion {
				character_class char_class; ///< The character class.
				bool boundary = false; ///< Whether to match at any boundary.
			};

			using storage = std::variant<
				error,
				match_start_override,
				codepoint_string,
				character_class,
				numbered_backreference,
				named_backreference,
				simple_assertion,
				character_class_assertion
			>; ///< Storage for escaped sequence data.

			/// Default constructor.
			escaped_sequence() = default;
			/// Initializes \ref value.
			template <typename T> escaped_sequence(T v) : value(std::in_place_type<std::decay_t<T>>, std::move(v)) {
			}
			/// Creates a \ref escaped_sequence object that contains the given codepoint.
			[[nodiscard]] inline static escaped_sequence from_codepoint(codepoint cp) {
				escaped_sequence result;
				result.value.emplace<codepoint_string>(1, cp);
				return result;
			}

			/// Converts this into a \ref ast::node.
			void into_node(ast::node &n, const options &opt) && {
				std::visit(
					[&](auto &&val) {
						_into_node(std::move(val), n, opt);
					},
					std::move(value)
				);
			}

			storage value; ///< The value of this escaped sequence.
		protected:
			/// Conversion for nodes that are imported from \ref ast_nodes.
			template <typename Node> inline static void _into_node(Node &&val, ast::node &n, const options&) {
				n.value.emplace<std::decay_t<Node>>(std::move(val));
			}
			/// Conversion from a \ref codepoint_string to a \ref ast_nodes::literal.
			inline static void _into_node(codepoint_string &&val, ast::node &n, const options &opt) {
				auto &out = n.value.emplace<ast_nodes::literal>();
				out.contents = std::move(val);
				out.case_insensitive = opt.case_insensitive;
			}
			/// Conversion from a \ref character_class to a \ref ast_nodes::character_class.
			inline static void _into_node(character_class &&val, ast::node &n, const options &opt) {
				auto &out = n.value.emplace<ast_nodes::character_class>();
				out.ranges = std::move(val.ranges);
				out.is_negate = val.is_negate;
				out.case_insensitive = opt.case_insensitive;
			}
			/// Conversion from a \ref numbered_backreference to a \ref ast_nodes::numbered_backreference.
			inline static void _into_node(numbered_backreference &&val, ast::node &n, const options &opt) {
				auto &out = n.value.emplace<ast_nodes::numbered_backreference>();
				out.index = val.index;
				out.case_insensitive = opt.case_insensitive;
			}
			/// Conversion from a \ref named_backreference to a \ref ast_nodes::named_backreference.
			inline static void _into_node(named_backreference &&val, ast::node &n, const options &opt) {
				auto &out = n.value.emplace<ast_nodes::named_backreference>();
				out.name = std::move(val.name);
				out.case_insensitive = opt.case_insensitive;
			}
			/// Conversion from a \ref character_class_assertion to a \ref ast_nodes::character_class_assertion.
			inline static void _into_node(character_class_assertion &&val, ast::node &n, const options&) {
				auto &out = n.value.emplace<ast_nodes::character_class_assertion>();
				out.char_class.ranges = std::move(val.char_class.ranges);
				out.char_class.is_negate = val.char_class.is_negate;
				out.boundary = val.boundary;
			}
		};
	}

	/// Regular expression parser.
	///
	/// \tparam Stream The input stream. This should be a lightweight wrapper around the state of the stream because
	///                the parser checkpoints the stream by copying the entire object.
	template <typename Stream> class parser {
	public:
		/// Type for error callbacks.
		using error_callback_t = std::function<void(const Stream&, std::u8string_view)>;

		/// Initializes \ref on_error_callback.
		explicit parser(error_callback_t e) : on_error_callback(std::move(e)) {
		}

		/// Parses the whole stream.
		[[nodiscard]] ast parse(Stream s, options options) {
			// reset all state
			_result = ast();
			_stream = std::move(s);
			_option_stack.emplace(std::move(options));
			_capture_id_stack.emplace(1);

			_result._root = _parse_subexpression(
				ast_nodes::subexpression::type::non_capturing, std::numeric_limits<codepoint>::max()
			).first;

			// clean up
			_option_stack.pop();
			_capture_id_stack.pop();
			assert_true_logical(_option_stack.empty(), "option stack push/pop mismatch");
			assert_true_logical(_capture_id_stack.empty(), "capture stack push/pop mismatch");
			return std::move(_result);
		}
	
		/// The callback that will be called whenever an error is encountered.
		std::function<void(const Stream&, std::u8string_view)> on_error_callback;
	protected:
		ast _result; ///< The result.
		Stream _stream; ///< The input stream.
		std::stack<options> _option_stack; ///< Stack of regular expression options.
		std::stack<std::size_t> _capture_id_stack; ///< Used to handle duplicate group numbers.

		/// Returns the current options in effect.
		[[nodiscard]] const options &_options() const {
			return _option_stack.top();
		}
		/// Pushes a new set of options that's the same as the current options, and returns a reference to it.
		[[nodiscard]] options &_push_options() {
			_option_stack.push(_option_stack.top());
			return _option_stack.top();
		}
		/// Pops the current set of options.
		void _pop_options() {
			_option_stack.pop();
		}

		/// Indicates where an escape sequence is located.
		enum class _escaped_sequence_context {
			subexpression, ///< The escape sequence is outside of a character class.
			character_class ///< The escape sequence is in a character class.
		};
		/// Used when parsing alternatives.
		struct _alternative_context {
			/// The number of options that has been pushed when parsing the alternative.
			std::size_t pushed_options = 0;
			bool continues = false; ///< Used to indicate that the alternative has ended.
		};

		/// Parses an numerical value. Terminates when a invalid character is encountered, when the stream ends, or
		/// when the specified number of characters are consumed.
		template <typename IntType> [[nodiscard]] IntType _parse_numeric_value(
			std::size_t base,
			std::size_t length_limit = std::numeric_limits<std::size_t>::max(),
			IntType initial = 0
		);
		/// Parses a capture index.
		[[nodiscard]] std::optional<std::size_t> _parse_capture_index();
		/// Parses a repetition in curly brackets.
		[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> _parse_curly_brackets_repetition();
		/// Checks if the next characters are the same as the given string. If it is, then the characters will be
		/// consumed; otherwise, no changes will be made to \ref _stream.
		[[nodiscard]] bool _check_prefix(std::u32string_view);

		/// Parses an escaped sequence.
		[[nodiscard]] _details::escaped_sequence _parse_escaped_sequence(_escaped_sequence_context);

		/// Parses a character class in square brackets.
		[[nodiscard]] std::pair<
			ast_nodes::node_ref, ast_nodes::character_class&
		> _parse_square_brackets_character_class();

		/// Parses an alternative. This is called when a \p | is encountered in a subexpression, and therefore takes
		/// the first subexpression before the vertical bar as a parameter, so that it can be added as one of the
		/// alternatives. This function basically calls \ref _parse_subexpression() until the termination condition
		/// is met.
		[[nodiscard]] std::pair<ast_nodes::node_ref, ast_nodes::alternative&> _parse_alternative(
			ast_nodes::node_ref first_alternative, codepoint terminate,
			std::size_t pushed_options, ast_nodes::subexpression::type
		);

		/// Parses an expression in round brackets. This function determines what exact type the expression is
		/// (assertion, subroutine call, etc.), then calls the appropriate function (in most cases
		/// \ref _parse_subexpression()) to handle the actual parsing.
		///
		/// \param options_pushed Used to record the effective number of \ref options objects pushed by this
		///                       function.
		[[nodiscard]] ast_nodes::node_ref _parse_round_brackets_group(std::size_t &options_pushed);

		/// Removes and reutrns the last subexpression of the given subexpression. If the subexpression is empty or
		/// if the last element is not valid for repetitions, an empty \ref ast_nodes::node_ref will be returned.
		[[nodiscard]] ast_nodes::node_ref _pop_last_element_of_subexpression(ast_nodes::subexpression&);

		/// Appends a literal to the given expression.
		void _append_literal(
			ast_nodes::subexpression &expr, codepoint_string str, bool case_insensitive
		) {
			if (str.empty()) {
				return;
			}
			if (!expr.nodes.empty() && _result.get_node(expr.nodes.back()).is<ast_nodes::literal>()) {
				auto &lit = std::get<ast_nodes::literal>(_result.get_node(expr.nodes.back()).value);
				if (lit.case_insensitive == case_insensitive) {
					lit.contents.append(str);
					return;
				}
			}
			auto [res_ref, res] = _result.create_node<ast_nodes::literal>();
			expr.nodes.emplace_back(res_ref);
			res.case_insensitive = case_insensitive;
			res.contents = std::move(str);
		}
		/// Parses a subexpression or an alternative, terminating when the specified character is encountered or when
		/// the stream is empty. The character that caused termination will be consumed. The returned node can only
		/// be either an \ref ast_nodes::subexpression, or if the given \ref ast_nodes::subexpression::type is
		/// \ref ast_nodes::subexpression::type::non_capturing, an \ref ast_nodes::alternative.
		[[nodiscard]] std::pair<ast_nodes::node_ref, ast_nodes::subexpression&> _parse_subexpression(
			ast_nodes::subexpression::type, codepoint terminate, _alternative_context *alt_context = nullptr
		);
	};
}
