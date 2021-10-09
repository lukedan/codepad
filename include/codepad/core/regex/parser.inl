// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once
#include "parser.h"

/// \file
/// Implementation of the parser.

namespace codepad::regex {
	template <typename Stream> template <typename IntType> inline IntType parser<Stream>::_parse_numeric_value(
		std::size_t base, std::size_t length_limit, IntType initial
	) {
		IntType value = initial;
		for (std::size_t i = 0; i < length_limit && !_stream.empty(); ++i) {
			codepoint digit = _stream.peek();
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
			_stream.take();
			value = value * static_cast<IntType>(base) + static_cast<IntType>(digit);
		}
		return value;
	}

	template <typename Stream> inline std::optional<std::size_t> parser<Stream>::_parse_capture_index() {
		if (_stream.empty()) {
			return std::nullopt;
		}
		Stream checkpoint = _stream;
		int direction = 0; // 1: relative forward, -1: relative backward
		codepoint next_cp = _stream.peek();
		if (next_cp == U'-') {
			_stream.take();
			direction = -1;
		} else if (next_cp == U'+') {
			_stream.take();
			direction = 1;
		}
		if (_stream.empty()) {
			_stream = checkpoint;
			return std::nullopt;
		}
		next_cp = _stream.peek();
		if (next_cp < U'0' || next_cp > U'9') { // invalid
			_stream = checkpoint;
			return std::nullopt;
		}
		// numbered backreference
		std::size_t index = _parse_numeric_value<std::size_t>(10);
		// relative backreferences
		if (direction > 0) {
			if (index == 0) {
				_stream = checkpoint;
				return std::nullopt;
			}
			index = _capture_id_stack.top() + index - 1;
		} else if (direction < 0) {
			if (_capture_id_stack.top() <= index) {
				_stream = checkpoint;
				return std::nullopt;
			}
			index = _capture_id_stack.top() - index;
		}
		return index;
	}

	template <
		typename Stream
	> inline std::optional<std::pair<std::size_t, std::size_t>> parser<Stream>::_parse_curly_brackets_repetition() {
		std::size_t min = 0, max = 0;
		// special case: no number between brackets, interpret as a literal
		if (!_stream.empty() && (_stream.peek() == U'}' || _stream.peek() == U',')) {
			return std::nullopt;
		}
		// parse first number
		while (true) {
			if (_stream.empty()) {
				return std::nullopt;
			}
			codepoint cp = _stream.peek();
			if (cp == U',') {
				_stream.take();
				break; // go on to parse the second number
			}
			if (cp == U'}') {
				// match exactly n times; we're done
				_stream.take(); // no need to add to str
				max = min;
				return std::make_pair(min, max);
			}
			if (cp >= U'0' && cp <= U'9') {
				_stream.take();
				min = min * 10 + (cp - U'0');
			} else {
				return std::nullopt;
			}
		}
		// special case: no second number, no upper bound
		if (!_stream.empty() && _stream.peek() == U'}') {
			_stream.take();
			max = ast_nodes::repetition::no_limit;
			return std::make_pair(min, max);
		}
		// parse second number
		while (true) {
			if (_stream.empty()) {
				return std::nullopt;
			}
			codepoint cp = _stream.peek();
			if (cp == U'}') {
				_stream.take();
				break;
			}
			if (cp >= U'0' && cp <= U'9') {
				_stream.take();
				max = max * 10 + (cp - U'0');
			} else {
				return std::nullopt;
			}
		}
		return std::make_pair(min, max);
	}

	template <typename Stream> bool parser<Stream>::_check_prefix(std::u32string_view sv) {
		Stream temp = _stream;
		for (char32_t c : sv) {
			if (temp.empty() || temp.take() != c) {
				return false;
			}
		}
		_stream = std::move(temp);
		return true;
	}

