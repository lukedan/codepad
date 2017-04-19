#pragma once

#include <string>

namespace codepad {
#ifdef UNICODE
#	define CPTEXT(X) L ## X
	typedef wchar_t char_t;
#else
#	define CPTEXT(X) X
	typedef char char_t;
#endif
	typedef std::basic_string<char_t> str_t;
	template <typename T> inline str_t to_str(T t) {
#ifdef UNICODE
		return std::to_wstring(t);
#else
		return std::to_string(t);
#endif
	}
}
