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

	template <typename Stream> inline parser<Stream>::_escaped_sequence_node parser<Stream>::_parse_escaped_sequence(
		_escaped_sequence_context ctx
	) {
		if (_stream.empty()) {
			// TODO error
			return _escaped_sequence_node();
		}
		codepoint cp = _stream.take();
		switch (cp) {
		// octal character code
		case U'0':
			return ast::nodes::literal::from_codepoint(_parse_numeric_value<codepoint>(8, 2));
		case U'o':
			{
				if (_stream.empty() || _stream.peek() != U'{') {
					return ast::nodes::error(); // no starting bracket
				}
				_stream.take();
				auto value = _parse_numeric_value<codepoint>(8);
				if (_stream.empty() || _stream.peek() != U'}') {
					return ast::nodes::error(); // no ending bracket
				}
				_stream.take();
				return ast::nodes::literal::from_codepoint(value);
			}

		// hexadecimal character code
		case U'x':
			{
				if (_stream.empty() || _stream.peek() != U'{') {
					return ast::nodes::literal::from_codepoint(_parse_numeric_value<codepoint>(16, 2));
				} else {
					// \x{...}: if parsing fails, restore checkpoint
					Stream checkpoint = _stream;
					_stream.take();
					auto value = _parse_numeric_value<codepoint>(16);
					if (_stream.empty() || _stream.peek() != U'}') {
						// no ending bracket; resume parsing from the beginning bracket
						_stream = std::move(checkpoint);
						return ast::nodes::literal::from_codepoint(0);
					}
					_stream.take();
					return ast::nodes::literal::from_codepoint(value);
				}
			}

		// control-x
		case U'c':
			{
				if (_stream.empty()) {
					return ast::nodes::error();
				}
				codepoint val = _stream.take();
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
			if (ctx == _escaped_sequence_context::subexpression) {
				std::size_t idx = cp - U'0';
				if (!_stream.empty()) {
					cp = _stream.peek();
					if (cp >= U'0' && cp <= U'9') {
						_stream.take();
						idx = idx * 10 + (cp - U'0');
					}
				}
				return ast::nodes::numbered_backreference(idx, _options().case_insensitive);
			}
			break;
		case U'g':
			if (ctx == _escaped_sequence_context::subexpression) {
				std::size_t index = 0;
				if (!_stream.empty() && _stream.peek() == U'{') {
					_stream.take();
					if (_stream.empty()) {
						// TODO error
						break;
					}
					if (auto id = _parse_capture_index()) { // numbered backreference
						if (_stream.empty() || _stream.peek() != U'}') {
							// TODO error
						} else {
							_stream.take();
						}
						index = id.value();
					} else { // named backreference
						codepoint_string res;
						while (true) {
							if (_stream.empty()) {
								// TODO error
								break;
							}
							codepoint cur_cp = _stream.take();
							if (cur_cp == U'}') {
								break;
							}
							res.push_back(cur_cp);
						}
						return ast::nodes::named_backreference(std::move(res), _options().case_insensitive);
					}
				} else {
					if (auto id = _parse_capture_index()) {
						index = id.value();
					} else {
						// TODO error
					}
				}
				return ast::nodes::numbered_backreference(index, _options().case_insensitive);
			}
			break;
		case U'k':
			if (ctx == _escaped_sequence_context::subexpression) {
				if (_stream.empty()) {
					// TODO error
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
					// TODO error
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
				return ast::nodes::named_backreference(std::move(capture_name), _options().case_insensitive);
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
				ast::nodes::character_class result;
				result.ranges = unicode::unicode_data::cache::get_codepoints_in_category(
					unicode::general_category_index::decimal_number
				);
				return result;
			}
		case U'D': // not decimal digit
			{
				ast::nodes::character_class result;
				result.ranges = unicode::unicode_data::cache::get_codepoints_in_category(
					unicode::general_category_index::decimal_number
				);
				result.is_negate = true;
				return result;
			}
		case U'h': // horizontal white space
			{
				ast::nodes::character_class result;
				result.ranges = tables::horizontal_whitespaces();
				return result;
			}
		case U'H': // not horizontal white space
			{
				ast::nodes::character_class result;
				result.ranges = tables::horizontal_whitespaces();
				result.is_negate = true;
				return result;
			}
		case U's': // white space
			{
				ast::nodes::character_class result;
				result.ranges = unicode::property_list::get_cached().white_space;
				return result;
			}
		case U'S': // not white space
			{
				ast::nodes::character_class result;
				result.ranges = unicode::property_list::get_cached().white_space;
				result.is_negate = true;
				return result;
			}
		case U'v': // vertical white space
			{
				ast::nodes::character_class result;
				result.ranges = tables::vertical_whitespaces();
				return result;
			}
		case U'V': // not vertical white space
			{
				ast::nodes::character_class result;
				result.ranges = tables::vertical_whitespaces();
				result.is_negate = true;
				return result;
			}
		case U'w': // word
			{
				ast::nodes::character_class result;
				result.ranges = tables::word_characters();
				return result;
			}
		case U'W': // not word
			{
				ast::nodes::character_class result;
				result.ranges = tables::word_characters();
				result.is_negate = true;
				return result;
			}

		// anchor
		case U'A':
			if (ctx != _escaped_sequence_context::character_class) {
				ast::nodes::simple_assertion result;
				result.assertion_type = ast::nodes::simple_assertion::type::subject_start;
				return result;
			}
			break;
		case U'Z':
			if (ctx != _escaped_sequence_context::character_class) {
				ast::nodes::simple_assertion result;
				result.assertion_type = ast::nodes::simple_assertion::type::subject_end_or_trailing_newline;
				return result;
			}
			break;
		case U'z':
			if (ctx != _escaped_sequence_context::character_class) {
				ast::nodes::simple_assertion result;
				result.assertion_type = ast::nodes::simple_assertion::type::subject_end;
				return result;
			}
			break;
		case U'b':
			{
				if (ctx == _escaped_sequence_context::character_class) {
					return ast::nodes::literal::from_codepoint(0x08);
				}
				ast::nodes::character_class_assertion result;
				result.boundary = true;
				result.char_class.case_insensitive = _options().case_insensitive;
				result.char_class.ranges = tables::word_characters();
				return result;
			}
		case U'B':
			if (ctx != _escaped_sequence_context::character_class) {
				ast::nodes::character_class_assertion result;
				result.boundary = false;
				result.char_class.case_insensitive = _options().case_insensitive;
				result.char_class.ranges = tables::word_characters();
				return result;
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

		// nothing - we handle \Q\E elsewhere
		case U'E':
			return ast::nodes::literal();

		// special
		case U'K':
			if (ctx != _escaped_sequence_context::character_class) {
				return ast::nodes::match_start_override();
			}
			break;
		}

		if (cp >= U'1' && cp <= U'7') { // octal character code or backreference
			if (ctx == _escaped_sequence_context::subexpression) {
				if (_stream.empty()) { // single digit - must be a backreference
					return ast::nodes::numbered_backreference(cp - U'0', _options().case_insensitive);
				}
				codepoint next_cp = _stream.peek();
				if (next_cp < U'0' || next_cp > U'9') { // single digit - must be a backreference
					return ast::nodes::numbered_backreference(cp - U'0', _options().case_insensitive);
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
					// TODO what happens when
					//      - we reference a capture while it's in progress?
					//      - we have duplicate capture indices?
					return ast::nodes::numbered_backreference(index, _options().case_insensitive);
				}
				_stream = std::move(checkpoint); // it's a hexadecimal codepoint; restore checkpoint
			}
			// read up to three octal digits, but we've read one already
			return ast::nodes::literal::from_codepoint(_parse_numeric_value<codepoint>(8, 2, cp - U'0'));
		}
		return ast::nodes::literal::from_codepoint(cp);
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
	> inline ast::nodes::character_class parser<Stream>::_parse_square_brackets_character_class() {
		ast::nodes::character_class result;
		if (_stream.empty()) {
			// TODO error - early termination
			return result;
		}
		if (_stream.peek() == U'^') {
			result.is_negate = true;
			_stream.take();
		}
		// if we run into \Q, this function simply returns an empty literal
		auto parse_char = [&]() -> _escaped_sequence_node {
			while (!_stream.empty()) {
				codepoint cp = _stream.take();
				if (_options().extended_more) {
					while (cp == U' ' || cp == U'\t') {
						if (_stream.empty()) {
							return ast::nodes::error();
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
						return ast::nodes::literal();
					}
					// an isolated \E - ignore it
					if (_stream.peek() == U'E') {
						_stream.take();
						continue;
					}
					return _parse_escaped_sequence(_escaped_sequence_context::character_class);
				}
				return ast::nodes::literal::from_codepoint(cp);
			}
			return ast::nodes::literal();
		};
		bool force_literal = false; // whether we're in a \Q \E sequence
		while (true) {
			if (_stream.empty()) {
				// TODO error
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
			// TODO check for special character classes
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
			if (std::holds_alternative<ast::nodes::character_class>(elem)) {
				const auto &list = std::get<ast::nodes::character_class>(elem);
				auto cls = list.is_negate ? list.ranges.get_negated() : list.ranges;
				result.ranges.ranges.insert(result.ranges.ranges.end(), cls.ranges.begin(), cls.ranges.end());
			} else if (std::holds_alternative<ast::nodes::literal>(elem)) {
				auto &literal = std::get<ast::nodes::literal>(elem).contents;
				if (literal.empty()) { // we've ran into a \Q
					force_literal = true;
					continue;
				}
				auto &range = result.ranges.ranges.emplace_back(literal.front(), literal.front());
				if (!_stream.empty() && _stream.peek() == U'-') { // parse a range
					_stream.take();
					if (_stream.empty()) {
						// TODO error
						break;
					}
					if (_stream.peek() == U']') {
						// special case: we want to terminate here instead of treating it as a range
						_stream.take();
						result.ranges.ranges.emplace_back(U'-', U'-');
						break;
					}
					auto next_elem = parse_char();
					if (std::holds_alternative<ast::nodes::literal>(next_elem)) {
						auto lit = std::get<ast::nodes::literal>(next_elem);
						if (!lit.contents.empty()) {
							range.last = lit.contents.front();
							if (range.last < range.first) {
								// TODO error
								std::swap(range.first, range.last);
							}
							continue;
						}
					}
					// we're unable to parse a full range - add the dash to the character class
					result.ranges.ranges.emplace_back(U'-', U'-');
					if (std::holds_alternative<ast::nodes::character_class>(next_elem)) {
						const auto &cls = std::get<ast::nodes::character_class>(next_elem);
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
		return result;
	}

	template <typename Stream> inline ast::nodes::alternative parser<Stream>::_parse_alternative(
		ast::nodes::subexpression first_alternative, codepoint terminate,
		std::size_t pushed_options, ast::nodes::subexpression::type expr_type
	) {
		std::size_t max = 0;

		ast::nodes::alternative result;
		result.alternatives.emplace_back(std::move(first_alternative));
		_alternative_context context;
		context.pushed_options = pushed_options;
		do {
			if (expr_type == ast::nodes::subexpression::type::duplicate) {
				// save & reset group index
				max = std::max(max, _capture_id_stack.top());
				_capture_id_stack.pop();
				_capture_id_stack.emplace(_capture_id_stack.top());
			}
			context.continues = false;
			_parse_subexpression(result.alternatives.emplace_back(), terminate, &context);
			if (!context.continues) {
				break;
			}
		} while (!_stream.empty());
		// pop all pused options (that are shared across the alternatives)
		for (std::size_t i = 0; i < context.pushed_options; ++i) {
			_pop_options();
		}
		// handle duplicate indices
		if (expr_type == ast::nodes::subexpression::type::duplicate) {
			// update group index
			max = std::max(max, _capture_id_stack.top());
			_capture_id_stack.pop();
			_capture_id_stack.top() = max;
		}
		return result;
	}

	template <typename Stream> inline void parser<Stream>::_parse_round_brackets_group(
		ast::nodes::subexpression &result, std::size_t &options_pushed
	) {
		if (_stream.empty()) {
			// TODO error
			return;
		}

		// is_complex_assertion is used to indicate whether we need to continue parsing the assertion, and the
		// attributes of the assertion are stored in complex_ass
		bool is_complex_assertion = false;
		ast::nodes::complex_assertion complex_ass;
		// otherwise, the result depends on is_subexpresion
		bool is_subexpression = true;
		auto expr_type = ast::nodes::subexpression::type::normal;
		codepoint_string capture_name;
		// determine if this is a named capture, assertion, etc.
		switch (_stream.peek()) {
		case U'?':
			_stream.take();
			if (_stream.empty()) {
				// TODO fail
				return;
			}
			switch (_stream.peek()) {
			case U':': // non-capturing subexpression
				_stream.take();
				expr_type = ast::nodes::subexpression::type::non_capturing;
				break;
			case U'|': // duplicate subexpression numbers
				_stream.take();
				expr_type = ast::nodes::subexpression::type::duplicate;
				// save the subexpression number
				_capture_id_stack.emplace(_capture_id_stack.top());
				break;
			case U'>': // atomic subexpression
				_stream.take();
				expr_type = ast::nodes::subexpression::type::atomic;
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
								complex_ass.backward = true;
								complex_ass.negative = false;
								break;
							case U'!': // negative lookbehind
								_stream.take();
								is_complex_assertion = true;
								complex_ass.backward = true;
								complex_ass.negative = true;
								break;
							}
							if (is_complex_assertion) {
								break; // parse as an assertion instead
							}
						}
					}

					if (start == U'P') {
						if (_stream.empty()) {
							// TODO error
							break;
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
							// TODO error
							break;
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
				complex_ass.backward = false;
				complex_ass.negative = false;
				break;
			case U'!':
				_stream.take();
				is_complex_assertion = true;
				complex_ass.backward = false;
				complex_ass.negative = true;
				break;

			// conditional subexpression
			case U'(':
				{
					_stream.take();
					is_subexpression = false;
					if (_stream.empty()) {
						// TODO error
						break;
					}
					auto &new_expr = result.nodes.emplace_back().value.emplace<ast::nodes::conditional_expression>();
					while (true) {
						// numbered capture
						if (auto id = _parse_capture_index()) {
							new_expr.condition.emplace<
								ast::nodes::conditional_expression::numbered_capture_available
							>().index = id.value();
							break;
						}

						// DEFINE
						if (_check_prefix(U"DEFINE")) {
							new_expr.condition.emplace<ast::nodes::conditional_expression::define>();
							break;
						}

						// assertion
						if (_stream.peek() == U'?') {
							ast::nodes::subexpression temp_expr;
							std::size_t tmp_options_pushed = 0;
							_parse_round_brackets_group(temp_expr, tmp_options_pushed);
							if (temp_expr.nodes.size() > 0 && temp_expr.nodes[0].is<ast::nodes::complex_assertion>()) {
								auto &assertion = std::get<ast::nodes::complex_assertion>(temp_expr.nodes[0].value);
								new_expr.condition.emplace<ast::nodes::complex_assertion>(std::move(assertion));
							} else {
								// TODO error
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
								ast::nodes::conditional_expression::named_capture_available
							>();
							while (!_stream.empty() && _stream.peek() != termination) {
								cond.name.push_back(_stream.take());
							}
							if (!_stream.empty() && termination != U')') {
								_stream.take();
							}
							break;
						}
					}
					// consume ending bracket
					if (!_stream.empty() && _stream.peek() == U')') {
						_stream.take();
					} else {
						// TODO error
					}

					// parse expressions
					ast::nodes::subexpression temp_expr;
					_parse_subexpression(temp_expr, U')');
					if (!temp_expr.nodes.empty() && temp_expr.nodes[0].is<ast::nodes::alternative>()) {
						// two branches
						if (temp_expr.nodes.size() > 1) {
							// TODO error
						}
						auto &alt = std::get<ast::nodes::alternative>(temp_expr.nodes[0].value);
						if (alt.alternatives.size() == 2) {
							new_expr.if_true = std::move(alt.alternatives[0]);
							new_expr.if_false = std::move(alt.alternatives[1]);
						} else {
							// TODO error
						}
					} else {
						// single branch
						new_expr.if_true = std::move(temp_expr);
					}
				}
				break;

			// recursion
			case U'R':
				_stream.take();
				is_subexpression = false;
				if (_stream.empty() || _stream.take() != U')') {
					// TODO error
				}
				result.nodes.emplace_back().value.emplace<ast::nodes::numbered_subroutine>(0);
				break;

			// named subroutine
			case U'&':
				{
					_stream.take();
					is_subexpression = false;
					auto &ref = result.nodes.emplace_back().value.emplace<ast::nodes::named_subroutine>();
					while (true) {
						if (_stream.empty()) {
							// TODO error
							break;
						}
						codepoint cp = _stream.take();
						if (cp == U')') {
							break;
						}
						ref.name.push_back(cp);
					}
				}
				break;

			// TODO

			default:
				is_subexpression = false;
				if (auto index = _parse_capture_index()) {
					// subroutine
					if (_stream.empty() || _stream.take() != U')') {
						// TODO error
					}
					result.nodes.emplace_back().value.emplace<ast::nodes::numbered_subroutine>(index.value());
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
						bool stop = false;
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
							// finish - continue parsing this subexpression
							stop = true;
							break;
						case U':':
							// syntax sugar - parse until the end for a subexpression
							{
								stop = true;
								auto &new_expr =
									result.nodes.emplace_back().value.emplace<ast::nodes::subexpression>();
								new_expr.subexpr_type = ast::nodes::subexpression::type::non_capturing;
								_parse_subexpression(new_expr, U')');
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
			break;

		case U'*':
			{
				_stream.take();
				is_subexpression = false;
				std::u8string command;
				while (!_stream.empty()) {
					codepoint cp = _stream.take();
					if (cp == U')') {
						break;
					} else if (cp == U':') {
						if (command == u8"atomic") {
							is_subexpression = true;
							expr_type = ast::nodes::subexpression::type::atomic;
						} else if (command == u8"positive_lookahead" || command == u8"pla") {
							is_complex_assertion = true;
							complex_ass.backward = false;
							complex_ass.negative = false;
						} else if (command == u8"negative_lookahead" || command == u8"nla") {
							is_complex_assertion = true;
							complex_ass.backward = false;
							complex_ass.negative = true;
						} else if (command == u8"positive_lookbehind" || command == u8"plb") {
							is_complex_assertion = true;
							complex_ass.backward = true;
							complex_ass.negative = false;
						} else if (command == u8"negative_lookbehind" || command == u8"nlb") {
							is_complex_assertion = true;
							complex_ass.backward = true;
							complex_ass.negative = true;
						} else {
							// TODO error
							return;
						}
						command.clear();
						break;
					} else {
						auto encoded = encodings::utf8::encode_codepoint(cp);
						command.append(reinterpret_cast<const char8_t*>(encoded.data()), encoded.size());
					}
				}
				if (!command.empty()) { // parse special option
					// TODO
				}
			}
			break;
		}
		if (is_complex_assertion) {
			_parse_subexpression(complex_ass.expression, U')');
			result.nodes.emplace_back().value.emplace<ast::nodes::complex_assertion>() = std::move(complex_ass);
		} else if (is_subexpression) {
			auto &new_expr = result.nodes.emplace_back().value.emplace<ast::nodes::subexpression>();
			new_expr.subexpr_type = expr_type;
			if (new_expr.subexpr_type == ast::nodes::subexpression::type::normal) {
				new_expr.capture_index = _capture_id_stack.top();
				++_capture_id_stack.top();
			}
			new_expr.capture_name = std::move(capture_name);
			_parse_subexpression(new_expr, U')');
		}
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
			max = ast::nodes::repetition::no_limit;
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

	template <typename Stream> inline ast::nodes::subexpression parser<Stream>::_pop_last_element_of_subexpression(
		ast::nodes::subexpression &expr
	) {
		ast::nodes::subexpression result;
		auto &last_node = expr.nodes.back();
		if (last_node.is<ast::nodes::subexpression>()) {
			result = std::move(std::get<ast::nodes::subexpression>(last_node.value));
			expr.nodes.pop_back();
		} else {
			auto &result_node = result.nodes.emplace_back();
			if (last_node.is<ast::nodes::literal>()) {
				auto &lit = std::get<ast::nodes::literal>(last_node.value);
				auto &res_lit = result_node.value.emplace<ast::nodes::literal>();
				res_lit.contents.push_back(lit.contents.back());
				res_lit.case_insensitive = lit.case_insensitive;
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

	template <typename Stream> inline void parser<Stream>::_parse_subexpression(
		ast::nodes::subexpression &result, codepoint terminate, _alternative_context *alt_context
	) {
		std::size_t options_pushed = 0;
		while (!_stream.empty()) {
			codepoint cp = _stream.take();
			if (cp == terminate) {
				break;
			}
			switch (cp) {
			// subexpression, alternatives, options
			case U'(':
				_parse_round_brackets_group(result, options_pushed);
				break;
			// alternatives
			case U'|':
				if (alt_context == nullptr) {
					// parse whatever remains using _parse_alternative()
					// here we want to keep the properties, capture index, etc. of the outer subexpression - so
					// create a new non-capturing subexpression and move all the parsed nodes into it
					ast::nodes::subexpression first_alt;
					first_alt.nodes = std::exchange(result.nodes, std::vector<ast::node>());
					auto alt = _parse_alternative(
						std::move(first_alt), terminate, options_pushed, result.subexpr_type
					);
					result.nodes.emplace_back().value.emplace<ast::nodes::alternative>(std::move(alt));
				} else {
					// otherwise, return the subexpression and set it to continue
					alt_context->continues = true;
					alt_context->pushed_options += options_pushed;
				}
				options_pushed = 0;
				return;

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
					auto &assertion = result.nodes.emplace_back().value.emplace<ast::nodes::simple_assertion>();
					assertion.assertion_type =
						_options().multiline ?
						ast::nodes::simple_assertion::type::line_start :
						ast::nodes::simple_assertion::type::subject_start;
				}
				break;
			case U'$':
				{
					auto &assertion = result.nodes.emplace_back().value.emplace<ast::nodes::simple_assertion>();
					assertion.assertion_type =
						_options().multiline ?
						ast::nodes::simple_assertion::type::line_end :
						ast::nodes::simple_assertion::type::subject_end_or_trailing_newline;
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
							Stream checkpoint = _stream;
							if (auto times = _parse_curly_brackets_repetition()) {
								rep.min = times->first;
								rep.max = times->second;
							} else {
								// failed to parse range; interpret it as a literal
								_stream = std::move(checkpoint);
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
					if (!_stream.empty()) {
						if (_stream.peek() == U'?') {
							_stream.take();
							rep.repetition_type = ast::nodes::repetition::type::lazy;
						} else if (_stream.peek() == U'+') {
							_stream.take();
							rep.repetition_type = ast::nodes::repetition::type::posessed;
						}
					}
					if (result.nodes.empty()) {
						// TODO error
						break;
					}
					rep.expression = _pop_last_element_of_subexpression(result);
					result.nodes.emplace_back().value.emplace<ast::nodes::repetition>(std::move(rep));
				}
				break;
					
			// literal or escaped sequence
			case U'\\':
				if (!_stream.empty() && _stream.peek() == U'Q') {
					// treat everything between this and \E as a string literal
					_stream.take();
					auto &literal = _append_literal(result, _options().case_insensitive);
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
						literal.contents.push_back(ch);
					}
				} else {
					// otherwise parse the literal/character class
					auto escaped = _parse_escaped_sequence(_escaped_sequence_context::subexpression);
					if (std::holds_alternative<ast::nodes::literal>(escaped)) {
						const auto &str = std::get<ast::nodes::literal>(escaped).contents;
						if (!str.empty()) {
							auto &lit = _append_literal(result, _options().case_insensitive);
							lit.contents.append(str);
						}
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
						auto &stream = _stream;
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
		if (result.subexpr_type == ast::nodes::subexpression::type::duplicate) {
			// if we're here, it means that this duplicate group does not actually have any alternatives, in which
			// case we should pop the index pushed by _parse_round_brackets_group()
			std::size_t id = _capture_id_stack.top();
			_capture_id_stack.pop();
			_capture_id_stack.top() = id;
		}
	}
}
