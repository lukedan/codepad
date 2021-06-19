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
			_state_stack.emplace(std::move(s));
			_option_stack.emplace(std::move(options));
			auto result = _parse_subexpression(std::numeric_limits<codepoint>::max());
			_state_stack.pop();
			assert_true_logical(_state_stack.empty(), "stream state stack push/pop mismatch");
			_option_stack.pop();
			assert_true_logical(_option_stack.empty(), "option stack push/pop mismatch");
			return result;
		}
	
		/// The callback that will be called whenever an error is encountered.
		std::function<bool(Stream, std::u8string_view)> on_error_callback;
	protected:
		std::stack<Stream> _state_stack; ///< The input stream.
		std::stack<options> _option_stack;

		/// Returns the current stream in the stack of saved states.
		[[nodiscard]] Stream &_stream() {
			return _state_stack.top();
		}
		/// Pushes the current stream state onto \ref _state_stack.
		void _checkpoint() {
			_state_stack.push(_state_stack.top());
		}
		/// Pops the current stream and replaces the previous state with it.
		void _cancel_checkpoint() {
			Stream s = std::move(_state_stack.top());
			_state_stack.pop();
			_state_stack.top() = std::move(s);
		}
		/// Pops the top element from \ref _state_stack.
		void _restore_checkpoint() {
			_state_stack.pop();
		}

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

		/// Parses an octal value. Terminates when a invalid character is encountered, when the stream ends, or when
		/// the specified number of characters are consumed.
		[[nodiscard]] codepoint _parse_numeric_value(
			std::size_t base,
			std::size_t length_limit = std::numeric_limits<std::size_t>::max(),
			codepoint initial = 0
		) {
			codepoint value = initial;
			for (std::size_t i = 0; i < length_limit && !_stream().empty(); ++i) {
				codepoint digit = _stream().peek();
				if (digit >= U'0' && digit <= U'9') {
					digit -= U'0';
				} else if (digit >= U'a' && digit <= U'z') {
					digit = (digit - U'a') + 10;
				} else if (digit >= U'A' && digit <= U'z') {
					digit = (digit - U'A') + 10;
				} else {
					break; // invalid character
				}
				if (digit >= base) {
					break; // digit too large; treat as invalid
				}
				_stream().take();
				value = static_cast<codepoint>(value * base + digit);
			}
			return value;
		}
		/// Parses an escaped sequence. This function checkpoints the stream in certain conditions, and should not be
		/// called when a checkpoint is active.
		[[nodiscard]] std::variant<
			ast::nodes::error,
			ast::nodes::literal,
			ast::nodes::character_class,
			ast::nodes::backreference,
			ast::nodes::assertion
		> _parse_escaped_sequence(_escaped_sequence_context ctx) {
			codepoint cp = _stream().take();
			switch (cp) {
			// octal character code
			case U'0':
				return ast::nodes::literal::from_codepoint(_parse_numeric_value(8, 2));
			case U'o':
				{
					if (_stream().empty() || _stream().peek() != U'{') {
						return ast::nodes::error(); // no starting bracket
					}
					_stream().take();
					codepoint value = _parse_numeric_value(8);
					if (_stream().empty() || _stream().peek() != U'}') {
						return ast::nodes::error(); // no ending bracket
					}
					_stream().take();
					return ast::nodes::literal::from_codepoint(value);
				}

			// hexadecimal character code
			case U'x':
				{
					if (_stream().empty() || _stream().peek() != U'{') {
						return ast::nodes::literal::from_codepoint(_parse_numeric_value(16, 2));
					} else {
						// \x{...}: if parsing fails, restore checkpoint
						_checkpoint();
						_stream().take();
						codepoint value = _parse_numeric_value(16);
						if (_stream().empty() || _stream().peek() != U'}') {
							// no ending bracket; resume parsing from the beginning bracket
							_restore_checkpoint();
							return ast::nodes::literal::from_codepoint(0);
						}
						_stream().take();
						_cancel_checkpoint();
						return ast::nodes::literal::from_codepoint(value);
					}
				}

			// control-x
			case U'c':
				{
					if (_stream().empty()) {
						return ast::nodes::error();
					}
					codepoint val = _stream().take();
					if (val >= U'a' && val <= U'z') {
						val = val - U'a' + U'A';
					}
					if (val >= 128) { // accept all characters < 128
						return ast::nodes::error();
					}
					return ast::nodes::literal::from_codepoint(val ^ 0x40);
				}

			// backreference
			case U'8':
				[[fallthrough]];
			case U'9':
				{
					if (ctx == _escaped_sequence_context::character_class) {
						return ast::nodes::literal::from_codepoint(cp); // interpret as literals
					}
					ast::nodes::backreference result;
					std::size_t &idx = std::get<std::size_t>(result.index);
					idx = cp - U'0';
					if (!_stream().empty()) {
						cp = _stream().peek();
						if (cp >= U'0' && cp <= U'9') {
							_stream().take();
							idx = idx * 10 + (cp - U'0');
						}
					}
					return result;
				}
			case U'g':
				// TODO backreference
				break;

			// character class
			case U'N':
				if (ctx == _escaped_sequence_context::character_class) {
					break; // \N not allowed in character classes
				}
				// TODO non-newline character
				break;
			case U'd': // decimal digit
				{
					ast::nodes::character_class result;
					result.ranges = unicode::unicode_data::cache::get_codepoints_in_category(
						unicode::general_category_index::decimal_number
					);
					return result;
				}
			case U's': // white space
				{
					ast::nodes::character_class result;
					result.ranges = unicode::property_list::get_cached().white_space;
					return result;
				}

			// anchor
			case U'b':
				{
					if (ctx == _escaped_sequence_context::character_class) {
						return ast::nodes::literal::from_codepoint(0x08);
					}
					ast::nodes::assertion result;
					result.assertion_type = ast::nodes::assertion::type::character_class_boundary;
					auto &char_class =
						result.expression.nodes.emplace_back().value.emplace<ast::nodes::character_class>();
					char_class.case_insensitive = _options().case_insensitive;
					char_class.ranges = tables::word_characters();
					return result;
				}
			case U'B':
				{
					if (ctx != _escaped_sequence_context::character_class) {
						ast::nodes::assertion result;
						result.assertion_type = ast::nodes::assertion::type::character_class_nonboundary;
						auto &char_class =
							result.expression.nodes.emplace_back().value.emplace<ast::nodes::character_class>();
						char_class.case_insensitive = _options().case_insensitive;
						char_class.ranges = tables::word_characters();
						return result;
					}
				}
				break;

			// 'proper' escaped characters
			case U'a':
				return ast::nodes::literal::from_codepoint(0x07);
			case U'e':
				return ast::nodes::literal::from_codepoint(0x1B);
			case U'f':
				return ast::nodes::literal::from_codepoint(0x0C);
			case U'n':
				return ast::nodes::literal::from_codepoint(0x0A);
			case U'r':
				return ast::nodes::literal::from_codepoint(0x0D);
			case U't':
				return ast::nodes::literal::from_codepoint(0x09);
			}
			if (cp >= U'0' && cp <= U'7') { // octal character code or backreference
				codepoint number = cp - U'0';
				if (ctx == _escaped_sequence_context::subexpression) {
					if (!_stream().empty()) {
						cp = _stream().peek();
						if (cp < U'0' || cp > U'9') { // must be a backreference
							ast::nodes::backreference ref;
							ref.index = number;
							return ref;
						}
						/*_stream().take();
						number = number * 10 + cp - U'0';*/
					}
				}
				// read up to three octal digits, but we've read one already
				return ast::nodes::literal::from_codepoint(_parse_numeric_value(8, 2, number));
			}
			return ast::nodes::literal::from_codepoint(cp);
		}

		/// Parses a character class in square brackets.
		[[nodiscard]] ast::nodes::character_class _parse_square_brackets_character_class() {
			// TODO support \Q \E
			ast::nodes::character_class result;
			if (_stream().empty()) {
				// TODO error - early termination
				return result;
			}
			if (_stream().peek() == U'^') {
				result.is_negate = true;
				_stream().take();
			}
			auto parse_char = [this]() -> std::variant<
				ast::nodes::error,
				ast::nodes::literal,
				ast::nodes::character_class,
				ast::nodes::backreference,
				ast::nodes::assertion
			> {
				codepoint cp = _stream().take();
				if (_options().extended_more) {
					while (cp == U' ' || cp == U'\t') {
						if (_stream().empty()) {
							return ast::nodes::error();
						}
						cp = _stream().take();
					}
				}
				if (cp == U'\\') {
					return _parse_escaped_sequence(_escaped_sequence_context::character_class);
				}
				return ast::nodes::literal::from_codepoint(cp);
			};
			while (true) {
				if (_stream().empty()) {
					// TODO error
					break;
				}
				// the first character can be a closing bracket and will still count; otherwise finish & return
				if (!result.ranges.ranges.empty() && _stream().peek() == U']') {
					_stream().take();
					break;
				}
				// TODO check for special character classes
				if (_stream().peek() == U'[') {

				}
				auto elem = parse_char();
				if (std::holds_alternative<ast::nodes::character_class>(elem)) {
					auto &cls = std::get<ast::nodes::character_class>(elem);
					result.ranges.ranges.insert(
						result.ranges.ranges.end(), cls.ranges.ranges.begin(), cls.ranges.ranges.end()
					);
				} else if (std::holds_alternative<ast::nodes::literal>(elem)) {
					auto &literal = std::get<ast::nodes::literal>(elem).contents;
					auto &range = result.ranges.ranges.emplace_back(literal.front(), literal.front());
					if (!_stream().empty() && _stream().peek() == U'-') { // parse a range
						_stream().take();
						if (_stream().empty()) {
							// TODO error
							break;
						}
						if (_stream().peek() == U']') {
							// special case: we want to terminate here instead of treating it as a range
							_stream().take();
							result.ranges.ranges.emplace_back(U'-', U'-');
							break;
						}
						auto next_elem = parse_char();
						if (std::holds_alternative<ast::nodes::literal>(next_elem)) {
							range.last = std::get<ast::nodes::literal>(next_elem).contents.front();
							if (range.last < range.first) {
								// TODO error
								std::swap(range.first, range.last);
							}
						} else {
							result.ranges.ranges.emplace_back(U'-', U'-');
							if (std::holds_alternative<ast::nodes::character_class>(next_elem)) {
								auto &cls = std::get<ast::nodes::character_class>(next_elem);
								result.ranges.ranges.insert(
									result.ranges.ranges.end(), cls.ranges.ranges.begin(), cls.ranges.ranges.end()
								);
							} // ignore error
						}
					}
				} // ignore errors
			}
			result.ranges.sort_and_compact();
			result.case_insensitive = _options().case_insensitive;
			return result;
		}

		/// Parses an alternative. This is called when a \p | is encountered in a subexpression, and therefore takes
		/// the first subexpression before the vertical bar as a parameter, so that it can be added as one of the
		/// alternatives. This function basically calls \ref _parse_subexpression() until the termination condition
		/// is met.
		[[nodiscard]] ast::nodes::alternative _parse_alternative(
			ast::nodes::subexpression first_alternative, codepoint terminate, std::size_t pushed_options
		) {
			ast::nodes::alternative result;
			result.alternatives.emplace_back(std::move(first_alternative));
			_alternative_context context;
			context.pushed_options = pushed_options;
			do {
				context.continues = false;
				result.alternatives.emplace_back(_parse_subexpression(terminate, &context));
				if (!context.continues) {
					break;
				}
			} while (!_stream().empty());
			for (std::size_t i = 0; i < context.pushed_options; ++i) {
				_pop_options();
			}
			return result;
		}

		/// Parses a repetition in curly brackets. If parsing fails, the codepoint string will be returned instead.
		[[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> _parse_curly_brackets_repetition() {
			std::size_t min = 0, max = 0;
			// special case: no number between brackets, interpret as a literal
			if (!_stream().empty() && (_stream().peek() == U'}' || _stream().peek() == U',')) {
				return std::nullopt;
			}
			// parse first number
			while (true) {
				if (_stream().empty()) {
					return std::nullopt;
				}
				codepoint cp = _stream().peek();
				if (cp == U',') {
					_stream().take();
					break; // go on to parse the second number
				}
				if (cp == U'}') {
					// match exactly n times; we're done
					_stream().take(); // no need to add to str
					max = min;
					return std::make_pair(min, max);
				}
				if (cp >= U'0' && cp <= U'9') {
					_stream().take();
					min = min * 10 + (cp - U'0');
				} else {
					return std::nullopt;
				}
			}
			// special case: no second number, no upper bound
			if (!_stream().empty() && _stream().peek() == U'}') {
				_stream().take();
				max = ast::nodes::repetition::no_limit;
				return std::make_pair(min, max);
			}
			// parse second number
			while (true) {
				if (_stream().empty()) {
					return std::nullopt;
				}
				codepoint cp = _stream().peek();
				if (cp == U'}') {
					_stream().take();
					break;
				}
				if (cp >= U'0' && cp <= U'9') {
					_stream().take();
					max = max * 10 + (cp - U'0');
				} else {
					return std::nullopt;
				}
			}
			return std::make_pair(min, max);
		}
		/// Removes and reutrns the last subexpression of the given subexpression. The input subexpression must not
		/// be empty.
		ast::nodes::subexpression _last_element_of_subexpression(ast::nodes::subexpression &expr) {
			ast::nodes::subexpression result;
			auto &last_node = expr.nodes.back();
			if (last_node.is<ast::nodes::subexpression>()) {
				result = std::move(std::get<ast::nodes::subexpression>(last_node.value));
				expr.nodes.pop_back();
			} else {
				auto &result_node = result.nodes.emplace_back();
				if (last_node.is<ast::nodes::literal>()) {
					auto &lit = std::get<ast::nodes::literal>(last_node.value);
					result_node.value.emplace<ast::nodes::literal>().contents.push_back(lit.contents.back());
					lit.contents.pop_back();
					if (lit.contents.empty()) {
						expr.nodes.pop_back();
					}
				} else if (last_node.is<ast::nodes::repetition>()) {
					// TODO error
				} else {
					result_node.value = std::move(last_node.value);
					expr.nodes.pop_back();
				}
			}
			return result;
		}

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
		/// Parses a subexpression or an alternative, terminating when the specified character is encountered or when
		/// the stream is empty. The character that caused termination will be consumed.
		[[nodiscard]] ast::nodes::subexpression _parse_subexpression(
			codepoint terminate, _alternative_context *alt_context = nullptr
		) {
			ast::nodes::subexpression result;
			std::size_t options_pushed = 0;
			while (!_stream().empty()) {
				codepoint cp = _stream().take();
				if (cp == terminate) {
					break;
				}
				switch (cp) {
				// subexpression, alternatives, options
				case U'(':
					{
						bool is_subexpression = true;
						auto expr_type = ast::nodes::subexpression::type::normal;
						codepoint_string capture_name;
						// determine if this is a named capture, assertion, etc.
						if (!_stream().empty() && _stream().peek() == U'?') {
							_stream().take();
							if (_stream().empty()) {
								// TODO fail
							} else {
								switch (_stream().peek()) {
								case U':': // non-capturing subexpression
									_stream().take();
									expr_type = ast::nodes::subexpression::type::non_capturing;
									break;
								case U'|': // duplicate subexpression numbers
									_stream().take();
									expr_type = ast::nodes::subexpression::type::duplicate;
									break;
								case U'>': // atomic subexpression
									_stream().take();
									expr_type = ast::nodes::subexpression::type::atomic;
									break;
								case U'=': // positive assertions
									_stream().take();
									// TODO
									break;
								case U'!': // negative assertions
									_stream().take();
									// TODO
									break;

								// named subpatterns
								case U'P':
									_stream().take();
									if (_stream().empty() || _stream().peek() != U'<') {
										// TODO error
										break;
									}
									[[fallthrough]];
								case U'\'':
									[[fallthrough]];
								case U'<':
									{ // parse named capture
										codepoint name_end = U'\0';
										switch (_stream().take()) {
										case U'\'':
											name_end = U'\'';
											break;
										case U'<':
											name_end = U'>';
											break;
										}
										while (true) {
											if (_stream().empty()) {
												// TODO error
												break;
											}
											codepoint next_cp = _stream().take();
											if (next_cp == name_end) {
												break;
											}
											capture_name.push_back(next_cp);
										}
										// TODO validate capture name?
									}
									break;

								// TODO

								default: // enable/disabe features midway through the expression
									{
										is_subexpression = false;
										bool enable_feature = true;
										bool allow_disable = true;
										auto &opts = _push_options();
										++options_pushed;
										if (!_stream().empty() && _stream().peek() == U'^') {
											allow_disable = false;
											opts = options();
										}
										while (!_stream().empty()) {
											bool stop = false;
											switch (_stream().take()) {
											case U'i': // caseless
												opts.case_insensitive = enable_feature;
												break;
											case U'm':
												opts.case_insensitive = enable_feature;
												break;
											case U'n':
												opts.no_auto_capture = enable_feature;
												break;
											case U's':
												opts.dot_all = enable_feature;
												break;
											case U'x':
												if (!_stream().empty() && _stream().peek() == U'x') {
													// extended_more
													_stream().take();
													opts.extended_more = true;
												}
												opts.extended = true;
												break;

											case U')':
												// finish - continue parsing this subexpression
												stop = true;
												break;
											case U':':
												// syntax sugar - parse until the end for a subexpression
												{
													stop = true;
													auto &new_expr = result.nodes.emplace_back().value.emplace<
														ast::nodes::subexpression
													>(_parse_subexpression(U')'));
													new_expr.type_or_assertion =
														ast::nodes::subexpression::type::non_capturing;
													// the options only affect this subexpression, not the outer one it contains
													--options_pushed;
													_pop_options();
												}
												break;
											}
											if (stop) {
												break;
											}
										}
									}
									break;
								}
							}
						}
						if (is_subexpression) {
							auto &new_expr = result.nodes.emplace_back().value.emplace<ast::nodes::subexpression>(
								_parse_subexpression(U')')
							);
							new_expr.type_or_assertion = expr_type;
							new_expr.capture_name = std::move(capture_name);
						}
					}
					break;
				// alternatives
				case U'|':
					if (alt_context == nullptr) {
						// parse whatever remains using _parse_alternative()
						auto alt = _parse_alternative(std::move(result), terminate, options_pushed);
						result = ast::nodes::subexpression();
						result.nodes.emplace_back().value.emplace<ast::nodes::alternative>(std::move(alt));
					} else {
						// otherwise, return the subexpression and set it to continue
						alt_context->continues = true;
						alt_context->pushed_options += options_pushed;
					}
					options_pushed = 0;
					return result;

				// character class
				case U'[':
					result.nodes.emplace_back().value.emplace<ast::nodes::character_class>(
						_parse_square_brackets_character_class()
					);
					break;
				case U'.':
					{
						auto &char_cls = result.nodes.emplace_back().value.emplace<ast::nodes::character_class>();
						char_cls.is_negate = true;
						if (!_options().dot_all) {
							char_cls.ranges.ranges.emplace_back(codepoint_range(U'\r'));
							char_cls.ranges.ranges.emplace_back(codepoint_range(U'\n'));
							char_cls.ranges.sort_and_compact();
						}
					}
					break;

				// anchors
				case U'^':
					{
						auto &assertion = result.nodes.emplace_back().value.emplace<ast::nodes::assertion>();
						assertion.assertion_type =
							_options().multiline ?
							ast::nodes::assertion::type::line_start :
							ast::nodes::assertion::type::subject_start;
					}
					break;
				case U'$':
					{
						auto &assertion = result.nodes.emplace_back().value.emplace<ast::nodes::assertion>();
						assertion.assertion_type =
							_options().multiline ?
							ast::nodes::assertion::type::line_end :
							ast::nodes::assertion::type::subject_end_or_trailing_newline;
					}
					break;

				// repetition
				case U'{':
					[[fallthrough]];
				case U'?':
					[[fallthrough]];
				case U'*':
					[[fallthrough]];
				case U'+':
					{
						ast::nodes::repetition rep;
						switch (cp) {
						case U'{':
							{
								_checkpoint();
								if (auto times = _parse_curly_brackets_repetition()) {
									_cancel_checkpoint();
									rep.min = times->first;
									rep.max = times->second;
								} else {
									// failed to parse range; interpret it as a literal
									_restore_checkpoint();
									_append_literal(result, _options().case_insensitive).contents.push_back(U'{');
									continue; // outer loop
								}
							}
							break;
						case U'?':
							rep.min = 0;
							rep.max = 1;
							break;
						case U'*':
							rep.min = 0;
							rep.max = ast::nodes::repetition::no_limit;
							break;
						case U'+':
							rep.min = 1;
							rep.max = ast::nodes::repetition::no_limit;
							break;
						}
						if (!_stream().empty() && _stream().peek() == U'?') {
							rep.lazy = true;
						}
						if (result.nodes.empty()) {
							// TODO error
							break;
						}
						rep.expression = _last_element_of_subexpression(result);
						result.nodes.emplace_back().value.emplace<ast::nodes::repetition>(std::move(rep));
					}
					break;
					
				// literal or escaped sequence
				case U'\\':
					if (!_stream().empty() && _stream().peek() == U'Q') {
						// treat everything between this and \E as a string literal
						_stream().take();
						auto &literal = _append_literal(result, _options().case_insensitive);
						while (true) {
							if (_stream().empty()) {
								// TODO error
								break;
							}
							codepoint ch = _stream().take();
							if (ch == U'\\') { // check for \E and break
								if (!_stream().empty() && _stream().peek() == U'E') {
									_stream().take();
									break;
								}
							}
							literal.contents.push_back(ch);
						}
					} else {
						// otherwise parse the literal/character class
						auto escaped = _parse_escaped_sequence(_escaped_sequence_context::subexpression);
						if (std::holds_alternative<ast::nodes::literal>(escaped)) {
							auto &lit = _append_literal(result, _options().case_insensitive);
							lit.contents.append(std::get<ast::nodes::literal>(escaped).contents);
						} else {
							std::visit(
								[&result](auto &&v) {
									result.nodes.emplace_back().value = std::move(v);
								},
								std::move(escaped)
							);
						}
					}
					break;
				default:
					if (_options().extended) {
						if (tables::extended_mode_whitespaces().contains(cp)) {
							continue;
						}
						if (cp == U'#') { // consume comment
							auto &stream = _stream();
							while (!stream.empty() && (stream.peek() != U'\r' && stream.peek() != U'\n')) {
								stream.take();
							}
							consume_line_ending(stream);
							continue;
						}
					}
					_append_literal(result, _options().case_insensitive).contents.push_back(cp);
					break;
				}
			}
			for (std::size_t i = 0; i < options_pushed; ++i) {
				_pop_options();
			}
			return result;
		}
	};
}
