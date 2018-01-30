#pragma once

#include <string>
#include <codecvt>
#include <locale>

#include <rapidjson/document.h>

#define CP_USE_UTF8

namespace codepad {
	using char8_t = unsigned char;
	using u8str_t = std::basic_string<char8_t>;
	constexpr char32_t
		replacement_character = 0xFFFD,
		invalid_min = 0xD800, invalid_max = 0xDFFF,
		unicode_max = 0x10FFFF;

#define CP_STRLIT_U8(X) reinterpret_cast<const char8_t*>(X)
#define CP_STRLIT_U32(X) U##X

	// encoding settings
#ifdef CP_USE_UTF8
	using char_t = char8_t;
#	define CP_STRLIT(X) CP_STRLIT_U8(X)
#elif defined(CP_USE_UTF32)
	using char_t = char32_t;
#	define CP_STRLIT(X) CP_STRLIT_U32(X)
#endif
	using str_t = std::basic_string<char_t>;

	namespace json {
		// encoding settings
#ifdef CP_USE_UTF8
		using encoding = rapidjson::UTF8<char_t>;
#elif defined(CP_USE_UTF32)
		using encoding = rapidjson::UTF32<char_t>;
#endif

		using value_t = rapidjson::GenericValue<encoding>;
		using parser_value_t = rapidjson::GenericDocument<encoding>;

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

	template <typename Char> inline size_t get_unit_count(const Char *cs) {
		size_t i = 0;
		for (; *cs; ++i, ++cs) {
		}
		return i;
	}

	inline bool is_newline(char32_t c) {
		return c == '\n' || c == '\r';
	}
	inline bool is_graphical_char(char32_t c) { // TODO incomplete
		return c != '\n' && c != '\r' && c != '\t' && c != ' ';
	}
	inline bool is_valid_codepoint(char32_t c) {
		return c < invalid_min || (c > invalid_max && c <= unicode_max);
	}

	template <typename EncChar, typename Char, typename T> using enable_for_encoding =
		std::enable_if<std::is_same_v<EncChar, std::decay_t<Char>>, T>;
	template <typename Char, typename T> using enable_for_utf8 = enable_for_encoding<char8_t, Char, T>;
	template <typename Char, typename T> using enable_for_utf8_t = typename enable_for_utf8<Char, T>::type;
	template <typename Char, typename T> using enable_for_utf16 = enable_for_encoding<char16_t, Char, T>;
	template <typename Char, typename T> using enable_for_utf16_t = typename enable_for_utf16<Char, T>::type;
	template <typename Char, typename T> using enable_for_utf32 = enable_for_encoding<char32_t, Char, T>;
	template <typename Char, typename T> using enable_for_utf32_t = typename enable_for_utf32<Char, T>::type;

