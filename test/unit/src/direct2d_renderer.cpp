// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Tests the Direct2D renderer.

#include <catch.hpp>

#include <codepad/os/windows/direct2d_renderer.h>

using renderer_t = codepad::os::direct2d::renderer;

/// Converts a \p std::u32string_view to a \p std::basic_string_view<codepad::codepoint>.
[[nodiscard]] std::basic_string_view<codepad::codepoint> cast_string_view(std::u32string_view view) {
	return std::basic_string_view<codepad::codepoint>(
		reinterpret_cast<const codepad::codepoint*>(view.data()), view.size()
	);
}

/// Converts a UTF-32 string to a UTF-8 string.
[[nodiscard]] std::u8string utf32_to_utf8(std::basic_string_view<codepad::codepoint> str) {
	std::u8string result;
	for (codepad::codepoint cp : str) {
		auto cp_str = codepad::encodings::utf8::encode_codepoint(cp);
		result.append(reinterpret_cast<const char8_t*>(cp_str.data()), cp_str.size());
	}
	return result;
}

/// Performs hit testing at \p -FLT_MAX and \p FLT_MAX, as well as around the specified characters. If \p test_all is
/// \p true then all characters will be tested.
void test_plain_text(
	codepad::ui::plain_text &text, std::size_t num_chars,
	std::initializer_list<std::size_t> test_characters, bool test_all = false
) {
	// test -FLT_MAX and +FLT_MAX
	{
		auto hit_test_negmax = text.hit_test(-std::numeric_limits<double>::max());
		REQUIRE(hit_test_negmax.character == 0);
		REQUIRE(!hit_test_negmax.rear);
	}
	{
		auto hit_test_posmax = text.hit_test(std::numeric_limits<double>::max());
		REQUIRE(hit_test_posmax.character == num_chars);
		REQUIRE(!hit_test_posmax.rear);
	}
	// test all requested character indices
	std::vector<std::size_t> test_positions(test_characters);
	if (test_all) {
		test_positions.clear();
		for (std::size_t i = 0; i < num_chars; ++i) {
			test_positions.emplace_back(i);
		}
	}
	for (std::size_t i : test_positions) {
		codepad::rectd placement = text.get_character_placement(i);
		{ // test 1/4 position
			auto hit_test_before = text.hit_test(placement.xmin * 0.75 + placement.xmax * 0.25);
			REQUIRE(hit_test_before.character == i);
			REQUIRE(!hit_test_before.rear);
		}
		{ // test 3/4 position
			auto hit_test_before = text.hit_test(placement.xmin * 0.75 + placement.xmax * 0.25);
			REQUIRE(hit_test_before.character == i);
			REQUIRE(!hit_test_before.rear);
		}
	}
}

/// Tests the same clip of text using \ref codepad::ui::renderer_base::create_plain_text(), both UTF-8 and
/// UTF-32 versions, and \ref codepad::ui::renderer_base::create_plain_text_fast(). This function first calls
/// \ref test_plain_text() on the result, then checks that all the characters in the UTF-32 and UTF-8 versions have
/// the same layout.
void test_plain_text_for_string(
	codepad::ui::renderer_base &rend, codepad::ui::font &font,
	std::basic_string_view<codepad::codepoint> u32_str,
	std::initializer_list<std::size_t> test_characters, bool test_all = false
) {
	std::u8string u8_str = utf32_to_utf8(u32_str);

	auto text_u32 = rend.create_plain_text(u32_str, font, 10.0);
	test_plain_text(*text_u32, u32_str.size(), test_characters, test_all);

	auto text_u32_fast = rend.create_plain_text_fast(u32_str, font, 10.0);
	test_plain_text(*text_u32_fast, u32_str.size(), test_characters, test_all);

	auto text_u8 = rend.create_plain_text(u8_str, font, 10.0);
	test_plain_text(*text_u8, u32_str.size(), test_characters, test_all);

	REQUIRE(text_u32->get_width() == text_u8->get_width());
	for (std::size_t i = 0; i < u32_str.size(); ++i) {
		codepad::rectd
			plac_u32 = text_u32->get_character_placement(i),
			plac_u8 = text_u8->get_character_placement(i);

		REQUIRE(plac_u32.xmin == plac_u8.xmin);
		REQUIRE(plac_u32.xmax == plac_u8.xmax);
		REQUIRE(plac_u32.ymin == plac_u8.ymin);
		REQUIRE(plac_u32.ymax == plac_u8.ymax);
	}
}

TEST_CASE("Creation of plain_text objects", "[direct2d.plain_text]") {
	auto rend = std::make_unique<renderer_t>();

	auto font_family = rend->find_font_family(u8"Cascadia Code");
	auto font = font_family->get_matching_font(
		codepad::ui::font_style::normal, codepad::ui::font_weight::normal, codepad::ui::font_stretch::normal
	);

	// test simple text
	test_plain_text_for_string(*rend, *font, cast_string_view(U"Hello, world!"), {}, true);

	// test ligatures
	test_plain_text_for_string(*rend, *font, cast_string_view(U"-> :: ()<=>"), {}, true);

	{ // test text with surrogate pairs
		std::basic_string<codepad::codepoint> surrogate_pair({ 0x20000, 0x10, 0x20001, 0xFFFD });
		test_plain_text_for_string(*rend, *font, surrogate_pair, {}, true);
	}

	{ // test text with invalid codepoints
		std::basic_string<codepad::codepoint> invalid_codepoints(
			{ 0x10, 0xD800, 0xD801, 0xDFFF, 0xDCCC, 0xE000, 0x10FFFF, 0x12 }
		);
		test_plain_text_for_string(*rend, *font, invalid_codepoints, {}, true);
	}
}
