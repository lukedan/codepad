#pragma once

#include <string>
#include <sstream>

#include <rapidjson/document.h>

namespace codepad {
	typedef char32_t char_t;
	typedef std::basic_string<char_t> str_t;
	namespace json {
		typedef rapidjson::GenericValue<rapidjson::UTF32<char_t>> value_t;
		typedef rapidjson::GenericDocument<rapidjson::UTF32<char_t>> parser_value_t;
	}
}
