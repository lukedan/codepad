// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Regex tests using the PCRE2 test data.

#include <set>
#include <fstream>
#include <sstream>
#include <filesystem>

#include <catch2/catch.hpp>

#include <codepad/core/logging.h>
#include <codepad/core/regex/matcher.h>
#include <codepad/core/regex/matcher.inl>

namespace cp = codepad;

/// Returns whether the character is a PCRE2 non-printing character.
[[nodiscard]] bool is_non_printing_char(cp::codepoint c) {
	return c < 32 || c > 126;
}
/// Dumps a string to the given stream using the PCRE2 format.
void dump_string(std::ostream &out, const cp::codepoint_string &str) {
	out << std::setfill('0') << std::hex;
	for (auto cp : str) {
		if (is_non_printing_char(cp)) {
			if (cp <= 0xFF) {
				out << "\\x" << std::setw(2) << cp;
			} else {
				out << "\\x{" << std::setw(4) << cp << "}";
			}
		} else {
			out << static_cast<char>(cp);
		}
	}
	out << std::dec;
}

/// A string to match against and associated options.
struct test_data {
	cp::codepoint_string string; ///< The string to match agains.
	std::size_t byte_begin = 0; ///< Beginning position of this pattern.
	std::size_t byte_end = 0; ///< Ending position of this pattern.
	/// Used to indicate that there should be no matches in all following strings. This is only valid when
	/// \ref string is empty.
	bool expect_no_match = false;
};
/// A pattern and associated options.
struct pattern_data {
	cp::codepoint_string pattern; ///< The pattern.
	cp::regex::options options; ///< Matching options.
	std::size_t byte_begin = 0; ///< Beginning position of this test data entry.
	std::size_t byte_end = 0; ///< Ending position of this test data entry.
	std::size_t options_begin = 0; ///< Beginning position of pattern options.
	std::size_t options_end = 0; ///< Ending position of pattern options.
	bool aftertext = false; ///< Indicates that text after each match should be printed.
	bool subject_literal = false; ///< Indicates that subject lines are not escaped.
	/// Whether or not to dump binary code (which we don't have, so only a placeholder for now).
	bool bincode = false;
	bool hex = false; ///< Whether the pattern is expressed as hexadecimal bytes.
	bool mark = false; ///< Whether or not to print out the marker after matching finishes.

	/// Dumps the pattern string to the given output stream.
	void dump_options(std::ostream &out) const {
		bool first = true;
		auto print_bit = [&](bool enable, std::string_view sv) {
			if (enable) {
				out << sv;
				first = false;
			}
		};
		auto print_word = [&](bool enable, std::string_view sv) {
			if (enable) {
				out << (first ? "" : ",") << sv;
				first = false;
			}
		};

		print_bit(options.global, "g");
		print_bit(options.case_insensitive, "i");
		print_bit(options.multiline, "m");
		print_bit(options.dot_all, "s");
		print_bit(options.extended, "x");
		print_bit(options.extended_more, "x");
		print_bit(bincode, "B");

		print_word(hex, "hex");
		print_word(mark, "mark");
		print_word(aftertext, "aftertext");
		print_word(subject_literal, "subject_literal");
	}
};
/// A test.
struct test {
	pattern_data pattern; ///< The pattern.
	std::vector<test_data> data; ///< Test strings.
};
using stream_t = cp::regex::basic_input_stream<cp::encodings::utf8, const std::byte*>; ///< UTF-8 input stream type.
using data_types = cp::regex::data_types::small_expression; ///< Data types.
using matcher_t = cp::regex::matcher<stream_t, data_types>; /// Matcher type.

/// Fails with the given message.
void fail(const char *msg = nullptr) {
	cp::assert_true_logical(false, msg);
}
// TODO replace this with a proper character class test
/// Determines if a codepoint is a graphical char, i.e., is not blank.
[[nodiscard]] bool is_graphical_char(cp::codepoint c) {
	return c != '\n' && c != '\r' && c != '\t' && c != ' ';
}

