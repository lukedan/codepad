#pragma once

#include <sstream>
#include <codecvt>
#include <locale>

#include "textconfig.h"

namespace codepad {
	typedef unsigned char char8_t;
	typedef std::basic_string<char8_t> u8str_t;
	constexpr char32_t
		replacement_character = 0xFFFD,
		invalid_min = 0xD800, invalid_max = 0xDFFF,
		unicode_max = 0x10FFFF;

	namespace json {
		inline str_t get_as_string(const value_t &v) {
			return str_t(v.GetString(), v.GetStringLength());
		}

		namespace _details {
			template <typename Res, typename Obj, bool(Obj::*TypeVerify)() const, Res(Obj::*Getter)() const> inline bool try_get(
				const Obj &v, const str_t &s, Res &ret
			) {
				auto found = v.FindMember(s.c_str());
				if (found != v.MemberEnd() && (found->value.*TypeVerify)()) {
					ret = (found->value.*Getter)();
					return true;
				}
				return false;
			}
		}
		template <typename T> bool try_get(const value_t&, const str_t&, T&);
		template <> inline bool try_get<bool>(const value_t &v, const str_t &s, bool &ret) {
			return _details::try_get<bool, value_t, &value_t::IsBool, &value_t::GetBool>(v, s, ret);
		}
		template <> inline bool try_get<double>(const value_t &v, const str_t &s, double &ret) {
			return _details::try_get<double, value_t, &value_t::IsNumber, &value_t::GetDouble>(v, s, ret);
		}
		template <> inline bool try_get<str_t>(const value_t &v, const str_t &s, str_t &ret) {
			auto found = v.FindMember(s.c_str());
			if (found != v.MemberEnd() && found->value.IsString()) {
				ret = get_as_string(found->value);
				return true;
			}
			return false;
		}

		template <typename T> inline T get_or_default(const value_t &v, const str_t &s, const T &def) {
			T res;
			if (try_get(v, s, res)) {
				return res;
			}
			return def;
		}
	}

	inline bool is_newline(char_t c) {
		return c == U'\n' || c == U'\r';
	}
	inline bool is_graphical_char(char_t c) { // TODO incomplete
		return c != U'\n' && c != U'\r' && c != U'\t' && c != U' ';
	}
	inline bool is_valid_codepoint(char_t c) {
		return c < invalid_min || (c > invalid_max && c <= unicode_max);
	}

	// the caller is responsible of determining whether i == end
	template <typename Iter> inline bool next_codepoint(
		Iter &i, u8str_t::const_iterator end, char32_t &v
	) {
		char8_t fc = *i;
		if ((fc & 0x80) == 0) {
			v = fc;
		} else if ((fc & 0xE0) == 0xC0) {
			v = static_cast<char32_t>(fc & 0x1F) << 6;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= *i & 0x3F;
		} else if ((fc & 0xF0) == 0xE0) {
			v = static_cast<char32_t>(fc & 0x0F) << 12;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= static_cast<char32_t>(*i & 0x3F) << 6;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= *i & 0x3F;
		} else if ((fc & 0xF8) == 0xF0) {
			v = static_cast<char32_t>(fc & 0x07) << 18;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= static_cast<char32_t>(*i & 0x3F) << 12;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= static_cast<char32_t>(*i & 0x3F) << 6;
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			v |= *i & 0x3F;
		} else {
			++i;
			return false;
		}
		++i;
		return true;
	}
	template <typename Iter> inline bool next_codepoint(
		Iter &i, std::u16string::const_iterator end, char32_t &v
	) {
		char16_t fc = *i;
		if ((fc & 0xDC00) == 0xD800) {
			v = (fc & 0x03FF) << 10;
			if (++i == end || (*i & 0xDC00) != 0xDC00) {
				return false;
			}
			v |= *i & 0x03FF;
		} else {
			v = fc;
			if ((fc & 0xDC00) == 0xDC00) {
				++i;
				return false;
			}
		}
		++i;
		return true;
	}
	template <typename Iter> inline bool next_codepoint(
		Iter &i, std::u32string::const_iterator, char32_t &v
	) {
		v = *i;
		++i;
		return is_valid_codepoint(v);
	}

	inline void append_codepoint(u8str_t &s, char32_t c) {
		if (c < 0x80) {
			s.append({static_cast<char8_t>(c)});
		} else if (c < 0x800) {
			s.append({
				static_cast<char8_t>(0xC0 | (c >> 6)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
			});
		} else if (c < 0x10000) {
			s.append({
				static_cast<char8_t>(0xE0 | (c >> 12)),
				static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
			});
		} else {
			s.append({
				static_cast<char8_t>(0xF0 | (c >> 18)),
				static_cast<char8_t>(0x80 | ((c >> 12) & 0x3F)),
				static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
			});
		}
	}
	inline void append_codepoint(std::u16string &s, char32_t c) {
		if (c < 0x10000) {
			s.append({static_cast<char16_t>(c)});
		} else {
			char32_t mined = c - 0x10000;
			s.append({
				static_cast<char16_t>((mined >> 10) | 0xD800),
				static_cast<char16_t>((mined & 0x03FF) | 0xDC00)
			});
		}
	}
	inline void append_codepoint(std::u32string &s, char32_t c) {
		s.append({c});
	}

	template <typename To, typename From> inline std::basic_string<To> convert_encoding(
		const std::basic_string<From> &str
	) {
		std::basic_string<To> result;
		result.reserve(str.length());
		auto i = str.begin(), end = str.end();
		char32_t c;
		while (i != end) {
			if (!next_codepoint(i, end, c)) {
				c = replacement_character;
			}
			append_codepoint(result, c);
		}
		return result;
	}

	inline u8str_t utf32_to_utf8(const std::u32string &str) {
		return convert_encoding<char8_t>(str);
	}
	inline u8str_t utf16_to_utf8(const std::u16string &str) {
		return convert_encoding<char8_t>(str);
	}
	inline std::u16string utf32_to_utf16(const std::u32string &str) {
		return convert_encoding<char16_t>(str);
	}
	inline std::u16string utf8_to_utf16(const u8str_t &str) {
		return convert_encoding<char16_t>(str);
	}
	inline std::u32string utf16_to_utf32(const std::u16string &str) {
		return convert_encoding<char32_t>(str);
	}
	inline std::u32string utf8_to_utf32(const u8str_t &str) {
		return convert_encoding<char32_t>(str);
	}

	inline u8str_t convert_to_utf8(u8str_t s) {
		return s;
	}
	inline u8str_t convert_to_utf8(const std::u16string &s) {
		return utf16_to_utf8(s);
	}
	inline u8str_t convert_to_utf8(const std::u32string &s) {
		return utf32_to_utf8(s);
	}
	inline std::u16string convert_to_utf16(std::u16string s) {
		return s;
	}
	inline std::u16string convert_to_utf16(const std::u32string &s) {
		return utf32_to_utf16(s);
	}
	inline std::u16string convert_to_utf16(const u8str_t &s) {
		return utf8_to_utf16(s);
	}
	inline std::u32string convert_to_utf32(std::u32string s) {
		return s;
	}
	inline std::u32string convert_to_utf32(const u8str_t &s) {
		return utf8_to_utf32(s);
	}
	inline std::u32string convert_to_utf32(const std::u16string &s) {
		return utf16_to_utf32(s);
	}

	template <typename T> inline str_t to_str(T t) {
		std::string res = std::to_string(t);
		str_t result;
		result.reserve(res.length());
		for (auto i = res.begin(); i != res.end(); ++i) {
			result.append({static_cast<char_t>(*i)});
		}
		return result;
	}
}
