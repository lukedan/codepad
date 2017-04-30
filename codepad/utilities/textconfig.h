#pragma once

#include <string>

namespace codepad {
	typedef char32_t char_t;
	typedef std::basic_string<char_t> str_t;
	template <typename T> inline str_t to_str(T t) {
		return std::to_wstring(t);
	}
}
