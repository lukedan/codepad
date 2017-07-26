#pragma once

#include <sstream>
#include <codecvt>
#include <locale>

#include "textconfig.h"

namespace codepad {
	inline bool is_newline(char_t c) {
		return c == U'\n' || c == U'\r';
	}
	inline bool is_graphical_char(char_t c) { // TODO incomplete
		return c != U'\n' && c != U'\r' && c != U'\t';
	}

	inline std::basic_string<char16_t> utf32_to_utf16(const std::basic_string<char32_t> &str) {
		std::basic_ostringstream<char16_t> result;
		for (auto i = str.begin(); i != str.end(); ++i) {
			if (*i < 0x10000) {
				result << static_cast<char16_t>(*i);
			} else {
				char32_t mined = *i - 0x10000;
				result << static_cast<char16_t>((mined >> 10) | 0xD800);
				result << static_cast<char16_t>((mined & 0x03FF) | 0xDC00);
			}
		}
		return result.str();
	}

	// TODO utilization of deprecated functionality
	namespace _helper {
		template <typename T> struct deletable_facet : public T {
			template <typename ...Args> explicit deletable_facet(Args &&...args) : T(std::forward<Args>(args)...) {
			}
		};
#ifdef _MSC_VER
		template <typename T> struct final_str_conv {
			inline static std::basic_string<T> conv(const std::basic_string<T> &s) {
				return s;
			}
		};
		template <> struct final_str_conv<__int32> {
			inline static std::basic_string<char32_t> conv(const std::basic_string<__int32> &s) {
				return std::basic_string<char32_t>(reinterpret_cast<const char32_t*>(s.c_str()));
			}
		};
#endif
	}
	template <typename To> inline std::basic_string<To> convert_from_utf8(const std::string &str) {
		// TODO utilization of deprecated functionality
#ifdef _MSC_VER
		// TODO fuck visual studio
		typedef typename std::conditional<std::is_same<To, char32_t>::value, __int32, To>::type _To_t;
		std::wstring_convert<_helper::deletable_facet<std::codecvt<_To_t, char, std::mbstate_t>>, _To_t> conv;
		return _helper::final_str_conv<_To_t>::conv(conv.from_bytes(str));
#else
		std::wstring_convert<_helper::deletable_facet<std::codecvt<To, char, std::mbstate_t>>, To> conv;
		return conv.from_bytes(str);
#endif
	}
	template <typename From> inline std::string convert_to_utf8(const std::basic_string<From> &str) {
		std::wstring_convert<_helper::deletable_facet<std::codecvt<From, char, std::mbstate_t>>, From> conv;
		return conv.to_bytes(str);
	}
#ifdef _MSC_VER
	template <> inline std::string convert_to_utf8<char32_t>(const std::basic_string<char32_t> &str) {
		std::wstring_convert<_helper::deletable_facet<std::codecvt<__int32, char, std::mbstate_t>>, __int32> conv;
		return conv.to_bytes(std::basic_string<__int32>(reinterpret_cast<const __int32*>(str.c_str())));
	}
#endif
	inline std::string convert_to_utf8(std::string str) {
		return std::move(str);
	}

#if defined(_MSC_VER) || defined(__GNUC__)
	// TODO fuck again, ugly workaround for VS AND g++
	template <typename T> inline str_t to_str(T t) {
		std::stringstream ss;
		ss << t;
		return convert_from_utf8<char32_t>(ss.str());
	}
#else
	template <typename T> inline str_t to_str(T t) {
		std::basic_ostringstream<char_t> ss;
		ss << t;
		return ss.str();
	}
#endif
}
