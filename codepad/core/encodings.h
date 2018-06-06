#pragma once

/// \file
/// Encoding settings, and conversions between one another.
/// Currently supported encodings: UTF-8, UTF-16, UTF-32

#include <string>
#include <codecvt>
#include <locale>
#include <iterator>

#include <rapidjson/document.h>

#define CP_USE_UTF8
//#define CP_USE_UTF32

namespace codepad {
	constexpr char32_t
		replacement_character = 0xFFFD, ///< Unicode replacement character.
		invalid_min = 0xD800, ///< Minimum code point value reserved by UTF-16.
		invalid_max = 0xDFFF, ///< Maximum code point value (inclusive) reserved by UTF-16.
		unicode_max = 0x10FFFF; ///< Maximum code point value (inclusive) of Unicode.

	/// A template version of std::strlen().
	template <typename Char> inline size_t get_unit_count(const Char *cs) {
		size_t i = 0;
		for (; *cs; ++i, ++cs) {
		}
		return i;
	}

	/// Determines if a codepoint is a `new line' character.
	inline bool is_newline(char32_t c) {
		return c == '\n' || c == '\r';
	}
	/// Determines if a codepoint is a graphical char, i.e., is not blank.
	///
	/// \todo May not be complete.
	inline bool is_graphical_char(char32_t c) {
		return c != '\n' && c != '\r' && c != '\t' && c != ' ';
	}
	/// Determines if a codepoint lies in the valid range of Unicode points.
	inline bool is_valid_codepoint(char32_t c) {
		return c < invalid_min || (c > invalid_max && c <= unicode_max);
	}

	/// Counts the number of codepoints between the two iterators. Encodings can use this to fall back to
	/// default behavior, or implement their optimized versions.
	///
	/// \tparam Encoding The encoding used.
	/// \param beg Iterator to the beginning of the range.
	/// \param end Iterator past the ending of the range.
	/// \return The number of codepoints in the range.
	template <
		typename It1, typename It2, template <typename> typename Encoding
	> inline size_t count_codepoints(It1 beg, It2 end) {
		using _encoding = Encoding<typename std::iterator_traits<It1>::value_type>;

		size_t result = 0;
		for (; beg != end; _encoding::next_codepoint(beg, end), ++result) {
		}
		return result;
	}
	/// Skips an iterator forward, until the end is reached or a number of codepoints is skipped.
	/// Encodings can use this to fall back to default behavior, or implement their optimized versions.
	///
	/// \tparam Encoding The encoding used.
	/// \param beg The iterator pointing to the current position.
	///        It will hold the resulting iterator after returned.
	/// \param end Iterator past tne end of the string.
	/// \param num Maximum number of codepoints to skip.
	/// \return The actual number of codepoints skipped.
	template <
		typename It1, typename It2, template <typename> typename Encoding
	> inline size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
		using _encoding = Encoding<typename std::iterator_traits<It1>::value_type>;

