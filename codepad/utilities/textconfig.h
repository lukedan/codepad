#pragma once

#include <string>
#include <sstream>

#include <rapidjson/document.h>

namespace codepad {
	using char_t = char32_t;
	using str_t = std::basic_string<char_t>;
	namespace json {
		using value_t = rapidjson::GenericValue<rapidjson::UTF32<char_t>>;
		using parser_value_t = rapidjson::GenericDocument<rapidjson::UTF32<char_t>>;
	}
}