/// Parses a pattern and its options, and consumes the following line break.
[[nodiscard]] std::optional<pattern_data> parse_pattern(stream_t &stream) {
	pattern_data result;
	while (true) {
		while (!stream.empty() && !is_graphical_char(stream.peek())) {
			stream.take();
		}
		if (stream.empty()) {
			return std::nullopt;
		}

		cp::codepoint cpt = stream.take();
		if (cpt == U'#') {
			// TODO process command
			while (!stream.empty()) {
				if (stream.peek() == U'\r' || stream.peek() == U'\n') {
					break;
				}
				stream.take();
			}
			// consume the line ending
			cp::regex::consume_line_ending(stream);
		} else {
			// read pattern
			bool is_valid_delimiter =
				cpt == U'/' || cpt == U'!' || cpt == U'"' || cpt == U'\'' || cpt == U'`' ||
				cpt == U'%' || cpt == U'&' || cpt == U'-' || cpt == U'=' || cpt == U'_' ||
				cpt == U':' || cpt == U';' || cpt == U',' || cpt == U'@' || cpt == U'~';
			if (!is_valid_delimiter) {
				INFO(
					"Invalid delimiter at position " << stream.codepoint_position() << ": " << static_cast<int>(cpt) <<
					", " << (cpt < 128 ? static_cast<char>(cpt) : '?')
				);
				fail();
			}
			result.byte_begin = stream.byte_position();
			while (true) {
				if (stream.empty()) {
					fail("Stream terminated prematurely");
				}
				result.byte_end = stream.byte_position();
				cp::codepoint next = stream.take();
				if (next == cpt) {
					break;
				} else {
					if (next == U'\\') {
						result.pattern.push_back(U'\\');
						if (stream.empty()) {
							fail("Stream terminated prematurely");
						}
						next = stream.take();
					}
					result.pattern.push_back(next);
				}
			}
			if (!stream.empty() && stream.peek() == U'\\') {
				// make the pattern end with a backslash
				stream.take();
				result.pattern.push_back(U'\\');
			}

			// read options
			result.options_begin = stream.byte_position();
			std::vector<cp::codepoint_string> options_strings;
			while (!stream.empty()) {
				if (stream.peek() == U'\r' || stream.peek() == U'\n') {
					result.options_end = stream.byte_position();
					break;
				}
				if (options_strings.empty() || stream.peek() == U',') {
					if (stream.peek() == U',') {
						stream.take();
					}
					options_strings.emplace_back();
					while (!stream.empty() && (stream.peek() == U' ' || stream.peek() == U'\t')) {
						stream.take();
					}
				} else {
					options_strings.back().push_back(stream.take());
				}
			}
			for (auto &str : options_strings) {
				while (!str.empty() && (str.back() == U' ' || str.back() == U'\t')) {
					str.pop_back();
				}
				std::u32string_view sv(reinterpret_cast<const char32_t*>(str.data()), str.size());
				if (sv == U"aftertext") {
					result.aftertext = true;
				} else if (sv == U"subject_literal") {
					result.subject_literal = true;
				} else if (sv == U"mark") {
					result.mark = true;
				} else if (sv == U"hex") {
					std::stringstream ss;
					for (cp::codepoint cp : result.pattern) {
						ss << cp::encodings::utf8::encode_codepoint_char(cp);
					}
					result.pattern.clear();
					while (true) {
						std::uint32_t b = 0;
						ss >> std::hex >> b;
						if (!ss) {
							break;
						}
						result.pattern.push_back(b);
					}
					result.hex = true;
				} else if (sv.starts_with(U"jitstack=")) {
					// ignore - we don't have JIT
				} else { // parse single-letter options
					for (std::size_t i = 0; i < str.size(); ++i) {
						switch (str[i]) {
						case U'B':
							result.bincode = true;
							break;
						case U'g':
							result.options.global = true;
							break;
						case U'i':
							result.options.case_insensitive = true;
							break;
						case U'm':
							result.options.multiline = true;
							break;
						case U's':
							result.options.dot_all = true;
							break;
						case U'x':
							result.options.extended = true;
							if (i + 1 < str.size() && str[i + 1] == U'x') {
								result.options.extended_more = true;
								++i;
							}
							break;
						default:
							/*cp::assert_true_logical(false, "invalid option");*/ // TODO
							break;
						}
					}
				}
			}
			// consume the line ending
			cp::regex::consume_line_ending(stream);
			break;
		}
	}
	return result;
}
/// Parses a string.
[[nodiscard]] std::optional<test_data> parse_data(stream_t &stream, const pattern_data &p) {
	if (stream.empty()) {
		return std::nullopt;
	}
	test_data result;
	if (p.subject_literal) {
		while (!stream.empty()) {
			if (stream.peek() == U'\r' || stream.peek() == U'\n') {
				cp::regex::consume_line_ending(stream);
				break;
			}

			if (result.string.empty()) {
				result.byte_begin = stream.byte_position();
			}
			cp::codepoint c = stream.take();
			if (is_graphical_char(c)) {
				result.byte_end = stream.byte_position();
			} else {
				if (result.string.empty()) {
					continue;
				}
			}

			result.string.push_back(c);
		}
	} else {
		std::size_t non_graphic = 0;
		while (!stream.empty()) {
			if (stream.peek() == U'\r' || stream.peek() == U'\n') {
				cp::regex::consume_line_ending(stream);
				break;
			}

			if (result.string.empty()) {
				result.byte_begin = stream.byte_position();
			}
			cp::codepoint c = stream.take();
			if (result.string.empty()) {
				if (!is_graphical_char(c)) {
					continue;
				}
				if (c == U'\\' && !stream.empty() && stream.peek() == U'=') {
					while (!stream.empty() && stream.peek() != U'\r' && stream.peek() != U'\n') {
						stream.take();
					}
					cp::regex::consume_line_ending(stream);
					result.expect_no_match = true;
					return result;
				}
			}
			if (c == U'\\') {
				// if the very last character in the line is a backslash (and there is no modifier list),
				// it is ignored
				if (stream.empty() || stream.peek() == U'\r' || stream.peek() == U'\n') {
					result.byte_end = stream.byte_position();
					cp::regex::consume_line_ending(stream);
					return result;
				}

				c = stream.take();
				switch (c) {
				case U'\\':
					c = U'\\';
					break;
				case U'a':
					c = U'\a';
					break;
				case U'b':
					c = U'\b';
					break;
				case U'e':
					c = U'\x1B';
					break;
				case U'f':
					c = U'\f';
					break;
				case U'n':
					c = U'\n';
					break;
				case U'r':
					c = U'\r';
					break;
				case U't':
					c = U'\t';
					break;
				case U'v':
					c = U'\v';
					break;

				case U'o':
					cp::assert_true_logical(!stream.empty(), "stream terminated too early");
					cp::assert_true_logical(stream.take() == U'{', "invalid octal sequence");
					c = 0;
					while (true) {
						cp::assert_true_logical(!stream.empty(), "stream terminated too early");
						cp::codepoint next_cp = stream.take();
						if (next_cp == U'}') {
							break;
						}
						cp::assert_true_logical(next_cp >= U'0' && next_cp <= U'7', "invalid octal character");
						c = c * 8 + (next_cp - U'0');
					}
					break;
				case U'x':
					c = 0;
					if (stream.empty() || stream.peek() != U'{') {
						for (std::size_t i = 0; i < 2 && !stream.empty(); ++i) {
							cp::codepoint next_cp = stream.peek();
							if (next_cp >= U'0' && next_cp <= U'9') {
								next_cp -= U'0';
							} else if (next_cp >= U'a' && next_cp <= U'f') {
								next_cp = next_cp - U'a' + 10;
							} else if (next_cp >= U'A' && next_cp <= U'F') {
								next_cp = next_cp - U'A' + 10;
							} else {
								break;
							}
							stream.take();
							c = c * 16 + next_cp;
						}
					} else {
						stream.take();
						while (true) {
							cp::assert_true_logical(!stream.empty(), "stream terminated too early");
							cp::codepoint next_cp = stream.take();
							if (next_cp == U'}') {
								break;
							}
							if (next_cp >= U'0' && next_cp <= U'9') {
								next_cp -= U'0';
							} else if (next_cp >= U'a' && next_cp <= U'f') {
								next_cp = next_cp - U'a' + 10;
							} else if (next_cp >= U'A' && next_cp <= U'F') {
								next_cp = next_cp - U'A' + 10;
							} else {
								cp::assert_true_logical(false, "invalid hexadecimal character");
							}
							c = c * 16 + next_cp;
						}
					}
					break;

				default:
					if (c >= U'0' && c <= U'7') {
						c -= U'0';
						for (std::size_t i = 0; i < 2 && !stream.empty(); ++i) {
							cp::codepoint next_cp = stream.peek();
							if (next_cp < U'0' || next_cp > U'7') {
								break;
							}
							stream.take();
							c = c * 8 + (next_cp - U'0');
						}
					}
				}
				non_graphic = result.string.size() + 1;
				result.byte_end = stream.byte_position();
			} else {
				if (is_graphical_char(c)) {
					non_graphic = result.string.size() + 1;
					result.byte_end = stream.byte_position();
				}
			}
			result.string.push_back(c);
		}
		result.string.erase(result.string.begin() + non_graphic, result.string.end());
	}
	if (result.string.empty()) {
		return std::nullopt;
	}
	return result;
}

