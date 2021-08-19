// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Test for regular expressions.

#include <iostream>

/// Enables printing UTF-8 strings using standard streams.
std::ostream &operator<<(std::ostream &out, std::u8string_view sv) {
	return out << std::string_view(reinterpret_cast<const char*>(sv.data()), sv.size());
}
/// Enables printing UTF-8 strings using standard streams.
std::ostream &operator<<(std::ostream &out, const char8_t *str) {
	return out << reinterpret_cast<const char*>(str);
}

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <codepad/core/regex/parser.h>
#include <codepad/core/regex/parser.inl>
#include <codepad/core/regex/compiler.h>
#include <codepad/core/regex/matcher.h>
#include <codepad/core/regex/matcher.inl>

namespace cp = codepad;

int main(int argc, char **argv) {
	Catch::Session session;
	bool interactive = false;
	auto cli =
		session.cli() |
		Catch::clara::Opt(interactive)["--interactive"]("Read and match in an interactive fashion.");
	session.cli(cli);
	int return_code = session.applyCommandLine(argc, argv);
	if (return_code) {
		return return_code;
	}
	if (!interactive) {
		return session.run();
	}

	using stream_t = cp::regex::basic_input_stream<cp::encodings::utf8, const std::byte*>;
	using parser_t = cp::regex::parser<stream_t>;
	using matcher_t = cp::regex::matcher<stream_t, std::ostream&>;

	std::string regex;
	std::string string;
	while (true) {
		std::cout << "\n\n==============\nregex: ";
		if (!std::getline(std::cin, regex)) {
			break;
		}
		auto regex_data = reinterpret_cast<const std::byte*>(regex.data());
		parser_t parser([](const stream_t &s, const std::u8string_view msg) {
			std::cout <<
				"Error at byte " << s.byte_position() << ", codepoint " << s.codepoint_position() <<
				": " << std::string_view(reinterpret_cast<const char*>(msg.data()), msg.length());
		});
		cp::regex::options opt;
		opt.case_insensitive = true;
		auto ast = parser.parse(stream_t(regex_data, regex_data + regex.size()), opt);
		auto dumper = cp::regex::ast::make_dumper(std::cout);
		dumper.dump(ast);

		cp::regex::compiler compiler;
		cp::regex::compiled::state_machine sm = compiler.compile(ast);

		{
			std::ofstream fout("regex.dot");
			sm.dump(fout);
		}

		matcher_t matcher(std::cout);
		while (true) {
			std::cout << "\nstring: ";
		
			if (!std::getline(std::cin, string)) {
				std::cin.clear();
				break;
			}
			auto string_data = reinterpret_cast<const std::byte*>(string.data());
			stream_t str_stream(string_data, string_data + string.size());
			matcher.find_all(str_stream, sm, [&](matcher_t::result res) {
				std::cout << "  match:\n";
				for (std::size_t i = 0; i < res.captures.size(); ++i) {
					std::size_t beg = res.captures[i].begin.codepoint_position();
					std::cout <<
						"    " << i << ": " << beg << " to " << beg + res.captures[i].length << "\n";
				}
			});
		}
	}
	return 0;
}
