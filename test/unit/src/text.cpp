// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Tests for string-matching algorithms.

#include <catch2/catch.hpp>

#include <codepad/core/text.h>

using kmp_t = codepad::kmp_matcher<std::u8string>;
using kmp_table_t = std::vector<std::size_t>;

/// Compares the result of the KMP matcher with that of \p std::find().
void test_kmp_matcher(const std::u8string &str, const std::u8string &patt) {
	std::vector<std::size_t> target_matches;
	for (std::size_t i = 0; ; ++i) {
		i = str.find(patt, i);
		if (i == std::u8string::npos) {
			break;
		}
		target_matches.emplace_back(i);
	}

	std::vector<std::size_t> kmp_matches;
	kmp_t matcher(patt);
	kmp_t::state state;
	for (auto it = str.begin(); it != str.end(); ++it) {
		auto [next_it, next_state] = matcher.next_match(it, str.end(), state);
		if (next_it == str.end()) {
			break;
		}
		kmp_matches.emplace_back((next_it - str.begin()) - (patt.size() - 1));

		state = next_state;
		it = next_it;
	}

	REQUIRE(target_matches == kmp_matches);
}

TEST_CASE("Computation of KMP prefix tables", "[kmp.table]") {
	kmp_t test(u8"abcdabd");
	REQUIRE(test.get_table() == kmp_table_t{ 0, 0, 0, 0, 0, 1, 2, 0 });

	test = kmp_t(u8"aaaabcdaa");
	REQUIRE(test.get_table() == kmp_table_t{ 0, 0, 1, 2, 3, 0, 0, 0, 1, 2});

	test = kmp_t(u8"abacababa");
	REQUIRE(test.get_table() == kmp_table_t{ 0, 0, 0, 1, 0, 1, 2, 3, 2, 3 });
}

TEST_CASE("KMP matching", "[kmp]") {
	test_kmp_matcher(u8"hello, world!", u8"world");
	test_kmp_matcher(u8"aabaaabbbaaabbaababaabbabbaaabbab", u8"aab");
	test_kmp_matcher(u8"full", u8"full");
}
