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

namespace codepad::regex {
	using codepoint_string = std::basic_string<codepoint>; ///< A string of codepoints.

	/// Input stream type for basic strings.
	template <typename Encoding> struct basic_string_input_stream {
	public:
		/// Default constructor.
		basic_string_input_stream() = default;
		/// Initializes \ref _string.
		basic_string_input_stream(const std::byte *beg, const std::byte *end) : _cur(beg), _end(end) {
			if (_cur != _end) {
				if (!Encoding::next_codepoint(_cur, _end, _cp)) {
					_cp = encodings::replacement_character;
				}
				--_cur;
			}
		}

		/// Returns whether _position is at the end of the string.
		[[nodiscard]] bool empty() const {
			return _cur == _end;
		}
		/// Returns the current character and increments \ref _position.
		codepoint take() {
			assert_true_logical(!empty(), "taking from an empty stream");
			codepoint res = _cp;
			if (++_cur != _end) {
				if (!Encoding::next_codepoint(_cur, _end, _cp)) {
					_cp = encodings::replacement_character;
				}
				--_cur;
			}
			++_pos;
			return res;
		}
		/// Returns the current character.
		[[nodiscard]] codepoint peek() {
			assert_true_logical(!empty(), "peeking an empty stream");
			return _cp;
		}

		/// Returns the current position of this stream.
		[[nodiscard]] std::size_t position() const {
			return _pos;
		}
	protected:
		std::size_t _pos = 0; ///< Position of the stream in codepoints.
		codepoint _cp = 0; /// The current codepoint.
		const std::byte *_cur = nullptr; ///< Last (not past!) byte of \ref _cp.
		const std::byte *_end = nullptr; ///< Pointer past the final byte.
	};

	/// Consumes a line ending from the given stream.
	///
	/// \return Type of the consumed line ending.
	template <typename Stream> line_ending consume_line_ending(Stream &s) {
		if (s.empty()) {
			return line_ending::none;
		}
		switch (s.peek()) {
		case U'\r':
			s.take();
			if (s.empty() || s.peek() != U'\n') {
				return line_ending::r;
			}
			s.take();
			return line_ending::rn;
		case U'\n':
			s.take();
			return line_ending::n;
		}
		return line_ending::none;
	}


	namespace ast {
		struct node;

		namespace nodes {
			/// A node indicating an error.
			struct error {
			};

			/// Node used to signal a feature is enabled.
			struct feature {
				codepoint_string identifier; ///< String used to identify the feature.
			};
			/// A node that contains a string literal.
			struct literal {
				/// Returns a \ref literal node that contains only the given codepoint.
				[[nodiscard]] inline static literal from_codepoint(codepoint cp) {
					literal result;
					result.contents.push_back(cp);
					return result;
				}

				codepoint_string contents; ///< The literal.
			};
			/// A backreference.
			struct backreference {
				std::variant<std::size_t, codepoint_string> index; ///< The index of this backreference.
				/// Indicates that this may be an octal character code instead of a backreference.
				bool is_ambiguous = false;
			};
			/// Node that represents a class of characters.
			struct character_class {
				std::vector<std::pair<codepoint, codepoint>> ranges; ///< Ranges in the character class.
				/// Indicates whether this matches all characters **not** in this class, as opposed to all characters
				/// in this class.
				bool is_negate = false;

				/// Sorts \ref ranges and merges intersecting ranges.
				void sort_and_compact() {
					std::sort(ranges.begin(), ranges.end());
					if (!ranges.empty()) {
						auto prev = ranges.begin();
						for (auto it = ranges.begin() + 1; it != ranges.end(); ++it) {
							if (it->first <= prev->second + 1) {
								prev->second = it->second;
							} else {
								*++prev = *it;
							}
						}
						ranges.erase(prev + 1, ranges.end());
					}
				}
				/// Handles negation. This function assumes that \ref sort_and_compact() has been called.
				[[nodiscard]] std::vector<std::pair<codepoint, codepoint>> get_effective_ranges() const {
					if (is_negate) {
						codepoint last = 0;
						std::vector<std::pair<codepoint, codepoint>> result;
						auto iter = ranges.begin();
						if (!ranges.empty() && ranges.front().first == 0) {
							last = ranges.front().second + 1;
							++iter;
						}
						for (; iter != ranges.end(); ++iter) {
							result.emplace_back(last, iter->first - 1);
							last = iter->second + 1;
						}
						if (last <= encodings::unicode_max) {
							result.emplace_back(last, encodings::unicode_max);
						}
						return result;
					}
					return ranges;
				}
			};