void dump_tests(std::ostream &out, const std::vector<test> &tests) {
	auto _dump_pattern = [&](const cp::codepoint_string &str) {
		for (auto cp : str) {
			if ((cp >= 0x20 && cp <= 0x7E) || cp == U'\r' || cp == U'\n') {
				out << static_cast<char>(cp);
			} else {
				out << "\\x{" << std::hex << cp << std::dec << "}";
			}
		}
	};

	for (auto &t : tests) {
		bool no_match = false;
		for (auto &d : t.data) {
			if (no_match) {
				cp::assert_true_logical(d.expect_no_match, "incorrect expect_no_match flag");
			} else {
				if (d.expect_no_match) {
					no_match = true;
					out << "\\= Expect no match\n";
				}
			}
			out << "    ";
			dump_string(out, d.string);
			out << "\n";
		}
		out << "\n";
	}
}

/// Parses the tests.
[[nodiscard]] std::vector<test> parse_tests(const std::byte *contents, std::size_t size) {
	stream_t stream(contents, contents + size);
	std::vector<test> result;
	while (!stream.empty()) {
		test current_test;
		if (auto patt = parse_pattern(stream)) {
			current_test.pattern = patt.value();
		} else {
			break;
		}
		bool expect_no_match = false;
		while (!stream.empty()) {
			auto data = parse_data(stream, current_test.pattern);
			if (data) {
				if (data->string.empty()) {
					if (data->expect_no_match) {
						expect_no_match = true;
						continue;
					}
				}
			} else {
				break;
			}
			data->expect_no_match = expect_no_match;
			current_test.data.emplace_back(std::move(data.value()));
		}
		result.emplace_back(std::move(current_test));
	}
	return result;
}