	// the caller is responsible of determining whether i == end
	template <typename It1, typename It2> inline enable_for_utf8_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2 end, char32_t &v) {
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
	template <typename It1, typename It2> inline enable_for_utf8_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2 end) {
		char8_t fc = *i;
		if ((fc & 0xE0) == 0xC0) {
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
		} else if ((fc & 0xF0) == 0xE0) {
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
		} else if ((fc & 0xF8) == 0xF0) {
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
			if (++i == end || (*i & 0xC0) != 0x80) {
				return false;
			}
		} else if ((fc & 0x80) != 0) {
			++i;
			return false;
		}
		++i;
		return true;
	}
	template <typename It1, typename It2> inline enable_for_utf16_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2 end, char32_t &v) {
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
	template <typename It1, typename It2> inline enable_for_utf16_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2 end) {
		char16_t fc = *i;
		if ((fc & 0xDC00) == 0xD800) {
			if (++i == end || (*i & 0xDC00) != 0xDC00) {
				return false;
			}
		} else {
			if ((fc & 0xDC00) == 0xDC00) {
				++i;
				return false;
			}
		}
		++i;
		return true;
	}
	template <typename It1, typename It2> inline enable_for_utf32_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2, char32_t &v) {
		v = *i;
		++i;
		return is_valid_codepoint(v);
	}
	template <typename It1, typename It2> inline enable_for_utf32_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint(It1 &i, It2) {
		bool res = is_valid_codepoint(*i);
		++i;
		return res;
	}

	// safe even if i == end or i == beg
	template <typename It1, typename It2> inline enable_for_utf8_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint_rough(It1 &i, It2 end) {
		for (; i != end && (*i & 0xC0) == 0x80; ++i) {
		}
	}
	template <typename It1, typename It2> inline enable_for_utf16_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint_rough(It1 &i, It2 end) {
		for (; i != end && (*i & 0xDC00) == 0xDC00; ++i) {
		}
	}
	template <typename It1, typename It2> inline enable_for_utf32_t<
		typename std::iterator_traits<It1>::value_type, bool
	> next_codepoint_rough(It1 &i, It2 end) {
		if (i != end) {
			++i;
		}
	}
	template <typename It1, typename It2> inline enable_for_utf8_t<
		typename std::iterator_traits<It1>::value_type, bool
	> previous_codepoint_rough(It1 &i, It2 beg) {
		for (; i != beg && (*i & 0xC0) == 0x80; --i) {
		}
	}
	template <typename It1, typename It2> inline enable_for_utf16_t<
		typename std::iterator_traits<It1>::value_type, bool
	> previous_codepoint_rough(It1 &i, It2 beg) {
		for (; i != beg && (*i & 0xDC00) == 0xDC00; --i) {
		}
	}
	template <typename It1, typename It2> inline enable_for_utf32_t<
		typename std::iterator_traits<It1>::value_type, bool
	> previous_codepoint_rough(It1 &i, It2 beg) {
		if (i != beg) {
			--i;
		}
	}

	template <typename It1, typename It2> inline size_t count_codepoints(It1 beg, It2 end) {
		size_t result = 0;
		for (; beg != end; next_codepoint(beg, end), ++result) {
		}
		return result;
	}
	template <typename It1, typename It2> inline size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
		size_t i = 0;
		for (; i < num && beg != end; next_codepoint(beg, end), ++i) {
		}
		return i;
	}

	template <typename T> struct codepoint_iterator_base {
		template <typename U> friend struct codepoint_iterator_base;
	public:
		codepoint_iterator_base() = default;
		codepoint_iterator_base(T beg, T end, size_t cp = 0) : _ptr(beg), _next(beg), _end(end), _cps(cp) {
			if (_next != _end) {
				_good = next_codepoint(_next, _end, _cv);
			}
		}
		template <typename U> codepoint_iterator_base(const codepoint_iterator_base<U> &it) :
			_ptr(it._ptr), _next(it._next), _end(it._end), _cps(it._cps), _cv(it._cv), _good(it._good) {
		}

		char32_t operator*() const {
			return _cv;
		}

		void next() {
			_ptr = _next;
			++_cps;
			if (_next != _end) {
				_good = next_codepoint(_next, _end, _cv);
			}
		}

		codepoint_iterator_base &operator++() {
			next();
			return *this;
		}
		codepoint_iterator_base operator++(int) {
			codepoint_iterator_base oldv = *this;
			++*this;
			return oldv;
		}

		bool at_end() const {
			return _ptr == _end;
		}
		bool next_end() const {
			return _next == _end;
		}
		bool current_good() const {
			return _good;
		}
		char32_t current_codepoint() const {
			return _cv;
		}
		void set_current_codepoint(size_t v) {
			_cps = v;
		}
		size_t codepoint_position() const {
			return _cps;
		}
		size_t unit_position(const T &beg) const {
			return _ptr - beg;
		}
		const T &get_raw_iterator() const {
			return _ptr;
		}
		const T &get_raw_next_iterator() const {
			return _next;
		}
	protected:
		T _ptr{}, _next{}, _end{};
		size_t _cps = 0;
		char32_t _cv{};
		bool _good = false;
	};
	using string_codepoint_iterator = codepoint_iterator_base<str_t::const_iterator>;
	using raw_codepoint_iterator = codepoint_iterator_base<const char_t*>;

	template <typename Cb> inline void translate_codepoint_utf8(Cb &&cb, char32_t c) {
		if (c < 0x80) {
			cb({static_cast<char8_t>(c)});
		} else if (c < 0x800) {
			cb({
				static_cast<char8_t>(0xC0 | (c >> 6)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
				});
		} else if (c < 0x10000) {
			cb({
				static_cast<char8_t>(0xE0 | (c >> 12)),
				static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
				});
		} else {
			cb({
				static_cast<char8_t>(0xF0 | (c >> 18)),
				static_cast<char8_t>(0x80 | ((c >> 12) & 0x3F)),
				static_cast<char8_t>(0x80 | ((c >> 6) & 0x3F)),
				static_cast<char8_t>(0x80 | (c & 0x3F))
				});
		}
	}
	template <typename Cb> inline void translate_codepoint_utf16(Cb &&cb, char32_t c) {
		if (c < 0x10000) {
			cb({static_cast<char16_t>(c)});
		} else {
			char32_t mined = c - 0x10000;
			cb({
				static_cast<char16_t>((mined >> 10) | 0xD800),
				static_cast<char16_t>((mined & 0x03FF) | 0xDC00)
				});
		}
	}
	template <typename Cb> inline void translate_codepoint_utf32(Cb &&cb, char32_t c) {
		cb({c});
	}

	template <typename Str> inline enable_for_utf8_t<typename Str::value_type, void> append_codepoint(Str &s, char32_t c) {
		translate_codepoint_utf8([&s, c](std::initializer_list<char8_t> l) { s.append(l); }, c);
	}
	template <typename Str> inline enable_for_utf16_t<typename Str::value_type, void> append_codepoint(Str &s, char32_t c) {
		translate_codepoint_utf16([&s, c](std::initializer_list<char16_t> l) { s.append(l); }, c);
	}
	template <typename Str> inline enable_for_utf32_t<typename Str::value_type, void> append_codepoint(Str &s, char32_t c) {
		translate_codepoint_utf32([&s, c](std::initializer_list<char32_t> l) { s.append(l); }, c);
	}

	template <typename To, typename From> inline To convert_encoding(
		const From &str
	) {
		To result;
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
		return convert_encoding<u8str_t>(str);
	}
	inline u8str_t utf16_to_utf8(const std::u16string &str) {
		return convert_encoding<u8str_t>(str);
	}
	inline std::u16string utf32_to_utf16(const std::u32string &str) {
		return convert_encoding<std::u16string>(str);
	}
	inline std::u16string utf8_to_utf16(const u8str_t &str) {
		return convert_encoding<std::u16string>(str);
	}
	inline std::u32string utf16_to_utf32(const std::u16string &str) {
		return convert_encoding<std::u32string>(str);
	}
	inline std::u32string utf8_to_utf32(const u8str_t &str) {
		return convert_encoding<std::u32string>(str);
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

	template <typename Char> inline str_t convert_to_current_encoding(const std::basic_string<Char> &s) {
#ifdef CP_USE_UTF8
		return convert_to_utf8(s);
#elif defined(CP_USE_UTF32)
		return convert_to_utf32(s);
#endif
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