			/// A subexpression. This is not necessarily surrounded by brackets; this node simply represents any
			/// sequence of tokens.
			struct subexpression {
				/// The type of a subexpression.
				enum class type {
					normal, ///< Normal subexpressions.
					non_capturing, ///< This subexpression does not capture its contents.
					duplicate, ///< All captures within this alternative use the same capture indices.
					atomic, ///< The matcher should not retry when matching fails after this subexpression.
				};

				std::vector<node> nodes; ///< Nodes in this sub-expression.
				std::variant<std::size_t, codepoint_string> capture_index; ///< Capture index.
				/// The type of this subexpression or assertion. Since subexpressions are used in many contexts, by
				/// default the subexpression does not capture.
				type type_or_assertion = type::non_capturing;
			};

			/// Alternatives.
			struct alternative {
				std::vector<subexpression> alternatives; ///< Alternative expressions.
			};
			/// A node that represents a repetition.
			struct repetition {
				/// Indicates that there's no limit for the upper or lower bound of the number of repetitions.
				constexpr static std::size_t no_limit = std::numeric_limits<std::size_t>::max();

				subexpression expression; ///< The expression to be repeated.
				std::size_t min = 0; ///< The minimum number of repetitions.
				std::size_t max = no_limit; ///< The maximum number of repetitions.
			};
		}

		/// A generic node.
		struct node {
			/// Variant type for node storage.
			using storage = std::variant<
				nodes::error,
				nodes::feature,
				nodes::literal,
				nodes::backreference,
				nodes::character_class,
				nodes::subexpression,
				nodes::alternative,
				nodes::repetition
			>;

			storage value; ///< The value of this node.

			/// Shorthand for \p std::holds_alternative().
			template <typename Node> [[nodiscard]] bool is() const {
				return std::holds_alternative<Node>(value);
			}
		};


		/// Dumps an AST.
		template <typename Stream> struct dumper {
		public:
			/// Initializes \ref _stream.
			explicit dumper(Stream &s) : _stream(s) {
			}

			/// Dumps a \ref nodes::error.
			void dump(const nodes::error&) {
				_indent();
				_stream << "── [ERROR]\n";
			}
			/// Dumps a \ref nodes::feature.
			void dump(const nodes::feature &n) {
				_indent();
				_stream << "── [feature: ";
				for (codepoint cp : n.identifier) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "]\n";
			}
			/// Dumps a \ref nodes::literal.
			void dump(const nodes::literal &n) {
				_indent();
				_stream << "── [literal: \"";
				for (codepoint cp : n.contents) {
					_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
				}
				_stream << "\"]\n";
			}
			/// Dumps a \ref nodes::backreference.
			void dump(const nodes::backreference &n) {
				_indent();
				_stream << "── [backreference: ";
				if (std::holds_alternative<codepoint_string>(n.index)) {
					_stream << "\"";
					const auto &id = std::get<codepoint_string>(n.index);
					for (codepoint cp : id) {
						_stream << reinterpret_cast<const char*>(encodings::utf8::encode_codepoint(cp).c_str());
					}
					_stream << "\"";
				} else {
					_stream << "#" << std::get<std::size_t>(n.index);
				}
				_stream << "]\n";
			}
			/// Dumps a \ref nodes::character_class.
			void dump(const nodes::character_class &n) {
				_indent();
				_stream << "── [character class " << (n.is_negate ? "[!]" : "") << ": ";
				bool first = true;
				for (auto [beg, end] : n.ranges) {
					if (!first) {
						_stream << ", ";
					}
					first = false;
					if (beg == end) {
						_stream << beg;
					} else {
						_stream << beg << " - " << end;
					}
				}
				_stream << "]\n";
			}
			/// Dumps a \ref nodes::subexpression.
			void dump(const nodes::subexpression &n) {
				_indent();
				if (n.nodes.empty()) {
					_stream << "─";
				} else {
					_stream << "┬";
				}
				_stream << "─ [subexpression";
				if (n.type_or_assertion != ast::nodes::subexpression::type::non_capturing) {
					_stream << " #";
					if (std::holds_alternative<std::size_t>(n.capture_index)) {
						_stream << std::get<std::size_t>(n.capture_index);
					} else {
						auto &str = std::get<codepoint_string>(n.capture_index);
						for (codepoint cp : str) {
							_stream << reinterpret_cast<const char*>(
								encodings::utf8::encode_codepoint(cp).c_str()
							);
						}
					}
				}
				switch (n.type_or_assertion) {
				case ast::nodes::subexpression::type::normal:
					break;
				case ast::nodes::subexpression::type::non_capturing:
					_stream << " (non-capturing)";
					break;
				case ast::nodes::subexpression::type::duplicate:
					_stream << " (duplicate)";
					break;
				case ast::nodes::subexpression::type::atomic:
					_stream << " (atomic)";
					break;
				}
				_stream << "]\n";
				_branch.emplace_back(true);
				for (std::size_t i = 0; i < n.nodes.size(); ++i) {
					if (i + 1 == n.nodes.size()) {
						_branch.back() = false;
					}
					dump(n.nodes[i]);
				}
				_branch.pop_back();
			}
			/// Dumps a \ref nodes::alternative.
			void dump(const nodes::alternative &n) {
				_indent();
				_stream << "┬─ [alternative]\n";
				_branch.emplace_back(true);
				for (std::size_t i = 0; i < n.alternatives.size(); ++i) {
					if (i + 1 == n.alternatives.size()) {
						_branch.back() = false;
					}
					dump(n.alternatives[i]);
				}
				_branch.pop_back();
			}
			/// Dumps a \ref nodes::repetition.
			void dump(const nodes::repetition &n) {
				_indent();
				_stream << "┬─ [repetition  min: " << n.min << "  max: " << n.max << "]\n";
				_branch.emplace_back(false);
				dump(n.expression);
				_branch.pop_back();
			}

			/// Dumps a \ref node.
			void dump(const node &n) {
				std::visit(
					[this](auto &&concrete_node) {
						dump(concrete_node);
					},
					n.value
				);
			}
		protected:
			std::vector<bool> _branch; ///< Indicates whether there's an active branch at the given level.
			Stream &_stream; ///< The output stream.

			/// Indents to the correct level.
			void _indent() {
				if (_branch.size() > 0) {
					_stream << "  ";
				}
				for (std::size_t i = 0; i + 1 < _branch.size(); ++i) {
					_stream << (_branch[i] ? "│ " : "  ");
				}
				if (_branch.empty()) {
					_stream << ">─";
				} else if (_branch.back()) {
					_stream << "├─";
				} else {
					_stream << "└─";
				}
			}
		};
		/// Shorthand for creating a \ref dumper.
		template <typename Stream> dumper<Stream> make_dumper(Stream &s) {
			return dumper<Stream>(s);
		}
	}

