#include <catch.hpp>

#include <codepad/editors/theme_manager.h>

using config_t = codepad::editors::theme_configuration;
using spec_t = codepad::editors::code::text_theme_specification;

[[nodiscard]] codepad::ui::font_weight unique_weight(std::size_t i) {
	return static_cast<codepad::ui::font_weight>(i);
}
[[nodiscard]] spec_t unique_spec(std::size_t i) {
	return spec_t(codepad::colord(), codepad::ui::font_style::normal, unique_weight(i));
}

TEST_CASE("Registration and querying within a theme_configuration", "[theme_config]") {
	config_t config;
	config.add_entry(u8"variable.local.const", unique_spec(0));
	config.add_entry(u8"variable.local.constexpr", unique_spec(1));
	config.add_entry(u8"variable.local", unique_spec(2));
	config.add_entry(u8"variable.constexpr", unique_spec(3));
	config.add_entry(u8"variable", unique_spec(4));

	config.add_entry(u8"function", unique_spec(5));
	config.add_entry(u8"function.constexpr", unique_spec(6));

	// test that the key is split and sorted
	REQUIRE(config.entries[0].key == std::vector<std::u8string>({ u8"const", u8"local", u8"variable" }));
	// test that the query key is also split and sorted
	REQUIRE(config.get_index_for(u8"const.local.variable") == 0);
	// test that the shortest one is picked
	REQUIRE(config.get_index_for(u8"variable") == 4);
	REQUIRE(config.get_index_for(u8"local") == 2);
	REQUIRE(config.get_index_for(u8"variable.constexpr") == 3);
	// test that a partial key can be used
	REQUIRE(config.get_index_for(u8"const.variable") == 0);
}
