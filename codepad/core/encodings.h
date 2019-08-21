// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Encoding settings, and conversions between one another.
/// Currently supported encodings: UTF-8, UTF-16, UTF-32.

#include <cstddef>
#include <string>
#include <iterator>

namespace codepad {
	/// Specifies the byte order of words.
	enum class endianness : unsigned char {
		little_endian, ///< Little endian.
		big_endian ///< Big endian.
	};
	/// The endianness of the current system.
	constexpr endianness system_endianness = endianness::little_endian;

	/// Default definition for string literals.
#define CP_STRLIT(X) u8 ## X
	/// STL string with default character type.
	///
	/// \todo C++20: Use std::u8string when C++20.
	using str_t = std::basic_string<char>;
	/// STL string_view with default character type.
	///
	/// \todo C++20: Use std::u8string_view.
	using str_view_t = std::basic_string_view<char>;
	/// Type used to store codepoints. \p char32_t is not used because its range is 0~0x10FFFF, so it may not be able
	/// to correctly represent invalid codepoints.
	using codepoint = std::uint32_t;

	/// A template version of \p std::strlen().
	template <typename Char> inline size_t get_unit_count(const Char *cs) {
		size_t i = 0;
		for (; *cs; ++i, ++cs) {
		}
		return i;
	}

	/// Determines if a codepoint is a `new line' character.
	inline bool is_newline(codepoint c) {
		return c == '\n' || c == '\r';
	}
	/// Determines if a codepoint is a graphical char, i.e., is not blank.
	///
	/// \todo May not be complete.
	inline bool is_graphical_char(codepoint c) {
		return c != '\n' && c != '\r' && c != '\t' && c != ' ';
	}
	/// Implementation of various encodings. All implementations accept only byte sequences as input.
	namespace encodings {
		constexpr codepoint
			replacement_character = 0xFFFD, ///< Unicode replacement character.
			invalid_min = 0xD800, ///< Minimum code point value reserved by UTF-16.
			invalid_max = 0xDFFF, ///< Maximum code point value (inclusive) reserved by UTF-16.
			unicode_max = 0x10FFFF; ///< Maximum code point value (inclusive) of Unicode.

		/// Determines if a codepoint lies in the valid range of Unicode points.
		inline bool is_valid_codepoint(codepoint c) {
			return c < invalid_min || (c > invalid_max && c <= unicode_max);
		}

		/// UTF-8 encoding.
		///
		/// \sa https://en.wikipedia.org/wiki/UTF-8.
		class utf8 {
		public:
			constexpr static std::byte
				mask_1{0x80}, ///< Mask for detecting single-byte codepoints.
				sig_1{0x00}, ///< Expected masked value of single-byte codepoints.
				mask_2{0xE0}, ///< Mask for detecting bytes leading double-byte codepoints.
				sig_2{0xC0}, ///< Expected masked value of bytes leading double-byte codepoints.
				mask_3{0xF0}, ///< Mask for detecting triple-byte codepoints.
				sig_3{0xE0}, ///< Expected masked value of bytes leading triple-byte codepoints.
				mask_4{0xF8}, ///< Mask for detecting quadruple-byte codepoints.
				sig_4{0xF0}, ///< Expected masked value of bytes leading quadruple-byte codepoints.
				mask_cont{0xC0}, ///< Mask for detecting continuation bytes.
				sig_cont{0x80}; ///< Expected masked value of continuation bytes.

