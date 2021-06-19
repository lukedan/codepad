// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Regex tests using the PCRE2 test data.

#include <set>
#include <fstream>
#include <filesystem>

#include <catch2/catch.hpp>

#include <codepad/core/logging.h>
#include <codepad/core/regex/matcher.h>

namespace cp = codepad;

/// A string to match against and associated options.
struct test_data {
	cp::codepoint_string string; ///< The string to match agains.
	/// Used to indicate that there should be no matches in all following strings. This is only valid when
	/// \ref string is empty.
	bool expect_no_match = false;
};
/// A pattern and associated options.
struct pattern_data {
	cp::codepoint_string pattern; ///< The pattern.
	cp::regex::options options; ///< Matching options.
};
/// A test.
struct test {
	pattern_data pattern; ///< The pattern.
	std::vector<test_data> data; ///< Test strings.
};
using stream_t = cp::regex::basic_input_stream<cp::encodings::utf8, const std::byte*>; ///< UTF-8 input stream type.

/// Fails with the given message.
void fail(const char *msg = nullptr) {
	if (msg) {
		INFO(msg);
	}
	REQUIRE(false);
}

/// Parses a pattern and its options, and consumes the following line break.
[[nodiscard]] pattern_data parse_pattern(stream_t &stream) {
	pattern_data result;
	while (true) {
		while (!stream.empty() && !cp::is_graphical_char(stream.peek())) {
			stream.take();
		}
		if (stream.empty()) {
			break;
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
					"Invalid delimiter: " << static_cast<int>(cpt) <<
					", " << (cpt < 128 ? static_cast<char>(cpt) : '?')
				);
				fail();
			}
			while (true) {
				if (stream.empty()) {
					fail("Stream terminated prematurely");
				}
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
			while (!stream.empty()) {
				if (stream.peek() == U'\r' || stream.peek() == U'\n') {
					break;
				}
				switch (stream.take()) {
				case U'i':
					result.options.case_insensitive = true;
					break;
				case U'x':
					result.options.extended = true;
					break;
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
[[nodiscard]] test_data parse_data(stream_t &stream) {
	test_data result;
	std::size_t non_graphic = 0;
	while (!stream.empty()) {
		if (stream.peek() == U'\r' || stream.peek() == U'\n') {
			cp::regex::consume_line_ending(stream);
			break;
		}
		cp::codepoint c = stream.take();
		if (result.string.empty()) {
			if (!cp::is_graphical_char(c)) {
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
		// TODO the escape sequences are really weird for PCRE2 test data
		if (c == U'\\' && !stream.empty()) {
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
				REQUIRE(!stream.empty());
				REQUIRE(stream.take() == U'{');
				c = 0;
				while (true) {
					REQUIRE(!stream.empty());
					cp::codepoint next_cp = stream.take();
					if (next_cp == U'}') {
						break;
					}
					REQUIRE(next_cp >= U'0');
					REQUIRE(next_cp <= U'7');
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
						REQUIRE(!stream.empty());
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
							REQUIRE(false);
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
		} else {
			if (cp::is_graphical_char(c)) {
				non_graphic = result.string.size() + 1;
			}
		}
		result.string.push_back(c);
	}
	result.string.erase(result.string.begin() + non_graphic, result.string.end());
	return result;
}

/// Parses the tests.
[[nodiscard]] std::vector<test> parse_tests(const std::byte *contents, std::size_t size) {
	stream_t stream(contents, contents + size);
	std::vector<test> result;
	while (!stream.empty()) {
		test current_test;
		current_test.pattern = parse_pattern(stream);
		if (current_test.pattern.pattern.empty()) {
			break;
		}
		bool expect_no_match = false;
		while (!stream.empty()) {
			test_data data = parse_data(stream);
			if (data.string.empty()) {
				if (data.expect_no_match) {
					expect_no_match = true;
					continue;
				} else {
					break;
				}
			}
			data.expect_no_match = expect_no_match;
			current_test.data.emplace_back(std::move(data));
		}
		result.emplace_back(std::move(current_test));
	}
	return result;
}

TEST_CASE("PCRE2 test cases for the regex engine", "[regex.pcre2]") {
	const std::set<std::filesystem::path> valid_test_files = {
		"testinput1", /*"testinput2", /*"testinput3", "testinput4", "testinput5", "testinput6", "testinput7"*/
	};

	std::filesystem::directory_iterator iter("thirdparty/pcre2/testdata");
	for (const auto &entry : iter) {
		std::vector<test> tests;
		if (!valid_test_files.contains(entry.path().filename())) {
			continue;
		}
		INFO("Test file: " << entry.path());
		{
			std::ifstream fin(entry.path(), std::ios::binary);
			fin.seekg(0, std::ios::end);
			auto file_size = fin.tellg();
			fin.seekg(0, std::ios::beg);
			auto test_file = std::make_unique<std::byte[]>(file_size);
			fin.read(reinterpret_cast<char*>(test_file.get()), file_size);
			tests = parse_tests(test_file.get(), file_size);
		}

		for (const auto &test : tests) {
			std::basic_string<std::byte> pattern_str;
			for (cp::codepoint c : test.pattern.pattern) {
				pattern_str.append(cp::encodings::utf8::encode_codepoint(c));
			}
			auto pattern_view = std::string_view(
				reinterpret_cast<const char*>(pattern_str.data()), pattern_str.size()
			);
			INFO("Pattern: " << pattern_view);
			INFO("  Extended: " << test.pattern.options.extended);
			INFO("  Case insensitive: " << test.pattern.options.case_insensitive);

			// compile regex
			cp::regex::ast::nodes::subexpression ast;
			cp::regex::compiled::state_machine sm;
			{
				stream_t stream(pattern_str.data(), pattern_str.data() + pattern_str.size());
				cp::regex::parser<stream_t> parser;
				ast = parser.parse(stream, test.pattern.options);
				cp::regex::compiler compiler;
				sm = compiler.compile(ast);
			}
			cp::regex::matcher<stream_t> matcher;

			// log ast
			std::stringstream ss;
			cp::regex::ast::make_dumper(ss).dump(ast);
			INFO("Dumped pattern:\n" << ss.str());

			for (const auto &str : test.data) {
				std::basic_string<std::byte> data_str;
				for (cp::codepoint c : str.string) {
					data_str.append(cp::encodings::utf8::encode_codepoint(c));
				}
				std::stringstream data_ss;
				{
					for (cp::codepoint cp : str.string) {
						data_ss << cp;
						if (cp >= 0x20 && cp <= 0x7E) {
							data_ss << "'" << static_cast<char>(cp) << "'";
						}
						data_ss << ", ";
					}
				}
				INFO("Data string: " << data_ss.str());
				stream_t stream(data_str.data(), data_str.data() + data_str.size());
				std::vector<std::size_t> matches;
				std::size_t attempts = 0;
				for (; attempts < 1000; ++attempts) {
					if (auto pos = matcher.find_next(stream, sm)) {
						matches.emplace_back(pos.value());
					} else {
						break;
					}
				}
				std::stringstream matches_str;
				for (std::size_t i : matches) {
					matches_str << i << ", ";
				}
				INFO("  Matches: " << matches_str.str());
				CHECK(attempts < 1000);
				CHECK(matches.empty() == str.expect_no_match);
			}
		}
	}
}