	/// Regular expression parser.
	///
	/// \tparam Stream The input stream. This should be a lightweight wrapper around the state of the stream because
	///                the parser checkpoints the stream by copying the entire object.
	template <typename Stream> class parser {
	public:
		/// Parses the whole stream.
		[[nodiscard]] ast::nodes::subexpression parse(Stream s) {
			_state_stack.emplace(std::move(s));
			return _parse_subexpression(std::numeric_limits<codepoint>::max());
		}

		bool extended = false; ///< Whether or not to parse in extended mode.
	protected:
		std::stack<Stream> _state_stack; ///< The input stream.

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

		/// Indicates where an escape sequence is located.
		enum class _escaped_sequence_context {
			subexpression, ///< The escape sequence is outside of a character class.
			character_class ///< The escape sequence is in a character class.
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
			ast::nodes::error, ast::nodes::literal, ast::nodes::character_class, ast::nodes::backreference
		> _parse_escaped_sequence(_escaped_sequence_context ctx) {
			codepoint cp = _stream().take();
			ast::nodes::literal lit;
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
					if (val < U'A' || val > U'Z') {
						// TODO PCRE documentation says it accepts all characters < 128, but testing showed otherwise
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
					result.ranges.emplace_back(U'0', U'9');
					// TODO unicode
					return result;
				}
			case U's': // white space
				{
					ast::nodes::character_class result;
					result.ranges.emplace_back(0x9, 0xD);
					result.ranges.emplace_back(0x20, 0x20);
					// TODO unicode
					return result;
				}

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
				ast::nodes::error, ast::nodes::literal, ast::nodes::character_class, ast::nodes::backreference
			> {
				codepoint cp = _stream().take();
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
				if (!result.ranges.empty() && _stream().peek() == U']') {
					_stream().take();
					break;
				}
				// TODO check for special character classes
				if (_stream().peek() == U'[') {

				}
				auto elem = parse_char();
				if (std::holds_alternative<ast::nodes::character_class>(elem)) {
					auto &cls = std::get<ast::nodes::character_class>(elem);
					result.ranges.insert(result.ranges.end(), cls.ranges.begin(), cls.ranges.end());
				} else if (std::holds_alternative<ast::nodes::literal>(elem)) {
					auto &literal = std::get<ast::nodes::literal>(elem).contents;
					auto &range = result.ranges.emplace_back(literal.front(), literal.front());
					if (!_stream().empty() && _stream().peek() == U'-') { // parse a range
						_stream().take();
						if (_stream().empty()) {
							// TODO error
							break;
						}
						if (_stream().peek() == U']') {
							// special case: we want to terminate here instead of treating it as a range
							_stream().take();
							result.ranges.emplace_back(U'-', U'-');
							break;
						}
						auto next_elem = parse_char();
						if (std::holds_alternative<ast::nodes::literal>(next_elem)) {
							range.second = std::get<ast::nodes::literal>(next_elem).contents.front();
							if (range.second < range.first) {
								// TODO error
								std::swap(range.first, range.second);
							}
						} else {
							result.ranges.emplace_back(U'-', U'-');
							if (std::holds_alternative<ast::nodes::character_class>(next_elem)) {
								auto &cls = std::get<ast::nodes::character_class>(next_elem);
								result.ranges.insert(result.ranges.end(), cls.ranges.begin(), cls.ranges.end());
							} // ignore error
						}
					}
				} // ignore errors
			}
			result.sort_and_compact();
			return result;
		}

		/// Parses an alternative. This is called when a \p | is encountered in a subexpression, and therefore takes
		/// the first subexpression before the vertical bar as a parameter, so that it can be added as one of the
		/// alternatives. This function basically calls \ref _parse_subexpression() until the termination condition
		/// is met.
		[[nodiscard]] ast::nodes::alternative _parse_alternative(
			ast::nodes::subexpression first_alternative, codepoint terminate
		) {
			ast::nodes::alternative result;
			result.alternatives.emplace_back(std::move(first_alternative));
			do {
				bool alt_continue = false;
				result.alternatives.emplace_back(_parse_subexpression(terminate, &alt_continue));
				if (!alt_continue) {
					break;
				}
			} while (!_stream().empty());
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
		ast::nodes::literal &_append_literal(ast::nodes::subexpression &expr) {
			if (!expr.nodes.empty() && expr.nodes.back().is<ast::nodes::literal>()) {
				return std::get<ast::nodes::literal>(expr.nodes.back().value);
			}
			return expr.nodes.emplace_back().value.emplace<ast::nodes::literal>();
		}
		/// Parses a subexpression or an alternative, terminating when the specified character is encountered or when
		/// the stream is empty. The character that caused termination will be consumed.
		[[nodiscard]] ast::nodes::subexpression _parse_subexpression(
			codepoint terminate, bool *alternative_continues = nullptr
		) {
			ast::nodes::subexpression result;
			while (!_stream().empty()) {
				codepoint cp = _stream().take();
				if (cp == terminate) {
					break;
				}
				switch (cp) {
				// subexpression & alternatives
				case U'(':
					{
						auto expr_type = ast::nodes::subexpression::type::normal;
						// determine if this is a named capture, assertion, etc.
						if (!_stream().empty() && _stream().peek() == U'?') {
							_stream().take();
							if (_stream().empty()) {
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
								// named subpatterns
								case U'P':
									[[fallthrough]];
								case U'<':
									_checkpoint();
									_stream().take();
									// TODO
									_restore_checkpoint();
									break;
								case U'>': // atomic subexpression
									_stream().take();
									expr_type = ast::nodes::subexpression::type::atomic;
									break;
								case U'=': // positive assertions
									// TODO
									break;
								case U'!': // negative assertions
									// TODO
									break;
								// TODO
								}
							}
						}
						auto &new_expr = result.nodes.emplace_back().value.emplace<ast::nodes::subexpression>(
							_parse_subexpression(U')')
						);
						new_expr.type_or_assertion = expr_type;
					}
					break;
				// alternatives
				case U'|':
					if (alternative_continues == nullptr) {
						// parse whatever remains using _parse_alternative()
						auto alt = _parse_alternative(std::move(result), terminate);
						result = ast::nodes::subexpression();
						result.nodes.emplace_back().value.emplace<ast::nodes::alternative>(std::move(alt));
					} else {
						// otherwise, return the subexpression and set it to continue
						*alternative_continues = true;
					}
					return result;

				// character class
				case U'[':
					result.nodes.emplace_back().value.emplace<ast::nodes::character_class>(
						_parse_square_brackets_character_class()
					);
					break;
				case U'.':
					result.nodes.emplace_back().value.emplace<ast::nodes::character_class>().is_negate = true;
					break;

				// anchors
				case U'^':
					// TODO
					break;
				case U'$':
					// TODO
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
									_append_literal(result).contents.push_back(U'{');
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
						auto &literal = _append_literal(result);
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
							_append_literal(result).contents.append(std::get<ast::nodes::literal>(escaped).contents);
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
					if (extended) {
						if (!is_graphical_char(cp)) {
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
					_append_literal(result).contents.push_back(cp);
					break;
				}
			}
			return result;
		}
	};
}
