#include <vector>

#include <catch.hpp>

#include <codepad/editors/code/theme.h>

using data_t = int;
using theme_t = codepad::editors::code::text_theme_parameter_info<data_t>;
using flat_theme_t = std::vector<std::pair<data_t, std::size_t>>;

namespace Catch {
	template <typename T, typename U> struct StringMaker<std::pair<T, U>> {
		static std::string convert(const std::pair<T, U> &value) {
			return "{" + StringMaker<T>::convert(value.first) + ", " + StringMaker<U>::convert(value.second) + "}";
		}
	};
}

[[nodiscard]] flat_theme_t flatten_theme(const theme_t &theme) {
	flat_theme_t result;
	for (auto iter = theme.begin(); iter != theme.end(); ++iter) {
		result.emplace_back(iter->value, iter->length);
	}
	return result;
}

TEST_CASE("Modifications on text theme objects keep them in a valid state", "[code][theme]") {
	theme_t theme;
	theme.set_range(0, 10, 1);
	theme.set_range(10, 15, 2);
	theme.set_range(15, 25, 3);
	theme.set_range(25, 40, 2);

	REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 10 }, { 2, 5 }, { 3, 10 }, { 2, 15 }, { 0, 0 } });


	SECTION("set_range") {
		// a range starting from 0 but does not end at a boundary
		theme.set_range(0, 5, 2);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 5 }, { 2, 5 }, { 3, 10 }, { 2, 15 }, { 0, 0 } });
		// a range completely within a segment
		theme.set_range(18, 22, 2);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 5 }, { 2, 5 }, { 3, 3 }, { 2, 4 }, { 3, 3 }, { 2, 15 }, { 0, 0 } });
		// a range that fully overlaps another segment and causes two merges
		theme.set_range(15, 18, 2);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 5 }, { 2, 12 }, { 3, 3 }, { 2, 15 }, { 0, 0 } });
		// a range that merges with the last segment
		theme.set_range(28, 50, 0);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 5 }, { 2, 12 }, { 3, 3 }, { 2, 3 }, { 0, 0 } });
		// a range that spans multiple segments
		theme.set_range(7, 27, 3);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 2 }, { 3, 20 }, { 2, 1 }, { 0, 0 } });
		// a range in/after the last segment with the same value, equivalent to no-op
		theme.set_range(100, 150, 0);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 2 }, { 3, 20 }, { 2, 1 }, { 0, 0 } });
		// a range in/after the last segment
		theme.set_range(100, 150, 1);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 2 }, { 3, 20 }, { 2, 1 }, { 0, 72 }, { 1, 50 }, { 0, 0 } });
		// a range starting at the last segment
		theme.set_range(150, 160, 2);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 2, 5 }, { 1, 2 }, { 3, 20 }, { 2, 1 }, { 0, 72 }, { 1, 50 }, { 2, 10 }, { 0, 0 } });
	}


	SECTION("on_modification") {
		// a modification after the last segment; equivalent to no-op
		theme.on_modification(50, 5, 10);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 10 }, { 2, 5 }, { 3, 10 }, { 2, 15 }, { 0, 0 } });
		// ends at a segment boundary
		theme.on_modification(20, 5, 10);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 10 }, { 2, 5 }, { 3, 5 }, { 0, 10 }, { 2, 15 }, { 0, 0 } });
		// starts & ends at a segment boundary, with the same value on both sides
		theme.on_modification(15, 15, 5);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 10 }, { 2, 25 }, { 0, 0 } });
		// within a single segment
		theme.on_modification(15, 5, 10);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 10 }, { 2, 30 }, { 0, 0 } });
		// cross segment
		theme.on_modification(5, 40, 5);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 5 }, { 0, 0 } });
		// starts at the very beginning
		theme.on_modification(0, 3, 5);
		REQUIRE(flatten_theme(theme) == flat_theme_t{ { 1, 7 }, { 0, 0 } });
	}
}