/// Runs all tests in the given file.
void run_pcre2_tests(const std::filesystem::path &filename) {
	std::vector<test> tests;
	INFO("Test file: " << filename);
	std::unique_ptr<std::byte[]> test_file;
	{
		std::ifstream fin(filename, std::ios::binary);
		fin.seekg(0, std::ios::end);
		auto file_size = fin.tellg();
		fin.seekg(0, std::ios::beg);
		test_file = std::make_unique<std::byte[]>(file_size);
		fin.read(reinterpret_cast<char*>(test_file.get()), file_size);
		tests = parse_tests(test_file.get(), file_size);
	}

	std::ofstream fout(filename.filename().concat(".out"));

	// use the same set of objects to test that their internal states are reset properly
	cp::regex::matcher<stream_t, data_types> matcher;
	cp::regex::parser<stream_t> parser([&](const stream_t &s, const std::u8string_view msg) {
		fout <<
			"Error at byte " << s.byte_position() << ", codepoint " << s.codepoint_position() <<
			": " << std::string_view(reinterpret_cast<const char*>(msg.data()), msg.length()) << "\n";
	});
	cp::regex::compiler compiler;

	for (const auto &test : tests) {
		std::basic_string<std::byte> pattern_str;
		for (cp::codepoint c : test.pattern.pattern) {
			pattern_str.append(cp::encodings::utf8::encode_codepoint(c));
		}

		// dump pattern
		auto pattern_view = std::string_view(
			reinterpret_cast<const char*>(pattern_str.data()), pattern_str.size()
		);
		INFO("Pattern: " << pattern_view.substr(0, std::min<std::size_t>(300, pattern_view.size())));
		INFO("  Extended: " << test.pattern.options.extended);
		INFO("  Case insensitive: " << test.pattern.options.case_insensitive);
		fout << "/" << std::string_view(
			reinterpret_cast<const char*>(test_file.get() + test.pattern.byte_begin),
			test.pattern.byte_end - test.pattern.byte_begin
		) << "/";
		constexpr bool dump_original_options = false;
		if (!test.pattern.pattern.empty() && test.pattern.pattern.back() == U'\\') {
			fout << "\\";
		}
		if constexpr (dump_original_options) {
			fout << std::string_view(
				reinterpret_cast<const char*>(test_file.get() + test.pattern.options_begin),
				test.pattern.options_end - test.pattern.options_begin
			);
		} else {
			test.pattern.dump_options(fout);
		}
		fout << "\n";
		if (test.pattern.bincode) {
			fout <<
				"------------------------------------------------------------------\n" <<
				"        No bincode available\n" <<
				"------------------------------------------------------------------\n";
		}

		// compile regex
		cp::regex::ast ast;
		cp::regex::ast::analysis analysis;
		cp::regex::compiled<data_types>::state_machine sm;
		{
			stream_t stream(pattern_str.data(), pattern_str.data() + pattern_str.size());
			ast = parser.parse(stream, test.pattern.options);
			analysis = ast.analyze();
			sm = compiler.compile(ast, analysis).finalize<data_types>();
		}

		// log ast
		std::stringstream ss;
		ast.create_dumper(ss).dump(ast.root());
		std::string ast_str = ss.str();
		if (pattern_str.size() > 300) {
			ast_str = "[Too long]";
		}
		INFO("Dumped pattern:\n" << ast_str);

		bool prev_no_match = false;
		for (const auto &str : test.data) {
			std::basic_string<std::byte> data_str;
			for (cp::codepoint c : str.string) {
				data_str.append(cp::encodings::utf8::encode_codepoint(c));
			}

			// dump data string
			std::stringstream data_ss;
			dump_string(data_ss, str.string);
			if (!prev_no_match && str.expect_no_match) {
				fout << "\\= Expect no match\n";
				prev_no_match = true;
			}
			cp::assert_true_logical(str.byte_end >= str.byte_begin, "invalid data range");
			fout << "    " << std::string_view(
				reinterpret_cast<const char*>(test_file.get() + str.byte_begin), str.byte_end - str.byte_begin
			) << "\n";
			INFO("Data string: " << data_ss.str());

			matcher.marker = matcher_t::marker_ref();
			stream_t stream(data_str.data(), data_str.data() + data_str.size());
			std::vector<std::pair<matcher_t::result, matcher_t::marker_ref>> matches;
			std::size_t attempts = 0;
			if (test.pattern.options.global) {
				matcher.find_all(stream, sm, [&](matcher_t::result pos) {
					matches.emplace_back(pos, matcher.marker);
					return attempts <= 1000;
				});
			} else {
				bool reject_empty_match = false;
				if (auto match = matcher.find_next(stream, sm, reject_empty_match)) {
					matches.emplace_back(std::move(match.value()), matcher.marker);
				}
			}
			CHECK((attempts <= 1000 && matches.empty() == str.expect_no_match));

			std::stringstream matches_str;
			for (const auto &[match, marker] : matches) {
				std::size_t beg = match.captures[0].begin.codepoint_position();
				matches_str << "[" << beg << ", " << beg + match.captures[0].length << "], ";
			}

			// dump matches
			INFO("  Matches: " << matches_str.str());
			if (matches.empty()) {
				if (matcher.marker) {
					const auto &name = sm.get_marker_name(matcher.marker);
					fout <<
						"No match, mark = " <<
						std::string_view(reinterpret_cast<const char*>(name.data()), name.size()) << "\n";
				} else {
					fout << "No match\n";
				}
			} else {
				for (const auto &[match, marker] : matches) {
					for (std::size_t i = 0; i < match.captures.size(); ++i) {
						fout << std::setw(2) << std::setfill(' ') << i << ": ";
						if (match.captures[i].is_valid()) {
							cp::codepoint_string capture_str;
							stream_t capture_stream = match.captures[i].begin;
							std::size_t length = match.captures[i].length;
							if (i == 0 && match.overriden_match_begin) {
								capture_stream = match.overriden_match_begin.value();
								std::size_t new_start = capture_stream.codepoint_position();
								std::size_t old_start = match.captures[i].begin.codepoint_position();
								length = std::max(length + old_start, new_start) - new_start;
							}
							for (std::size_t j = 0; j < length; ++j) {
								capture_str.push_back(capture_stream.take());
							}
							dump_string(fout, capture_str);
							fout << "\n";

							if (test.pattern.aftertext && i == 0) {
								fout << std::setw(2) << std::setfill(' ') << i << "+ ";
								cp::codepoint_string after_str;
								while (!capture_stream.empty()) {
									after_str.push_back(capture_stream.take());
								}
								dump_string(fout, after_str);
								fout << "\n";
							}
						} else {
							fout << "<unset>\n";
						}
						if (marker) {
							const auto &name = sm.get_marker_name(marker);
							fout <<
								"MK: " <<
								std::string_view(reinterpret_cast<const char*>(name.data()), name.size()) << "\n";
						}
					}
				}
			}
		}
			
		fout << "\n";
	}
}

const std::filesystem::path directory = "thirdparty/pcre2/testdata";

TEST_CASE("PCRE2 testinput1", "[regex.pcre2.testinput1]") {
	run_pcre2_tests(directory / "testinput1");
}

TEST_CASE("PCRE2 testinput2", "[regex.pcre2.testinput2]") {
	run_pcre2_tests(directory / "testinput2");
}
