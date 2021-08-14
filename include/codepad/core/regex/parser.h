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
	/// Regular expression parser.
	///
	/// \tparam Stream The input stream. This should be a lightweight wrapper around the state of the stream because
	///                the parser checkpoints the stream by copying the entire object.
	template <typename Stream> class parser {
	public:
		/// Parses the whole stream.
		[[nodiscard]] ast::nodes::subexpression parse(Stream s, options options) {
			_stream = std::move(s);
			_option_stack.emplace(std::move(options));
			_capture_id_stack.emplace(1);
			ast::nodes::subexpression result;
			result.subexpr_type = ast::nodes::subexpression::type::non_capturing;
			_parse_subexpression(result, std::numeric_limits<codepoint>::max());
			_option_stack.pop();
			_capture_id_stack.pop();
			assert_true_logical(_option_stack.empty(), "option stack push/pop mismatch");
			assert_true_logical(_capture_id_stack.empty(), "capture stack push/pop mismatch");
			return result;
		}
	
		/// The callback that will be called whenever an error is encountered.
		std::function<bool(Stream, std::u8string_view)> on_error_callback;
	protected:
		using _escaped_sequence_node = std::variant<
			ast::nodes::error,
			ast::nodes::match_start_override,
			ast::nodes::literal,
			ast::nodes::character_class,
			ast::nodes::numbered_backreference,
			ast::nodes::named_backreference,
			ast::nodes::simple_assertion,
			ast::nodes::character_class_assertion
		>; ///< A node that could result from an escaped sequence.

		Stream _stream; ///< The input stream.
		std::stack<options> _option_stack; ///< Stack of regular expression options.
		std::stack<std::size_t> _capture_id_stack; ///< Used to handle duplicate group numbers.

		/// Returns the current options in effect.
		[[nodiscard]] const options &_options() const {
			return _option_stack.top();
		}
		/// Pushes a new set of options.
		options &_push_options() {
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
		/// Checks if the next characters are the same as the given string. If it is, then the characters will be
		/// consumed; otherwise, no changes will be made to \ref _stream.
		[[nodiscard]] bool _check_prefix(std::u32string_view);

		/// Parses an escaped sequence. This function checkpoints the stream in certain conditions, and should not be
		/// called when a checkpoint is active.
		[[nodiscard]] _escaped_sequence_node _parse_escaped_sequence(_escaped_sequence_context);
		/// Parses a capture index.
		[[nodiscard]] std::optional<std::size_t> _parse_capture_index();

		/// Parses a character class in square brackets.
		[[nodiscard]] ast::nodes::character_class _parse_square_brackets_character_class();

		/// Parses an alternative. This is called when a \p | is encountered in a subexpression, and therefore takes
		/// the first subexpression before the vertical bar as a parameter, so that it can be added as one of the
		/// alternatives. This function basically calls \ref _parse_subexpression() until the termination condition
		/// is met.
		[[nodiscard]] ast::nodes::alternative _parse_alternative(
			ast::nodes::subexpression first_alternative, codepoint terminate,
			std::size_t pushed_options, ast::nodes::subexpression::type
		);

		/// Parses a group in round brackets, and adds the result to the end of the given subexpression.
		void _parse_round_brackets_group(ast::nodes::subexpression&, std::size_t &options_pushed);

		/// Parses a repetition in curly brackets. If parsing fails, the codepoint string will be returned instead.
		[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> _parse_curly_brackets_repetition();
		/// Removes and reutrns the last subexpression of the given subexpression. The input subexpression must not
		/// be empty.
		ast::nodes::subexpression _pop_last_element_of_subexpression(ast::nodes::subexpression&);

		/// Makes sure that the last element of the subexpression is a literal, and returns it.
		ast::nodes::literal &_append_literal(ast::nodes::subexpression &expr, bool case_insensitive) {
			if (!expr.nodes.empty() && expr.nodes.back().is<ast::nodes::literal>()) {
				auto &lit = std::get<ast::nodes::literal>(expr.nodes.back().value);
				if (lit.case_insensitive == case_insensitive) {
					return lit;
				}
			}
			auto &lit = expr.nodes.emplace_back().value.emplace<ast::nodes::literal>();
			lit.case_insensitive = case_insensitive;
			return lit;
		}
		/// Parses a subexpression, an assertion, or an alternative, terminating when the specified character is
		/// encountered or when the stream is empty. The character that caused termination will be consumed.
		void _parse_subexpression(
			ast::nodes::subexpression&, codepoint terminate, _alternative_context *alt_context = nullptr
		);
	};
}
