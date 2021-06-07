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
	cp::regex::codepoint_string string; ///< The string to match agains.
	/// Used to indicate that there should be no matches in all following strings. This is only valid when
	/// \ref string is empty.
	bool expect_no_match = false;
};
/// A pattern and associated options.
struct pattern_data {
	cp::regex::codepoint_string pattern; ///< The pattern.
	bool case_insensitive = false; ///< Case insensitive matching.
	bool extended = false; ///< Extended mode.
};
/// A test.
struct test {
	pattern_data pattern; ///< The pattern.
	std::vector<test_data> data; ///< Test strings.
};
using stream_t = cp::regex::basic_string_input_stream<cp::encodings::utf8>; ///< UTF-8 input stream type.

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

			// read options
			while (!stream.empty()) {
				if (stream.peek() == U'\r' || stream.peek() == U'\n') {
					break;
				}
				switch (stream.take()) {
				case U'i':
					result.case_insensitive = true;
					break;
				case U'e':
					result.extended = true;
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
		} else {
			// TODO the escape sequences are really weird for PCRE2 test data
			if (c == U'\\' && !stream.empty()) {
				c = stream.take();
				switch (c) {
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

				// TODO
				}
			}
		}
		result.string.push_back(c);
	}
	while (!result.string.empty() && !cp::is_graphical_char(result.string.back())) {
		result.string.pop_back();
	}
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
		"testinput1"
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
			// TODO temporarily skip over certain tests
			if (test.pattern.case_insensitive || test.pattern.extended) {
				continue;
			}

			std::basic_string<std::byte> pattern_str;
			for (cp::codepoint c : test.pattern.pattern) {
				pattern_str.append(cp::encodings::utf8::encode_codepoint(c));
			}
			auto pattern_view = std::string_view(
				reinterpret_cast<const char*>(pattern_str.data()), pattern_str.size()
			);
			INFO("Pattern: " << pattern_view);

			// compile regex
			cp::regex::ast::nodes::subexpression ast;
			cp::regex::state_machine sm;
			{
				stream_t stream(pattern_str.data(), pattern_str.data() + pattern_str.size());
				cp::regex::parser<stream_t> parser(stream);
				ast = parser.parse();
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
				INFO("Data string: " << std::string_view(
					reinterpret_cast<const char*>(data_str.data()), data_str.size()
				));
				stream_t stream(data_str.data(), data_str.data() + data_str.size());
				bool has_match = false;
				while (true) {
					if (auto pos = matcher.find_next(stream, sm)) {
						INFO("  Match at " << pos.value());
						if (str.expect_no_match) {
							fail();
						}
						has_match = true;
					} else {
						break;
					}
				}
				REQUIRE(has_match != str.expect_no_match);
			}
		}
	}
}