			/// Returns `UTF-8'.
			inline static str_view_t get_name() {
				return u8"UTF-8";
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-8.
			inline static size_t get_maximum_codepoint_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint to \p v, and returns whether
			/// it is valid. The caller is responsible of determining if <tt>i == end</tt>. If the codepoint is not
			/// valid, \p v will contain the byte that \p i initially points to, and \p i will be moved to point to the
			/// next byte.
			template <typename It1, typename It2> inline static bool next_codepoint(
				It1 &i, const It2 &end, codepoint &v
			) {
				std::byte fb = _get(i);
				if ((fb & mask_1) == sig_1) {
					v = static_cast<codepoint>(fb & ~mask_1);
				} else if ((fb & mask_2) == sig_2) {
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_2) << 6;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask_3) == sig_3) {
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_3) << 12;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 6;
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask_4) == sig_4) {
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_4) << 18;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 12;
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & mask_cont) << 6;
					if (++i == end || (_get(i) & mask_cont) != sig_cont) {
						--i;
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else {
					++i;
					v = static_cast<codepoint>(fb);
					return false;
				}
				++i;
				return true;
			}
			/// Moves the iterator to the next codepoint and returns whether it is valid. The caller is responsible
			/// of determining if <tt>i == end</tt>. If the codepoint is not valid, \p v will contain the byte that
			/// \p i initially points to, and \p i will be moved to point to the next byte.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				std::byte fb = _get(i);
				if ((fb & mask_1) != sig_1) {
					if ((fb & mask_2) == sig_2) {
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							return false;
						}
					} else if ((fb & mask_3) == sig_3) {
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							--i;
							return false;
						}
					} else if ((fb & mask_4) == sig_4) {
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							--i;
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != sig_cont) {
							--i;
							--i;
							return false;
						}
					} else {
						++i;
						return false;
					}
				}
				++i;
				return true;
			}
			/// Returns the UTF-8 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if (c < 0x80) {
					return {static_cast<std::byte>(c) & ~mask_1};
				}
				if (c < 0x800) {
					return {
						(static_cast<std::byte>(c >> 6) & ~mask_2) | sig_2,
						(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
					};
				}
				if (c < 0x10000) {
					return {
						(static_cast<std::byte>(c >> 12) & ~mask_3) | sig_3,
						(static_cast<std::byte>(c >> 6) & ~mask_cont) | sig_cont,
						(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
					};
				}
				return {
					(static_cast<std::byte>(c >> 18) & ~mask_4) | sig_4,
					(static_cast<std::byte>(c >> 12) & ~mask_cont) | sig_cont,
					(static_cast<std::byte>(c >> 6) & ~mask_cont) | sig_cont,
					(static_cast<std::byte>(c) & ~mask_cont) | sig_cont
				};
			}
		protected:
			/// Extracts an element (char, unsigned char, etc.) from the given iterator, and converts it into a
			/// \p std::byte.
			template <typename It> inline static std::byte _get(It &&it) {
				return static_cast<std::byte>(*it);
			}
		};

		/// UTF-16 encoding.
		template <endianness Endianness = system_endianness> class utf16 {
		public:
			/// Returns either `UTF-16 LE' or `UTF-16 BE', depending on the Endianness.
			inline static str_view_t get_name() {
				if constexpr (Endianness == endianness::little_endian) {
					return u8"UTF-16 LE";
				} else {
					return u8"UTF-16 BE";
				}
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-16.
			inline static size_t get_maximum_codepoint_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint,
			/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &v) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					v = word;
					return false;
				}
				if ((word & 0xDC00) == 0xD800) {
					if (i == end) {
						v = word;
						return false;
					}
					std::uint16_t w2;
					if (!_extract_word(i, end, w2)) {
						--i;
						v = word;
						return false;
					}
					if ((w2 & 0xDC00) != 0xDC00) {
						--i;
						--i;
						v = word;
						return false;
					}
					v = (static_cast<codepoint>(word & 0x03FF) << 10) | (w2 & 0x03FF);
				} else {
					v = word;
					if ((word & 0xDC00) == 0xDC00) {
						return false;
					}
				}
				return true;
			}
			/// Moves the iterator to the next codepoint and returns whether it is valid.
			/// The caller is responsible of determining if <tt>i == end</tt>.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					return false;
				}
				if ((word & 0xDC00) == 0xD800) {
					if (i == end) {
						return false;
					}
					std::uint16_t w2;
					if (!_extract_word(i, end, w2)) {
						--i;
						return false;
					}
					if ((w2 & 0xDC00) != 0xDC00) {
						--i;
						--i;
						return false;
					}
				} else {
					if ((word & 0xDC00) == 0xDC00) {
						return false;
					}
				}
				return true;
			}
			/// Returns the UTF-16 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if (c < 0x10000) {
					return _encode_word(static_cast<std::uint16_t>(c));
				}
				codepoint mined = c - 0x10000;
				return
					_encode_word(static_cast<std::uint16_t>((mined >> 10) | 0xD800)) +
					_encode_word(static_cast<std::uint16_t>((mined & 0x03FF) | 0xDC00));
			}
		protected:
			/// Extracts a two-byte word from the given range of bytes, with the specified endianness.
			///
			/// \return A boolean indicating whether it was successfully extracted. This operation fails only if
			///         there are not enough bytes.
			template <typename It1, typename It2> inline static bool _extract_word(
				It1 &i, It2 end, std::uint16_t &word
			) {
				auto b1 = static_cast<std::byte>(*i);
				if (++i == end) {
					word = static_cast<std::uint16_t>(b1);
					return false;
				}
				auto b2 = static_cast<std::byte>(*i);
				++i;
				if constexpr (Endianness == endianness::little_endian) {
					word = static_cast<std::uint16_t>(
						static_cast<std::uint16_t>(b1) | (static_cast<std::uint16_t>(b2) << 8)
						);
				} else {
					word = static_cast<std::uint16_t>(
						static_cast<std::uint16_t>(b2) | (static_cast<std::uint16_t>(b1) << 8)
						);
				}
				return true;
			}
			/// Rearranges the two bytes of the given word according to the current endianness.
			inline static std::basic_string<std::byte> _encode_word(std::uint16_t word) {
				if constexpr (Endianness == endianness::little_endian) {
					return {
						static_cast<std::byte>(word & 0xFF),
						static_cast<std::byte>(word >> 8)
					};
				} else {
					return {
						static_cast<std::byte>(word >> 8),
						static_cast<std::byte>(word & 0xFF)
					};
				}
			}
		};

		/*/// UTF-32 encoding.
		///
		/// \tparam C Type of characters.
		template <typename C = codepoint> class utf32 {
		public:
			using char_type = C; ///< The character type.

			static_assert(sizeof(C) >= 3, "invalid character type for utf-32");
			/// Moves the iterator to the next codepoint, extracting the current codepoint,
			/// and returns whether it is valid. The caller is responsible of determining if <tt>i == end</tt>.
			///
			/// \param i The `current' iterator.
			/// \param end The end of the string.
			/// \param v The value will hold the value of the current codepoint if the function returns \p true.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &v) {
				static_cast<void>(end);
				v = static_cast<codepoint>(*i);
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
			inline static std::basic_string<C> encode_codepoint(codepoint c) {
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
		};*/
	}
}