		size_t i = 0;
		for (; i < num && beg != end; _encoding::next_codepoint(beg, end), ++i) {
		}
		return i;
	}

	/// UTF-8 encoding.
	///
	/// \tparam C Type of characters.
	template <typename C = char> class utf8 {
	public:
		using char_type = C; ///< The character type.

		/// Moves the iterator to the next codepoint, extracting the current codepoint,
		/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		/// \param v The value will hold the value of the current codepoint if the function returns \p true.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, char32_t &v) {
			C fc = *i;
			if ((fc & 0x80) == 0) {
				v = static_cast<char32_t>(fc);
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
		/// Moves the iterator to the next codepoint and returns whether it is valid.
		/// The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
			char fc = *i;
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
		/// next_codepoint(It1, It2) without error checking.
		/// Also, the caller doesn't need to check if <tt>i == end</tt>.
		template <typename It1, typename It2> inline static void next_codepoint_rough(It1 &i, It2 end) {
			for (; i != end && (*i & 0xC0) == 0x80; ++i) {
			}
		}
		/// Go back to the previous codepoint. Note that the result is only an estimate.
		///
		/// \param i The `current' iterator.
		/// \param beg The beginning of the string.
		template <typename It1, typename It2> inline static void previous_codepoint_rough(It1 &i, It2 beg) {
			for (; i != beg && (*i & 0xC0) == 0x80; --i) {
			}
		}
		/// Returns the UTF-8 representation of a Unicode codepoint.
		///
		/// \remark Since char may be either signed or unsigned,
		///         narrowing conversion is implementation-defined.
		///         There doesn't seem to be other ways to do this,
		///         and this is likely to work, so I'm leaving it this way.
		inline static std::basic_string<C> encode_codepoint(char32_t c) {
			std::basic_string<C> result;
			if (c < 0x80) {
				result.append(1, static_cast<C>(c));
			} else if (c < 0x800) {
				result.append(1, static_cast<C>(0xC0 | (c >> 6)));
				result.append(1, static_cast<C>(0x80 | (c & 0x3F)));
			} else if (c < 0x10000) {
				result.append(1, static_cast<C>(0xE0 | (c >> 12)));
				result.append(1, static_cast<C>(0x80 | ((c >> 6) & 0x3F)));
				result.append(1, static_cast<C>(0x80 | (c & 0x3F)));
			} else {
				result.append(1, static_cast<C>(0xF0 | (c >> 18)));
				result.append(1, static_cast<C>(0x80 | ((c >> 12) & 0x3F)));
				result.append(1, static_cast<C>(0x80 | ((c >> 6) & 0x3F)));
				result.append(1, static_cast<C>(0x80 | (c & 0x3F)));
			}
			return result;
		}

		/// Counts the number of codepoints in the given range.
		/// Uses codepad::count_codepoints to provide the default behavior.
		template <typename It1, typename It2> inline static size_t count_codepoints(It1 beg, It2 end) {
			return codepad::count_codepoints<It1, It2, codepad::utf8>(beg, end);
		}
		/// Skips an iterator forward, until the end is reached or a number of codepoints is skipped.
		/// Uses codepad::skip_codepoints to provide the default behavior.
		template <typename It1, typename It2> inline static size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
			return codepad::skip_codepoints<It1, It2, codepad::utf8>(beg, end, num);
		}
	};

	/// UTF-16 encoding.
	///
	/// \tparam C Type of characters.
	template <typename C = char16_t> class utf16 {
	public:
		using char_type = C; ///< The character type.

		static_assert(sizeof(C) >= 2, "invalid character type for utf-16");
		/// Moves the iterator to the next codepoint, extracting the current codepoint,
		/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		/// \param v The value will hold the value of the current codepoint if the function returns \p true.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, char32_t &v) {
			C fc = *i;
			if ((fc & 0xDC00) == 0xD800) {
				v = static_cast<char32_t>(fc & 0x03FF) << 10;
				if (++i == end || (*i & 0xDC00) != 0xDC00) {
					return false;
				}
				v |= *i & 0x03FF;
			} else {
				v = static_cast<char32_t>(fc);
				if ((fc & 0xDC00) == 0xDC00) {
					++i;
					return false;
				}
			}
			++i;
			return true;
		}
		/// Moves the iterator to the next codepoint and returns whether it is valid.
		/// The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
			C fc = *i;
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
		/// next_codepoint(It1, It2) without error checking.
		/// Also, the caller doesn't need to check if <tt>i == end</tt>.
		template <typename It1, typename It2> inline static void next_codepoint_rough(It1 &i, It2 end) {
			for (; i != end && (*i & 0xDC00) == 0xDC00; ++i) {
			}
		}
		/// Go back to the previous codepoint. Note that the result is only an estimate.
		///
		/// \param i The `current' iterator.
		/// \param beg The beginning of the string.
		template <typename It1, typename It2> inline static void previous_codepoint_rough(It1 &i, It2 beg) {
			for (; i != beg && (*i & 0xDC00) == 0xDC00; --i) {
			}
		}
		/// Returns the UTF-16 representation of a Unicode codepoint.
		inline static std::basic_string<C> encode_codepoint(char32_t c) {
			if (c < 0x10000) {
				return std::basic_string<C>(1, static_cast<C>(c));
			} else {
				char32_t mined = c - 0x10000;
				std::basic_string<C> res;
				res.append(1, static_cast<C>((mined >> 10) | 0xD800));
				res.append(1, static_cast<C>((mined & 0x03FF) | 0xDC00));
				return res;
			}
		}

		/// Counts the number of codepoints in the given range.
		/// Uses codepad::count_codepoints to provide the default behavior.
		template <typename It1, typename It2> inline static size_t count_codepoints(It1 beg, It2 end) {
			return codepad::count_codepoints<It1, It2, codepad::utf16>(beg, end);
		}
		/// Skips an iterator forward, until the end is reached or a number of codepoints is skipped.
		/// Uses codepad::skip_codepoints to provide the default behavior.
		template <typename It1, typename It2> inline static size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
			return codepad::skip_codepoints<It1, It2, codepad::utf16>(beg, end, num);
		}
	};

	/// UTF-32 encoding.
	///
	/// \tparam C Type of characters.
	template <typename C = char32_t> class utf32 {
	public:
		using char_type = C; ///< The character type.

		static_assert(sizeof(C) >= 3, "invalid character type for utf-32");
		/// Moves the iterator to the next codepoint, extracting the current codepoint,
		/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		/// \param v The value will hold the value of the current codepoint if the function returns \p true.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, char32_t &v) {
			static_cast<void>(end);
			v = static_cast<char32_t>(*i);
			++i;
			return is_valid_codepoint(v);
		}
		/// Moves the iterator to the next codepoint and returns whether it is valid.
		/// The caller is responsible of determining if <tt>i == end</tt>.
		///
		/// \param i The `current' iterator.
		/// \param end The end of the string.
		template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
			bool res = is_valid_codepoint(*i);
			++i;
			return res;
		}
		/// next_codepoint(It1, It2) without error checking.
		/// Also, the caller doesn't need to check if <tt>i == end</tt>.
		template <typename It1, typename It2> inline static void next_codepoint_rough(It1 &i, It2 end) {
			if (i != end) {
				++i;
			}
		}
		/// Go back to the previous codepoint. Note that the result is only an estimate.
		///
		/// \param i The `current' iterator.
		/// \param beg The beginning of the string.
		template <typename It1, typename It2> inline static void previous_codepoint_rough(It1 &i, It2 beg) {
			if (i != beg) {
				--i;
			}
		}
		/// Returns the UTF-32 representation of a Unicode codepoint.
		inline static std::basic_string<C> encode_codepoint(char32_t c) {
			return std::u32string(1, c);
		}

		/// Counts the number of codepoints in the given range.
		/// Uses the distance between the two iterators if possible, otherwise falls back to the default behavior.
		template <typename It1, typename It2> inline static size_t count_codepoints(It1 beg, It2 end) {
			if constexpr (std::is_same_v<It1, It2>) {
				return std::distance(beg, end);
			} else {
				return codepad::count_codepoints<It1, It2, codepad::utf32>(beg, end);
			}
		}
		/// Skips an iterator forward, until the end is reached or a number of codepoints is skipped.
		/// Directly increments the iterator if possible, otherwise falls back to the default behavior.
		template <typename It1, typename It2> inline static size_t skip_codepoints(It1 &beg, It2 end, size_t num) {
			if constexpr (std::is_same_v<It1, It2> && std::is_base_of_v<
				std::random_access_iterator_tag, typename std::iterator_traits<It1>::iterator_category
			>) {
				auto dist = std::min(num, static_cast<size_t>(end - beg));
				beg = beg + num;
				return dist;
			} else {
				return codepad::skip_codepoints<It1, It2, codepad::utf32>(beg, end, num);
			}
		}
	};

	/// Automaticly chooses a UTF encoding given the character type.
	template <typename C> class auto_utf;
	/// UTF-8 for \p char.
	template <> class auto_utf<char> : public utf8<char> {
	};
	/// UTF-8 for <tt>unsigned char</tt>.
	template <> class auto_utf<unsigned char> : public utf8<unsigned char> {
	};
	/// UTF-16 for \p char16_t.
	template <> class auto_utf<char16_t> : public utf16<char16_t> {
	};
	/// UTF-32 for \p char32_t.
	template <> class auto_utf<char32_t> : public utf32<char32_t> {
	};
