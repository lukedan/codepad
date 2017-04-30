#pragma once

#include <sstream>
#include <codecvt>

#include "textconfig.h"

namespace codepad {
	inline bool is_newline(char_t c) {
		return c == '\n' || c == '\r';
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
	namespace helper {
		template <typename T> struct deletable_facet : public T {
			template <typename ...Args> deletable_facet(Args &&...args) : T(std::forward<Args>(args)...) {
			}
			~deletable_facet() {
			}
		};
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
	}
	template <typename To> inline std::basic_string<To> convert_from_utf8(const std::string &str) {
		// TODO utilization of deprecated functionality
		// TODO fuck visual studio
		typedef std::conditional<std::is_same<To, char32_t>::value, __int32, To>::type _to_t;
		std::wstring_convert<helper::deletable_facet<std::codecvt<_to_t, char, std::mbstate_t>>, _to_t> conv;
		return helper::final_str_conv<_to_t>::conv(conv.from_bytes(str));
	}
	template <typename From> inline std::string convert_to_utf8(const std::basic_string<From> &str) {
		std::wstring_convert<helper::deletable_facet<std::codecvt<From, char, std::mbstate_t>>, From> conv;
		return conv.to_bytes(str);
	}
	inline std::string convert_to_utf8(const std::basic_string<char32_t> &str) {
		std::wstring_convert<helper::deletable_facet<std::codecvt<__int32, char, std::mbstate_t>>, __int32> conv;
		return conv.to_bytes(std::basic_string<__int32>(reinterpret_cast<const __int32*>(str.c_str())));
	}
}