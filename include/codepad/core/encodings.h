// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Encoding implementations for UTF-8, UTF-16, and UTF-32.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <iterator>

#include "unicode/common.h"

namespace codepad {
	/// Specifies the byte order of words.
	enum class endianness : unsigned char {
		little_endian, ///< Little endian.
		big_endian ///< Big endian.
	};
	/// The endianness of the current system.
	constexpr endianness system_endianness = endianness::little_endian;

	/// A templated version of \p std::strlen().
	template <typename Char> inline std::size_t get_unit_count(const Char *cs) {
		std::size_t i = 0;
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
		/// UTF-8 encoding.
		///
		/// \sa https://en.wikipedia.org/wiki/UTF-8.
		class utf8 {
		public:
			constexpr static std::byte
				mask_1{0x80}, ///< Mask for detecting single-byte codepoints.
				mask_2{0xE0}, ///< Mask for detecting bytes leading double-byte codepoints.
				mask_3{0xF0}, ///< Mask for detecting triple-byte codepoints.
				mask_4{0xF8}, ///< Mask for detecting quadruple-byte codepoints.
				mask_cont{0xC0}, ///< Mask for detecting continuation bytes.

				patt_1{0x00}, ///< Expected masked value of single-byte codepoints.
				patt_2{0xC0}, ///< Expected masked value of bytes leading double-byte codepoints.
				patt_3{0xE0}, ///< Expected masked value of bytes leading triple-byte codepoints.
				patt_4{0xF0}, ///< Expected masked value of bytes leading quadruple-byte codepoints.
				patt_cont{0x80}; ///< Expected masked value of continuation bytes.

			/// Returns `UTF-8'.
			inline static std::u8string_view get_name() {
				return u8"UTF-8";
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-8.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint to \p v, and returns
			/// whether it is valid. The caller is responsible of determining if <tt>i == end</tt> before this call.
			/// If the codepoint is not valid, \p v will contain the byte that \p i initially points to, and \p i
			/// will be moved to point to the next byte. This function does not check if the resulting codepoint lies
			/// between \ref invalid_min and \ref invalid_max.
			template <typename It1, typename It2> inline static bool next_codepoint(
				It1 &i, const It2 &end, codepoint &v
			) {
				std::byte fb = _get(i);
				if ((fb & mask_1) == patt_1) {
					v = static_cast<codepoint>(fb & ~mask_1);
				} else if ((fb & mask_2) == patt_2) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_2) << 6;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask_3) == patt_3) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_3) << 12;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 6;
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask_4) == patt_4) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask_4) << 18;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 12;
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 6;
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
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
				return unicode::is_valid_codepoint(v);
			}
			/// \overload
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end) {
				std::byte fb = _get(i);
				if ((fb & mask_1) != patt_1) {
					if ((fb & mask_2) == patt_2) {
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							return false;
						}
					} else if ((fb & mask_3) == patt_3) {
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							--i;
							return false;
						}
					} else if ((fb & mask_4) == patt_4) {
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							--i;
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
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
						(static_cast<std::byte>(c >> 6) & ~mask_2) | patt_2,
						(static_cast<std::byte>(c) & ~mask_cont) | patt_cont
					};
				}
				if (c < 0x10000) {
					return {
						(static_cast<std::byte>(c >> 12) & ~mask_3) | patt_3,
						(static_cast<std::byte>(c >> 6) & ~mask_cont) | patt_cont,
						(static_cast<std::byte>(c) & ~mask_cont) | patt_cont
					};
				}
				return {
					(static_cast<std::byte>(c >> 18) & ~mask_4) | patt_4,
					(static_cast<std::byte>(c >> 12) & ~mask_cont) | patt_cont,
					(static_cast<std::byte>(c >> 6) & ~mask_cont) | patt_cont,
					(static_cast<std::byte>(c) & ~mask_cont) | patt_cont
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
			constexpr static std::uint16_t
				mask_pair = 0xDC00, ///< Mask for detecting surrogate pairs.
				patt_pair = 0xD800, ///< Expected masked value of the first unit of the surrogate pair.
				patt_pair_second = 0xDC00; ///< Expected masked value of the second unit of the surrogate pair.

			/// Returns either `UTF-16 LE' or `UTF-16 BE', depending on the Endianness.
			inline static std::u8string_view get_name() {
				if constexpr (Endianness == endianness::little_endian) {
					return u8"UTF-16 LE";
				} else {
					return u8"UTF-16 BE";
				}
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-16.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint, and returns whether it is
			/// valid. The caller is responsible of determining if <tt>i == end</tt> before the call.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &v) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					v = word;
					return false;
				}
				if ((word & mask_pair) == patt_pair) {
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
					if ((w2 & mask_pair) != patt_pair_second) {
						--i;
						--i;
						v = word;
						return false;
					}
					v = (static_cast<codepoint>(word & 0x03FF) << 10) | (w2 & 0x03FF);
				} else {
					v = word;
					if ((word & mask_pair) == patt_pair_second) {
						return false;
					}
				}
				return unicode::is_valid_codepoint(v);
			}
			/// \overload
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

		/// UTF-32 encoding.
		///
		/// \tparam C Type of characters.
		template <endianness Endianness = system_endianness> class utf32 {
		public:
			/// Returns either `UTF-32 LE' or `UTF-32 BE', depending on the Endianness.
			inline static std::u8string_view get_name() {
				if constexpr (Endianness == endianness::little_endian) {
					return "UTF-32 LE";
				} else {
					return "UTF-32 BE";
				}
			}
			/// 4 bytes of a codepoint.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint and returns whether it is valid.
			/// The caller is responsible of determining if <tt>i == end</tt>.
			///
			/// \param i The `current' iterator.
			/// \param end The end of the string.
			template <typename It1, typename It2> inline static bool next_codepoint(It1 &i, It2 end, codepoint &c) {
				std::array<std::byte, 4> data{ { 0, 0, 0, 0 } };
				bool result = false;
				data[0] = static_cast<std::byte>(*i);
				if (++i != end) {
					data[1] = static_cast<std::byte>(*i);
					if (++i != end) {
						data[2] = static_cast<std::byte>(*i);
						if (++i != end) {
							data[3] = static_cast<std::byte>(*i);
							result = true;
							++i;
						}
					}
				}
				if constexpr (Endianness == endianness::little_endian) {
					c =
						static_cast<codepoint>(data[0]) |
						(static_cast<codepoint>(data[1]) << 8) |
						(static_cast<codepoint>(data[2]) << 16) |
						(static_cast<codepoint>(data[3]) << 24);
				} else {
					c =
						(static_cast<codepoint>(data[0]) << 24) |
						(static_cast<codepoint>(data[1]) << 16) |
						(static_cast<codepoint>(data[2]) << 8) |
						static_cast<codepoint>(data[3]);
				}
				return unicode::is_valid_codepoint(c);
			}
			/// \overload
			template <typename It1, typename It2> inline static void next_codepoint(It1 &i, It2 end) {
				if (++i != end) {
					if (++i != end) {
						if (++i != end) {
							++i;
						}
					}
				}
			}
			/// Returns the UTF-32 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if constexpr (Endianness == endianness::little_endian) {
					return {
						static_cast<std::byte>(c & 0xFF),
						static_cast<std::byte>((c >> 8) & 0xFF),
						static_cast<std::byte>((c >> 16) & 0xFF),
						static_cast<std::byte>((c >> 24) & 0xFF)
					};
				} else {
					return {
						static_cast<std::byte>((c >> 24) & 0xFF),
						static_cast<std::byte>((c >> 16) & 0xFF),
						static_cast<std::byte>((c >> 8) & 0xFF),
						static_cast<std::byte>(c & 0xFF)
					};
				}
			}
		};
	}
}