	template <typename Stream> inline _details::escaped_sequence parser<Stream>::_parse_escaped_sequence(
		_escaped_sequence_context ctx
	) {
		if (_stream.empty()) {
			on_error_callback(_stream, u8"Pattern ends with a back slash");
			return _details::escaped_sequence();
		}
		codepoint cp = _stream.take();
		switch (cp) {
		// octal character code
		case U'0':
			return _details::escaped_sequence::from_codepoint(_parse_numeric_value<codepoint>(8, 2));
		case U'o':
			{
				if (_stream.empty() || _stream.peek() != U'{') {
					on_error_callback(_stream, u8"Missing opening curly bracket for \\o escaped sequence");
					return _details::escaped_sequence(); // no starting bracket
				}
				_stream.take();
				auto value = _parse_numeric_value<codepoint>(8);
				if (_stream.empty() || _stream.peek() != U'}') {
					on_error_callback(_stream, u8"Missing closing curly bracket for \\o escaped sequence");
				} else {
					_stream.take();
				}
				return _details::escaped_sequence::from_codepoint(value);
			}

		// hexadecimal character code
		case U'x':
			{
				if (_stream.empty() || _stream.peek() != U'{') {
					return _details::escaped_sequence::from_codepoint(_parse_numeric_value<codepoint>(16, 2));
				} else {
					// \x{...}: if parsing fails, restore checkpoint
					_stream.take();
					auto value = _parse_numeric_value<codepoint>(16);
					if (_stream.empty() || _stream.peek() != U'}') {
						on_error_callback(_stream, u8"Missing closing curly bracket for \\x escaped sequence");
					} else {
						_stream.take();
					}
					return _details::escaped_sequence::from_codepoint(value);
				}
			}

		// control-x
		case U'c':
			{
				if (_stream.empty()) {
					on_error_callback(_stream, u8"Invalid \\c escaped sequence");
					return _details::escaped_sequence();
				}
				codepoint val = _stream.take();
				if (val >= U'a' && val <= U'z') {
					val = val - U'a' + U'A';
				}
				if (val >= 128) { // accept all characters < 128
					on_error_callback(_stream, u8"Value too large for \\c escaped sequence");
					return _details::escaped_sequence();
				}
				return _details::escaped_sequence::from_codepoint(val ^ 0x40);
			}

		// backreference
		case U'8':
			[[fallthrough]];
		case U'9':
			if (ctx == _escaped_sequence_context::subexpression) {
				std::size_t idx = cp - U'0';
				if (!_stream.empty()) {
					cp = _stream.peek();
					if (cp >= U'0' && cp <= U'9') {
						_stream.take();
						idx = idx * 10 + (cp - U'0');
					}
				}
				return _details::escaped_sequence::numbered_backreference(idx);
			}
			break;
		case U'g':
			if (ctx == _escaped_sequence_context::subexpression) {
				std::size_t index = 0;
				if (_stream.empty()) {
					on_error_callback(_stream, u8"\\g escaped sequence not followed by group name or index");
				} else if (_stream.peek() == U'{') {
					_stream.take();
					if (auto id = _parse_capture_index()) { // numbered backreference
						if (_stream.empty() || _stream.peek() != U'}') {
							on_error_callback(_stream, u8"Missing closing curly bracket for \\g escaped sequence");
						} else {
							_stream.take();
						}
						index = id.value();
					} else { // named backreference
						codepoint_string res;
						while (true) {
							if (_stream.empty()) {
								on_error_callback(
									_stream, u8"Missing closing curly bracket for \\g escaped sequence"
								);
								break;
							}
							codepoint cur_cp = _stream.take();
							if (cur_cp == U'}') {
								break;
							}
							res.push_back(cur_cp);
						}
						return _details::escaped_sequence::named_backreference(std::move(res));
					}
				} else {
					if (auto id = _parse_capture_index()) {
						index = id.value();
					} else {
						on_error_callback(_stream, u8"Invalid \\g escaped sequence");
					}
				}
				return _details::escaped_sequence::numbered_backreference(index);
			}
			break;
		case U'k':
			if (ctx == _escaped_sequence_context::subexpression) {
				if (_stream.empty()) {
					on_error_callback(_stream, u8"\\k escaped sequence not followed by capture name");
					break;
				}
				codepoint end = U'\0';
				switch (_stream.take()) {
				case U'{':
					end = U'}';
					break;
				case U'<':
					end = U'>';
					break;
				case U'\'':
					end = U'\'';
					break;
				default:
					on_error_callback(_stream, u8"Invalid capture name delimiter for \\k escaped sequence");
					break;
				}
				codepoint_string capture_name;
				while (!_stream.empty()) {
					codepoint cur_cp = _stream.take();
					if (cur_cp == end) {
						break;
					}
					capture_name.push_back(cur_cp);
				}
				return _details::escaped_sequence::named_backreference(std::move(capture_name));
			}
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
				_details::escaped_sequence::character_class result;
				result.ranges = unicode::unicode_data::cache::get_codepoints_in_category(
					unicode::general_category_index::decimal_number
				);
				return result;
			}
		case U'D': // not decimal digit
			{
				_details::escaped_sequence::character_class result;
				result.ranges = unicode::unicode_data::cache::get_codepoints_in_category(
					unicode::general_category_index::decimal_number
				);
				result.is_negate = true;
				return result;
			}
		case U'h': // horizontal white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::horizontal_whitespaces();
				return result;
			}
		case U'H': // not horizontal white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::horizontal_whitespaces();
				result.is_negate = true;
				return result;
			}
		case U's': // white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = unicode::property_list::get_cached().white_space;
				return result;
			}
		case U'S': // not white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = unicode::property_list::get_cached().white_space;
				result.is_negate = true;
				return result;
			}
		case U'v': // vertical white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::vertical_whitespaces();
				return result;
			}
		case U'V': // not vertical white space
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::vertical_whitespaces();
				result.is_negate = true;
				return result;
			}
		case U'w': // word
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::word_characters();
				return result;
			}
		case U'W': // not word
			{
				_details::escaped_sequence::character_class result;
				result.ranges = tables::word_characters();
				result.is_negate = true;
				return result;
			}

		// anchor
		case U'A':
			if (ctx != _escaped_sequence_context::character_class) {
				ast_nodes::simple_assertion result;
				result.assertion_type = ast_nodes::simple_assertion::type::subject_start;
				return result;
			}
			break;
		case U'Z':
			if (ctx != _escaped_sequence_context::character_class) {
				ast_nodes::simple_assertion result;
				result.assertion_type = ast_nodes::simple_assertion::type::subject_end_or_trailing_newline;
				return result;
			}
			break;
		case U'z':
			if (ctx != _escaped_sequence_context::character_class) {
				ast_nodes::simple_assertion result;
				result.assertion_type = ast_nodes::simple_assertion::type::subject_end;
				return result;
			}
			break;
		case U'b':
			{
				if (ctx == _escaped_sequence_context::character_class) {
					return _details::escaped_sequence::from_codepoint(0x08);
				}
				_details::escaped_sequence::character_class_assertion result;
				result.boundary = true;
				result.char_class.ranges = tables::word_characters();
				return result;
			}
		case U'B':
			if (ctx != _escaped_sequence_context::character_class) {
				_details::escaped_sequence::character_class_assertion result;
				result.boundary = false;
				result.char_class.ranges = tables::word_characters();
				return result;
			}
			break;

		// 'proper' escaped characters
		case U'a':
			return _details::escaped_sequence::from_codepoint(0x07);
		case U'e':
			return _details::escaped_sequence::from_codepoint(0x1B);
		case U'f':
			return _details::escaped_sequence::from_codepoint(0x0C);
		case U'n':
			return _details::escaped_sequence::from_codepoint(0x0A);
		case U'r':
			return _details::escaped_sequence::from_codepoint(0x0D);
		case U't':
			return _details::escaped_sequence::from_codepoint(0x09);

		// nothing - we handle \Q\E elsewhere
		case U'E':
			return codepoint_string();

		// special
		case U'K':
			if (ctx != _escaped_sequence_context::character_class) {
				return ast_nodes::match_start_override();
			}
			break;
		}

		if (cp >= U'1' && cp <= U'7') { // octal character code or backreference
			if (ctx == _escaped_sequence_context::subexpression) {
				if (_stream.empty()) { // single digit - must be a backreference
					return _details::escaped_sequence::numbered_backreference(cp - U'0');
				}
				codepoint next_cp = _stream.peek();
				if (next_cp < U'0' || next_cp > U'9') { // single digit - must be a backreference
					return _details::escaped_sequence::numbered_backreference(cp - U'0');
				}
				// otherwise check if there are already this much captures
				Stream checkpoint = _stream;
				std::size_t index = (cp - U'0') * 10 + (next_cp - U'0');
				_stream.take();
				while (!_stream.empty()) {
					next_cp = _stream.peek();
					if (next_cp < U'0' || next_cp > U'9') {
						break;
					}
					index = index * 10 + (next_cp - U'0');
					_stream.take();
				}
				if (index < _capture_id_stack.top()) {
					return _details::escaped_sequence::numbered_backreference(index);
				}
				_stream = std::move(checkpoint); // it's a hexadecimal codepoint; restore checkpoint
			}
			// read up to three octal digits, but we've read one already
			return _details::escaped_sequence::from_codepoint(_parse_numeric_value<codepoint>(8, 2, cp - U'0'));
		}
		return _details::escaped_sequence::from_codepoint(cp);
	}

	template <typename Stream> inline std::pair<
		ast_nodes::node_ref, ast_nodes::character_class&
	> parser<Stream>::_parse_square_brackets_character_class() {
		using _token = std::variant<
			_details::escaped_sequence::error,
			codepoint_string,
			_details::escaped_sequence::character_class
		>;

		auto [result_ref, result] = _result.create_node<ast_nodes::character_class>();
		if (!_stream.empty() && _stream.peek() == U'^') {
			result.is_negate = true;
			_stream.take();
		}
		// if we run into \Q, this function simply returns an empty literal
		auto parse_char = [&]() -> _token {
			while (!_stream.empty()) {
				codepoint cp = _stream.take();
				if (_options().extended_more) {
					while (cp == U' ' || cp == U'\t') {
						if (_stream.empty()) {
							return _details::escaped_sequence::error();
						}
						cp = _stream.take();
					}
				}
				if (cp == U'\\') {
					if (_stream.empty()) {
						break;
					}
					if (_stream.peek() == U'Q') {
						_stream.take();
						return codepoint_string();
					}
					// an isolated \E - ignore it
					if (_stream.peek() == U'E') {
						_stream.take();
						continue;
					}
					auto raw_res = _parse_escaped_sequence(_escaped_sequence_context::character_class);
					if (std::holds_alternative<codepoint_string>(raw_res.value)) {
						return std::move(std::get<codepoint_string>(raw_res.value));
					} else if (std::holds_alternative<_details::escaped_sequence::character_class>(raw_res.value)) {
						return std::move(std::get<_details::escaped_sequence::character_class>(raw_res.value));
					} else {
						assert_true_logical(
							std::holds_alternative<_details::escaped_sequence::error>(raw_res.value),
							"invalid return type for escape sequence in character class"
						);
						return _details::escaped_sequence::error();
					}
				}
				return codepoint_string(1, cp);
			}
			return codepoint_string();
		};
		bool force_literal = false; // whether we're in a \Q \E sequence
		while (true) {
			if (_stream.empty()) {
				on_error_callback(_stream, u8"Missing closing square bracket for character class");
				break;
			}
			// handle \Q \E
			if (force_literal) {
				codepoint cp = _stream.take();
				if (cp == U'\\' && !_stream.empty() && _stream.peek() == U'E') {
					// end of the sequence
					_stream.take();
					force_literal = false;
				} else {
					result.ranges.ranges.emplace_back(cp, cp);
				}
				continue;
			}
			// the first character can be a closing bracket and will still count; otherwise finish & return
			if (!result.ranges.ranges.empty() && _stream.peek() == U']') {
				_stream.take();
				break;
			}
			// check for special character classes
			if (_stream.peek() == U'[') {
				Stream checkpoint = _stream;
				_stream.take();
				if (!_stream.empty() && _stream.take() == U':') {
					bool negate = false;
					if (!_stream.empty() && _stream.peek() == U'^') {
						negate = true;
					}
					codepoint_string str;
					while (!_stream.empty()) { // extract characters until :]
						codepoint cp = _stream.take();
						if (cp == U':') {
							break;
						}
						str.push_back(cp);
					}
					if (!_stream.empty() && _stream.take() == U']') {
						// we have an enclosed range; check if the name is valid
						std::u32string_view sv(reinterpret_cast<const char32_t*>(str.data()), str.size());
						codepoint_range_list ranges;
						if (sv == U"alnum") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category::letter | unicode::general_category::number
							);
						} else if (sv == U"alpha") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category::letter
							);
						} else if (sv == U"ascii") {
							ranges.ranges.emplace_back(0, 127);
						} else if (sv == U"blank") {
							ranges = tables::horizontal_whitespaces();
						} else if (sv == U"cntrl") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category_index::control
							);
						} else if (sv == U"digit") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category_index::decimal_number
							);
						} else if (sv == U"graph") {
							// TODO
						} else if (sv == U"lower") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category_index::lowercase_letter
							);
						} else if (sv == U"print") {
							// TODO
						} else if (sv == U"punct") {
							// TODO
						} else if (sv == U"space") {
							ranges = tables::posix_spaces();
						} else if (sv == U"upper") {
							ranges = unicode::unicode_data::cache::get_codepoints_in_category(
								unicode::general_category_index::uppercase_letter
							);
						} else if (sv == U"word") {
							// TODO
						} else if (sv == U"xdigit") {
							// TODO
						}
						if (!ranges.ranges.empty()) {
							// we have a set of ranges; add it to the result and continue to the next iteration
							if (negate) {
								// NOTE here we're simply negating the ranges. this should work relatively well since
								//      we cover entire categories, but there may be subtle bugs
								ranges = ranges.get_negated();
							}
							result.ranges.ranges.insert(
								result.ranges.ranges.end(), ranges.ranges.begin(), ranges.ranges.end()
							);
							continue;
						}
					}
				}
				_stream = std::move(checkpoint); // failed; restore checkpoint
			}
			auto elem = parse_char();
			if (std::holds_alternative<_details::escaped_sequence::character_class>(elem)) {
				const auto &list = std::get<_details::escaped_sequence::character_class>(elem);
				auto cls = list.is_negate ? list.ranges.get_negated() : list.ranges;
				result.ranges.ranges.insert(result.ranges.ranges.end(), cls.ranges.begin(), cls.ranges.end());
			} else if (std::holds_alternative<codepoint_string>(elem)) {
				auto &literal = std::get<codepoint_string>(elem);
				if (literal.empty()) { // we've ran into a \Q
					force_literal = true;
					continue;
				}
				auto &range = result.ranges.ranges.emplace_back(literal.front(), literal.front());
				if (!_stream.empty() && _stream.peek() == U'-') { // parse a range
					_stream.take();
					if (_stream.empty()) {
						on_error_callback(_stream, u8"Missing closing square bracket for character class");
						break;
					}
					if (_stream.peek() == U']') {
						// special case: we want to terminate here instead of treating it as a range
						_stream.take();
						result.ranges.ranges.emplace_back(U'-', U'-');
						break;
					}
					auto next_elem = parse_char();
					if (std::holds_alternative<codepoint_string>(next_elem)) {
						auto &lit = std::get<codepoint_string>(next_elem);
						if (!lit.empty()) {
							range.last = lit.front();
							if (range.last <= range.first) {
								on_error_callback(_stream, u8"Inverted character range in character class");
								std::swap(range.first, range.last);
							}
							continue;
						}
					}
					// we're unable to parse a full range - add the dash to the character class
					result.ranges.ranges.emplace_back(U'-', U'-');
					if (std::holds_alternative<_details::escaped_sequence::character_class>(next_elem)) {
						const auto &cls = std::get<_details::escaped_sequence::character_class>(next_elem);
						auto ranges = cls.is_negate ? cls.ranges.get_negated() : cls.ranges;
						result.ranges.ranges.insert(
							result.ranges.ranges.end(), ranges.ranges.begin(), ranges.ranges.end()
						);
					} // ignore error
				}
			} // ignore errors
		}
		result.ranges.sort_and_compact();
		result.case_insensitive = _options().case_insensitive;
		return { result_ref, result };
	}

	template <typename Stream> inline std::pair<
		ast_nodes::node_ref, ast_nodes::alternative&
	> parser<Stream>::_parse_alternative(
		ast_nodes::node_ref first_alternative, codepoint terminate,
		std::size_t pushed_options, ast_nodes::subexpression::type expr_type
	) {
		std::size_t max = 0;

		auto [result_ref, result] = _result.create_node<ast_nodes::alternative>();
		result.alternatives.emplace_back(first_alternative);
		_alternative_context context;
		context.pushed_options = pushed_options;
		do {
			if (expr_type == ast_nodes::subexpression::type::duplicate) {
				// save & reset group index
				max = std::max(max, _capture_id_stack.top());
				_capture_id_stack.pop();
				_capture_id_stack.emplace(_capture_id_stack.top());
			}
			context.continues = false;
			result.alternatives.emplace_back(_parse_subexpression(
				ast_nodes::subexpression::type::non_capturing, terminate, &context
			).first);
			if (!context.continues) {
				break;
			}
		} while (!_stream.empty());
		// pop all pused options (that are shared across the alternatives)
		for (std::size_t i = 0; i < context.pushed_options; ++i) {
			_pop_options();
		}
		// handle duplicate indices
		if (expr_type == ast_nodes::subexpression::type::duplicate) {
			// update group index
			max = std::max(max, _capture_id_stack.top());
			_capture_id_stack.pop();
			_capture_id_stack.top() = max;
		}
		return { result_ref, result };
	}

	template <typename Stream> inline ast_nodes::node_ref parser<Stream>::_parse_round_brackets_group(
		std::size_t &options_pushed
	) {
		if (_stream.empty()) {
			on_error_callback(_stream, u8"Missing closing round bracket");
			return ast_nodes::node_ref();
		}

		// is_complex_assertion is used to indicate whether we need to continue parsing the assertion, and the
		// attributes of the assertion are stored in complex_ass
		bool is_complex_assertion = false;
		bool complex_assertion_negative = false;
		bool complex_assertion_backward = false;
		bool complex_assertion_non_atomic = false;
		// otherwise, the result is a subexpresion (other types return early)
		auto expr_type = ast_nodes::subexpression::type::normal;
		codepoint_string capture_name;

		// determine if this is a named capture, assertion, etc.
		switch (_stream.peek()) {
		case U'?':
			_stream.take();
			if (_stream.empty()) {
				on_error_callback(_stream, u8"Missing closing round bracket");
				return ast_nodes::node_ref();
			}
			switch (_stream.peek()) {
			case U':': // non-capturing subexpression
				_stream.take();
				expr_type = ast_nodes::subexpression::type::non_capturing;
				break;
			case U'|': // duplicate subexpression numbers
				_stream.take();
				expr_type = ast_nodes::subexpression::type::duplicate;
				// save the subexpression number
				_capture_id_stack.emplace(_capture_id_stack.top());
				break;
			case U'>': // atomic subexpression
				_stream.take();
				expr_type = ast_nodes::subexpression::type::atomic;
				break;

			// named subpatterns
			case U'<':
				[[fallthrough]];
			case U'P':
				[[fallthrough]];
			case U'\'':
				{
					codepoint start = _stream.take();
					if (start == U'<') { // may also be an assertion
						if (!_stream.empty()) {
							switch (_stream.peek()) {
							case U'=': // positive lookbehind
								_stream.take();
								is_complex_assertion = true;
								complex_assertion_backward = true;
								complex_assertion_negative = false;
								break;
							case U'!': // negative lookbehind
								_stream.take();
								is_complex_assertion = true;
								complex_assertion_backward = true;
								complex_assertion_negative = true;
								break;
							}
							if (is_complex_assertion) {
								break; // parse as an assertion instead
							}
						}
					}

					if (start == U'P') {
						if (_stream.empty()) {
							on_error_callback(_stream, u8"Missing closing round bracket");
							return ast_nodes::node_ref();
						}
						if (_stream.peek() == U'>') { // named subroutine
							_stream.take();
							auto [node_ref, node] = _result.create_node<ast_nodes::named_subroutine>();
							while (true) {
								if (_stream.empty()) {
									on_error_callback(_stream, u8"Missing end round bracket for named subroutine");
									break;
								}
								codepoint cp = _stream.take();
								if (cp == U')') {
									break;
								}
								node.name.push_back(cp);
							}
							return node_ref;
						}
						if (_stream.peek() != U'<') {
							// TODO not a named capture
							break;
						}
						_stream.take();
						start = U'<';
					}

					// parse named capture
					codepoint name_end = U'\0';
					switch (start) {
					case U'\'':
						name_end = U'\'';
						break;
					case U'<':
						name_end = U'>';
						break;
					}
					while (true) {
						if (_stream.empty()) {
							on_error_callback(_stream, u8"Missing closing delimiter for capture name");
							return ast_nodes::node_ref();
						}
						codepoint next_cp = _stream.take();
						if (next_cp == name_end) {
							break;
						}
						capture_name.push_back(next_cp);
					}
					// TODO validate capture name?
				}
				break;

			// assertions
			case U'=':
				_stream.take();
				is_complex_assertion = true;
				complex_assertion_backward = false;
				complex_assertion_negative = false;
				break;
			case U'!':
				_stream.take();
				is_complex_assertion = true;
				complex_assertion_backward = false;
				complex_assertion_negative = true;
				break;

			// conditional subexpression
			case U'(':
				{
					_stream.take();
					if (_stream.empty()) {
						on_error_callback(_stream, u8"Missing closing round bracket for condition");
						return ast_nodes::node_ref();
					}
					auto [new_expr_ref, new_expr] = _result.create_node<ast_nodes::conditional_expression>();
					bool ending_bracket_consumed = false;
					while (true) {
						// numbered capture
						if (auto id = _parse_capture_index()) {
							new_expr.condition.emplace<
								ast_nodes::conditional_expression::numbered_capture_available
							>().index = id.value();
							break;
						}

						// DEFINE
						if (_check_prefix(U"DEFINE")) {
							new_expr.condition.emplace<ast_nodes::conditional_expression::define>();
							break;
						}

						// assertion
						if (_stream.peek() == U'?') {
							std::size_t tmp_options_pushed = 0;
							if (auto cond = _parse_round_brackets_group(tmp_options_pushed)) {
								ending_bracket_consumed = true;
								if (_result.get_node(cond).is<ast_nodes::complex_assertion>()) {
									new_expr.condition.emplace<
										ast_nodes::conditional_expression::complex_assertion
									>().node = cond;
								} else {
									on_error_callback(
										_stream, u8"Failed to parse assertion for conditional expression"
									);
								}
							} else {
								on_error_callback(_stream, u8"Failed to parse assertion for conditional expression");
							}
							for (; tmp_options_pushed > 0; --tmp_options_pushed) {
								_option_stack.pop();
							}
							break;
						}

						{ // named capture
							codepoint termination = U')';
							switch (_stream.peek()) {
							case U'<':
								_stream.take();
								termination = U'>';
								break;
							case U'\'':
								_stream.take();
								termination = U'\'';
								break;
							default: // no delimiters
								break;
							}
							auto &cond = new_expr.condition.emplace<
								ast_nodes::conditional_expression::named_capture_available
							>();
							while (!_stream.empty() && _stream.peek() != termination) {
								cond.name.push_back(_stream.take());
							}
							if (_stream.empty()) {
								on_error_callback(_stream, u8"Missing ending delimiter for named capture condition");
							} else if (termination != U')') {
								_stream.take();
							}
							break;
						}
					}
					// consume ending bracket
					if (!ending_bracket_consumed) {
						if (!_stream.empty() && _stream.peek() == U')') {
							_stream.take();
						} else {
							on_error_callback(_stream, u8"Missing closing round bracket for condition");
						}
					}

					// parse expressions
					auto [result_ref, result] = _parse_subexpression(
						ast_nodes::subexpression::type::non_capturing, U')'
					);
					if (result.nodes.size() == 1 && _result.get_node(result.nodes[0]).is<ast_nodes::alternative>()) {
						// two branches
						auto &alt = std::get<ast_nodes::alternative>(_result.get_node(result.nodes[0]).value);
						if (alt.alternatives.size() == 2) {
							new_expr.if_true = alt.alternatives[0];
							new_expr.if_false = alt.alternatives[1];
						} else {
							// TODO error
							new_expr.if_true = result_ref;
						}
					} else {
						// single branch
						new_expr.if_true = result_ref;
					}
					return new_expr_ref;
				}

			// recursion
			case U'R':
				{
					_stream.take();
					if (_stream.empty() || _stream.take() != U')') {
						// TODO error
					}
					auto [result_ref, result] = _result.create_node<ast_nodes::numbered_subroutine>();
					result.index = 0;
					return result_ref;
				}

			// named subroutine
			case U'&':
				{
					_stream.take();
					auto [result_ref, result] = _result.create_node<ast_nodes::named_subroutine>();
					while (true) {
						if (_stream.empty()) {
							// TODO error
							break;
						}
						codepoint cp = _stream.take();
						if (cp == U')') {
							break;
						}
						result.name.push_back(cp);
					}
					return result_ref;
				}

			// TODO

			default:
				if (auto index = _parse_capture_index()) {
					// numbered subroutine
					if (_stream.empty() || _stream.take() != U')') {
						// TODO error
					}
					auto [result_ref, result] = _result.create_node<ast_nodes::numbered_subroutine>();
					result.index = index.value();
					return result_ref;
				} else {
					// enable/disabe features midway through the expression
					bool enable_feature = true;
					bool allow_disable = true;
					auto &opts = _push_options();
					++options_pushed;
					if (!_stream.empty() && _stream.peek() == U'^') {
						allow_disable = false;
						opts = options();
					}
					while (!_stream.empty()) {
						switch (_stream.take()) {
						case U'i': // caseless
							opts.case_insensitive = enable_feature;
							break;
						case U'm':
							opts.multiline = enable_feature;
							break;
						case U'n':
							opts.no_auto_capture = enable_feature;
							break;
						case U's':
							opts.dot_all = enable_feature;
							break;
						case U'x':
							if (!_stream.empty() && _stream.peek() == U'x') {
								// extended_more
								_stream.take();
								opts.extended_more = enable_feature;
							}
							opts.extended = enable_feature;
							break;

						case U'-':
							if (!enable_feature) {
								// TODO error
							}
							enable_feature = false;
							break;

						case U')':
							return ast_nodes::node_ref();
						case U':':
							// syntax sugar - parse until the end for a subexpression
							{
								auto result = _parse_subexpression(ast_nodes::subexpression::type::non_capturing, U')');
								// the options only affect this subexpression, not the outer one that contains it
								--options_pushed;
								_pop_options();
								return result.first;
							}
						}
					}
					on_error_callback(_stream, u8"Missing closing round bracket for options");
					return ast_nodes::node_ref();
				}
			}
			break;

		case U'*':
			{
				_stream.take();
				bool handled = false;
				bool has_mark = false;
				std::u8string command;
				while (true) {
					if (_stream.empty()) {
						on_error_callback(_stream, u8"Missing closing round bracket");
						break;
					}
					codepoint cp = _stream.take();
					if (cp == U')') {
						break;
					} else if (cp == U':') {
						if (command == u8"atomic") {
							handled = true;
							expr_type = ast_nodes::subexpression::type::atomic;
						} else if (command == u8"positive_lookahead" || command == u8"pla") {
							handled = true;
							is_complex_assertion = true;
							complex_assertion_backward = false;
							complex_assertion_negative = false;
						} else if (command == u8"negative_lookahead" || command == u8"nla") {
							handled = true;
							is_complex_assertion = true;
							complex_assertion_backward = false;
							complex_assertion_negative = true;
						} else if (command == u8"positive_lookbehind" || command == u8"plb") {
							handled = true;
							is_complex_assertion = true;
							complex_assertion_backward = true;
							complex_assertion_negative = false;
						} else if (command == u8"negative_lookbehind" || command == u8"nlb") {
							handled = true;
							is_complex_assertion = true;
							complex_assertion_backward = true;
							complex_assertion_negative = true;
						} else {
							has_mark = true;
						}
						break;
					} else {
						auto encoded = encodings::utf8::encode_codepoint(cp);
						command.append(reinterpret_cast<const char8_t*>(encoded.data()), encoded.size());
					}
				}
				if (!handled) { // parse verb or option
					codepoint_string mark;
					if (has_mark) {
						while (true) {
							if (_stream.empty()) {
								on_error_callback(_stream, u8"Missing closing round bracket");
								break;
							}
							codepoint cp = _stream.take();
							if (cp == U')') {
								break;
							}
							mark.push_back(cp);
						}
					}
					if (command == u8"F" || command == u8"FAIL") {
						auto [node_ref, node] = _result.create_node<ast_nodes::verbs::fail>();
						node.mark = std::move(mark);
						return node_ref;
					} else if (command == u8"ACCEPT") {
						auto [node_ref, node] = _result.create_node<ast_nodes::verbs::accept>();
						node.mark = std::move(mark);
						return node_ref;
					} else if (command == u8"" || command == u8"MARK") {
						auto [node_ref, node] = _result.create_node<ast_nodes::verbs::mark>();
						node.mark = std::move(mark);
						return node_ref;
					}
					// TODO other verbs
					// TODO error
					return ast_nodes::node_ref();
				}
			}
			break;
		}
		if (is_complex_assertion) {
			auto [result_ref, result] = _result.create_node<ast_nodes::complex_assertion>();
			result.negative = complex_assertion_negative;
			result.backward = complex_assertion_backward;
			result.non_atomic = complex_assertion_non_atomic;
			result.expression = _parse_subexpression(ast_nodes::subexpression::type::non_capturing, U')').first;
			return result_ref;
		}
		// otherwise, parse it as a subexpression
		std::size_t capture_id = 0;
		if (expr_type == ast_nodes::subexpression::type::normal) {
			// gather capture id before parsing, so we have correct numbers for all groups inside
			capture_id = _capture_id_stack.top();
			++_capture_id_stack.top();
		}
		auto [result_ref, result] = _parse_subexpression(expr_type, U')');
		if (expr_type == ast_nodes::subexpression::type::normal) {
			result.capture_name = std::move(capture_name);
			result.capture_index = capture_id;
		}
		return result_ref;
	}

	template <typename Stream> inline ast_nodes::node_ref parser<Stream>::_pop_last_element_of_subexpression(
		ast_nodes::subexpression &expr
	) {
		if (expr.nodes.empty()) {
			on_error_callback(_stream, u8"No subject for repetition");
			return ast_nodes::node_ref();
		}
		auto &last_node = _result.get_node(expr.nodes.back());
		if (last_node.is<ast_nodes::literal>()) {
			auto &lit = std::get<ast_nodes::literal>(last_node.value);
			assert_true_logical(!lit.contents.empty(), "empty literal node");
			if (lit.contents.size() > 1) {
				auto [res_lit_ref, res_lit] = _result.create_node<ast_nodes::literal>();
				res_lit.contents.push_back(lit.contents.back());
				res_lit.case_insensitive = lit.case_insensitive;
				lit.contents.pop_back();
				return res_lit_ref;
			}
			// if the literal only contains one character, then we just pop and return the entire node instead
		} else if (last_node.is<ast_nodes::repetition>()) {
			on_error_callback(_stream, u8"Invalid subject for repetition");
			return ast_nodes::node_ref();
		}
		auto result = expr.nodes.back();
		expr.nodes.pop_back();
		return result;
	}

	template <typename Stream> inline std::pair<
		ast_nodes::node_ref, ast_nodes::subexpression&
	> parser<Stream>::_parse_subexpression(
		ast_nodes::subexpression::type type, codepoint terminate, _alternative_context *alt_context
	) {
		std::size_t options_pushed = 0;
		auto [result_ref, result] = _result.create_node<ast_nodes::subexpression>();
		result.subexpr_type = type;
		while (!_stream.empty()) {
			codepoint cp = _stream.take();
			if (cp == terminate) {
				break;
			}
			switch (cp) {
			// subexpression, alternatives, options
			case U'(':
				{
					if (auto node = _parse_round_brackets_group(options_pushed)) {
						result.nodes.emplace_back(node);
					}
					break;
				}
			// alternatives (return early)
			case U'|':
				if (alt_context == nullptr) {
					// parse whatever remains using _parse_alternative()
					ast_nodes::subexpression::type original_type = result.subexpr_type;
					result.subexpr_type = ast_nodes::subexpression::type::non_capturing;
					auto [alt_ref, alt] = _parse_alternative(result_ref, terminate, options_pushed, original_type);
					// we want to keep the type of the outer subexpression
					auto [wrap_ref, wrap] = _result.create_node<ast_nodes::subexpression>();
					wrap.subexpr_type = type;
					wrap.nodes.emplace_back(alt_ref);
					return { wrap_ref, wrap };
				} else {
					// otherwise, return the subexpression and set it to continue
					alt_context->continues = true;
					alt_context->pushed_options += options_pushed;
					return { result_ref, result };
				}

			// character class
			case U'[':
				result.nodes.emplace_back(_parse_square_brackets_character_class().first);
				break;
			case U'.':
				{
					auto [char_cls_ref, char_cls] = _result.create_node<ast_nodes::character_class>();
					char_cls.is_negate = true;
					if (!_options().dot_all) {
						char_cls.ranges.ranges.emplace_back(codepoint_range(U'\r'));
						char_cls.ranges.ranges.emplace_back(codepoint_range(U'\n'));
						char_cls.ranges.sort_and_compact();
					}
					result.nodes.emplace_back(char_cls_ref);
				}
				break;

			// anchors
			case U'^':
				{
					auto [node_ref, node] = _result.create_node<ast_nodes::simple_assertion>();
					node.assertion_type =
						_options().multiline ?
						ast_nodes::simple_assertion::type::line_start :
						ast_nodes::simple_assertion::type::subject_start;
					result.nodes.emplace_back(node_ref);
				}
				break;
			case U'$':
				{
					auto [node_ref, node] = _result.create_node<ast_nodes::simple_assertion>();
					node.assertion_type =
						_options().multiline ?
						ast_nodes::simple_assertion::type::line_end :
						ast_nodes::simple_assertion::type::subject_end_or_trailing_newline;
					result.nodes.emplace_back(node_ref);
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
					std::size_t rep_min = 0;
					std::size_t rep_max = 0;
					switch (cp) {
					case U'{':
						{
							Stream checkpoint = _stream;
							if (auto times = _parse_curly_brackets_repetition()) {
								rep_min = times->first;
								rep_max = times->second;
							} else {
								// failed to parse range; interpret it as a literal
								_stream = std::move(checkpoint);
								_append_literal(result, codepoint_string(1, U'{'), _options().case_insensitive);
								continue; // outer loop
							}
						}
						break;
					case U'?':
						rep_min = 0;
						rep_max = 1;
						break;
					case U'*':
						rep_min = 0;
						rep_max = ast_nodes::repetition::no_limit;
						break;
					case U'+':
						rep_min = 1;
						rep_max = ast_nodes::repetition::no_limit;
						break;
					}
					if (auto rep_subject = _pop_last_element_of_subexpression(result)) {
						auto [rep_ref, rep] = _result.create_node<ast_nodes::repetition>();
						rep.min = rep_min;
						rep.max = rep_max;
						rep.expression = rep_subject;
						if (!_stream.empty()) {
							if (_stream.peek() == U'?') {
								_stream.take();
								rep.repetition_type = ast_nodes::repetition::type::lazy;
							} else if (_stream.peek() == U'+') {
								_stream.take();
								rep.repetition_type = ast_nodes::repetition::type::posessed;
							}
						}
						result.nodes.emplace_back(rep_ref);
					} else {
						// TODO error
					}
				}
				break;
					
			// literal or escaped sequence
			case U'\\':
				if (!_stream.empty() && _stream.peek() == U'Q') {
					// treat everything between this and \E as a string literal
					_stream.take();
					codepoint_string lit;
					while (true) {
						if (_stream.empty()) {
							// TODO error
							break;
						}
						codepoint ch = _stream.take();
						if (ch == U'\\') { // check for \E and break
							if (!_stream.empty() && _stream.peek() == U'E') {
								_stream.take();
								break;
							}
						}
						lit.push_back(ch);
					}
					_append_literal(result, std::move(lit), _options().case_insensitive);
				} else {
					// otherwise parse the literal/character class
					auto escaped = _parse_escaped_sequence(_escaped_sequence_context::subexpression);
					if (std::holds_alternative<codepoint_string>(escaped.value)) {
						auto &str = std::get<codepoint_string>(escaped.value);
						_append_literal(result, std::move(str), _options().case_insensitive);
					} else {
						auto node_ref = _result.create_node();
						std::move(escaped).into_node(_result.get_node(node_ref), _options());
						result.nodes.emplace_back(node_ref);
					}
				}
				break;
			default:
				if (_options().extended) {
					if (tables::extended_mode_whitespaces().contains(cp)) {
						continue;
					}
					if (cp == U'#') { // consume comment
						auto &stream = _stream;
						while (!stream.empty() && (stream.peek() != U'\r' && stream.peek() != U'\n')) {
							stream.take();
						}
						consume_line_ending(stream);
						continue;
					}
				}
				_append_literal(result, codepoint_string(1, cp), _options().case_insensitive);
				break;
			}
		}
		for (std::size_t i = 0; i < options_pushed; ++i) {
			_pop_options();
		}
		if (result.subexpr_type == ast_nodes::subexpression::type::duplicate) {
			// if we're here, it means that this duplicate group does not actually have any alternatives, in which
			// case we should pop the index pushed by _parse_round_brackets_group()
			std::size_t id = _capture_id_stack.top();
			_capture_id_stack.pop();
			_capture_id_stack.top() = id;
		}
		return { result_ref, result };
	}
}
