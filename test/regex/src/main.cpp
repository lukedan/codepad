// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Test for regular expressions.

#include <iostream>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <codepad/core/regex/parser.h>
#include <codepad/core/regex/compiler.h>
#include <codepad/core/regex/matcher.h>

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

	using stream_t = cp::regex::basic_string_input_stream<cp::encodings::utf8>;
	using parser_t = cp::regex::parser<stream_t>;

	std::string regex;
	std::string string;
	while (true) {
		std::cout << "\n\n==============\nregex: ";
		if (!std::getline(std::cin, regex)) {
			break;
		}
		auto regex_data = reinterpret_cast<const std::byte*>(regex.data());
		parser_t parser;
		auto ast = parser.parse(stream_t(regex_data, regex_data + regex.size()));
		auto dumper = cp::regex::ast::make_dumper(std::cout);
		dumper.dump(ast);

		cp::regex::compiler compiler;
		cp::regex::state_machine sm = compiler.compile(ast);

		while (true) {
			std::cout << "\nstring: ";
		
			cp::regex::matcher<stream_t> matcher;
			if (!std::getline(std::cin, string)) {
				std::cin.clear();
				break;
			}
			auto string_data = reinterpret_cast<const std::byte*>(string.data());
			stream_t str_stream(string_data, string_data + string.size());
			while (true) {
				if (auto x = matcher.find_next(str_stream, sm)) {
					std::cout << "  match from " << x.value() << " to " << str_stream.position() << "\n";
				} else {
					break;
				}
			}
		}
	}
	return 0;
}