#ifdef CP_PLATFORM_WINDOWS
	/// UTF-16 for \p wchar_t on windows.
	template <> class auto_utf<wchar_t> : public utf16<wchar_t> {
	};
#else
	/// UTF-32 for \p wchar_t on other platforms.
	template <> class auto_utf<wchar_t> : public utf32<wchar_t> {
	};
#endif

	/// Macro for defining UTF-8 string literals.
#define CP_STRLIT_U8(X) u8##X
/// Macro for defining UTF-32 string literals.
#define CP_STRLIT_U32(X) U##X

// encoding settings
#ifdef CP_USE_UTF8
/// Default character type and encoding.
	using char_t = char;
	/// Default definition for string literals.
#	define CP_STRLIT(X) CP_STRLIT_U8(X)
	/// The default encoding.
	using default_encoding = utf8<char_t>;
#elif defined(CP_USE_UTF32)
/// Default character type and encoding.
	using char_t = char32_t;
	/// Default definition for string literals.
#	define CP_STRLIT(X) CP_STRLIT_U32(X)
	/// The default encoding.
	using default_encoding = utf8<char_t>;
#endif
	/// STL string with default character type.
	using str_t = std::basic_string<char_t>;

	/// Settings and utilities of RapidJSON library.
	namespace json {
		// encoding settings
#ifdef CP_USE_UTF8
		/// Default encoding used by RapidJSON.
		using encoding = rapidjson::UTF8<char_t>;
#elif defined(CP_USE_UTF32)
		/// Default encoding used by RapidJSON.
		using encoding = rapidjson::UTF32<char_t>;
#endif

		/// RapidJSON type that holds a JSON object.
		using value_t = rapidjson::GenericValue<encoding>;
		/// RapidJSON type that holds a JSON object, and can be used to parse JSON text.
		using parser_value_t = rapidjson::GenericDocument<encoding>;

		/// Returns a STL string for a JSON string object.
		///
		/// \param v A JSON object. The caller must guarantee that the object is a string.
		/// \return A STL string whose content is the same as the JSON object.
		/// \remark This is the preferred way to get a string object's contents
		///         since it may contain null characters.
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
		/// Checks if the object has a member with a certain name which is of type \p T,
		/// then returns the value of the member if there is.
		///
		/// \param val A JSON object.
		/// \param name The desired name of the member.
		/// \param ret If the function returns true, then \p ret holds the value of the member.
		/// \return \p true if it has a member of type \p T.
		template <typename T> bool try_get(const value_t &val, const str_t &name, T &ret);
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

		/// \ref try_get with a default value.
		///
		/// \param v A JSON object.
		/// \param s The desired name of the member.
		/// \param def The default value if no such member is found.
		/// \return The value if one exists, or the default otherwise.
		template <typename T> inline T get_or_default(const value_t &v, const str_t &s, const T &def) {
			T res;
			if (try_get(v, s, res)) {
				return res;
			}
			return def;
		}
	}

	/// Struct for iterating over a series of codepoints in a string.
	///
	/// \tparam T The type of iterator.
	/// \tparam Encoding The encoding used.
	template <
		typename T, typename Encoding = auto_utf<typename std::iterator_traits<T>::value_type>
	> struct codepoint_iterator_base {
		template <typename U, typename Enc> friend struct codepoint_iterator_base;
	public:
		/// Default constructor.
		codepoint_iterator_base() = default;
		/// Constructs the iterator with a range of units.
		///
		/// \param beg First unit of the string.
		/// \param end Past the last unit of the string.
		/// \param cp Used when creating an iterator from part of a string which still needs to be
		///           treated as a whole, to indicate the number of codepoints before \p beg.
		codepoint_iterator_base(T beg, T end, size_t cp = 0) : _ptr(beg), _next(beg), _end(end), _cps(cp) {
			if (_next != _end) { // get the first codepoint
				_good = Encoding::next_codepoint(_next, _end, _cv);
			}
		}
		/// Constructs the iterator from another iterator with
		/// compatible underlying iterator types and the same encoding.
		template <typename U> codepoint_iterator_base(const codepoint_iterator_base<U, Encoding> &it) :
			_ptr(it._ptr), _next(it._next), _end(it._end), _cps(it._cps), _cv(it._cv), _good(it._good) {
		}

		/// Returns the current codepoint.
		char32_t operator*() const {
			return _cv;
		}

		/// Moves to the next codepoint.
		void next() {
			_ptr = _next;
			++_cps;
			if (_next != _end) {
				_good = Encoding::next_codepoint(_next, _end, _cv);
			}
		}

		/// Pre-increment.
		codepoint_iterator_base &operator++() {
			next();
			return *this;
		}
		/// Post-increment.
		codepoint_iterator_base operator++(int) {
			codepoint_iterator_base oldv = *this;
			++*this;
			return oldv;
		}

		/// Checks if the iterator is past the end of the string.
		bool at_end() const {
			return _ptr == _end;
		}
		/// Checks if the iterator will move past the end of the string after the next call to next().
		bool next_end() const {
			return _next == _end;
		}
		/// Checks if the representation of the current codepoint is valid.
		bool current_good() const {
			return _good;
		}
		/// Returns the current codepoint.
		char32_t current_codepoint() const {
			return _cv;
		}
		/// Returns the current codepoint position, i.e.,
		/// how many codepoints there are before the current position.
		size_t codepoint_position() const {
			return _cps;
		}
		/// Sets the current codepoint position.
		///
		/// \sa codepoint_position()
		void set_current_codepoint_position(size_t v) {
			_cps = v;
		}
		/// Returns how many units there are before the current position given a starting position.
		///
		/// \param beg The starting position.
		size_t unit_position(const T &beg) const {
			return _ptr - beg;
		}
		/// Returns the underlying iterator to the beginning of the current codepoint.
		const T &get_raw_iterator() const {
			return _ptr;
		}
		/// Returns the underlying iterator to the beginning of the next codepoint.
		const T &get_raw_next_iterator() const {
			return _next;
		}
	protected:
		T
			_ptr{}, ///< Iterator to the current position, i.e., beginning of the current codepoint.
			_next{}, ///< Iterator to the beginning of the next codepoint.
			_end{}; ///< Iterator past the end of the string.
		size_t _cps = 0; ///< Number of codepoints before \ref _ptr.
		char32_t _cv{}; ///< Value of the current codepoint.
		bool _good = false; ///< Indicates whether the underlying representation of the current codepoint is valid.
	};
	/// \ref codepoint_iterator_base specialiaztion of default string type.
	using string_codepoint_iterator = codepoint_iterator_base<str_t::const_iterator>;
	/// \ref codepoint_iterator_base specialization of default raw string type.
	using raw_codepoint_iterator = codepoint_iterator_base<const char_t*>;

	/// Convert a range of units from one encoding to another.
	/// The source encoding is determined by the type of its units.
	///
	/// \tparam To A string type such as \p std::basic_string.
	/// \tparam SrcEncoding The source encoding.
	/// \tparam DstEncoding The destination encoding.
	/// \param beg Iterator to the beginning of the string.
	/// \param end Iterator past the ending of the string.
	/// \remark This function automatically replaces invalid codepoints with the \ref replacement_character.
	template <
		typename To, typename It,
		typename SrcEncoding = auto_utf<typename std::iterator_traits<It>::value_type>,
		typename DstEncoding = auto_utf<typename To::value_type>
	> inline To convert_encoding(
		It beg, It end
	) {
		To result;
		char32_t c;
		while (beg != end) {
			if (!SrcEncoding::next_codepoint(beg, end, c)) {
				c = replacement_character;
			}
			result.append(DstEncoding::encode_codepoint(c));
		}
		return result;
	}
	/// \overload
	template <
		typename To, typename From,
		typename SrcEncoding = auto_utf<typename From::value_type>,
		typename DstEncoding = auto_utf<typename To::value_type>
	> inline To convert_encoding(
		const From &str
	) {
		if constexpr (std::is_same_v<SrcEncoding, DstEncoding>) {
			return To(str);
		} else {
			return convert_encoding<
				To, typename From::const_iterator, SrcEncoding, DstEncoding
			>(str.begin(), str.end());
		}
	}

	/// Converts a string to UTF-8 encoding.
	template <typename From> inline std::string convert_to_utf8(From &&f) {
		return convert_encoding<std::string>(std::forward<From>(f));
	}
	/// Converts a string to UTF-16 encoding.
	template <typename From> inline std::u16string convert_to_utf16(From &&f) {
		return convert_encoding<std::u16string>(std::forward<From>(f));
	}
	/// Converts a string to UTF-32 encoding.
	template <typename From> inline std::u32string convert_to_utf32(From &&f) {
		return convert_encoding<std::u32string>(std::forward<From>(f));
	}

	/// Converts a string to \ref default_encoding.
	///
	/// \tparam SrcEncoding The source encoding.
	template <
		typename Char, typename SrcEncoding = auto_utf<Char>
	> inline str_t convert_to_default_encoding(const std::basic_string<Char> &s) {
		return convert_encoding<str_t, std::basic_string<Char>, SrcEncoding, default_encoding>(s);
	}
	/// \overload
	template <
		typename It, typename SrcEncoding = auto_utf<typename std::iterator_traits<It>::value_type>
	> inline str_t convert_to_default_encoding(It beg, It end) {
		return convert_encoding<str_t, It, SrcEncoding, default_encoding>(beg, end);
	}

	/// Calls \p std::to_string to convert the param into a string,
	/// then converts the string to the current encoding.
	///
	/// \param t An object. Must be one of the type that \p std::to_string accepts.
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
