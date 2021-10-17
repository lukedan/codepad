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
#include <bit>

#include "unicode/common.h"

namespace codepad {
	/// A templated version of \p std::strlen().
	template <typename Char> inline std::size_t get_unit_count(const Char *cs) {
		std::size_t i = 0;
		for (; *cs; ++i, ++cs) {
		}
		return i;
	}

	/// A \p std::basic_string whose elements are of type \p std::byte. This differs from \ref byte_array in that
	/// this may contain optimizations designed for strings, e.g., short string optimization.
	using byte_string = std::basic_string<std::byte>;
	/// A \p std::vector whose elements are of type \p std::byte.
	using byte_array = std::vector<std::byte>;

	/// Implementation of various encodings. All implementations accept only byte sequences as input.
	namespace encodings {
		/// Returns \p true if the given position is aligned.
		[[nodiscard]] inline bool is_position_aligned(std::size_t pos, std::size_t align) {
			return pos % align == 0;
		}
		/// \overload
		template <typename Encoding> inline bool is_position_aligned(std::size_t pos) {
			return is_position_aligned(pos, Encoding::get_word_length());
		}
		/// Aligns the given iterator to the given word boundary by moving it forward as little bytes as possible.
		template <typename It> inline codepoint align_iterator(It &it, const It &begin, std::size_t align) {
			codepoint res = 0;
			std::size_t count = (it - begin) % align;
			for (std::size_t i = 0; i < count; ++i) {
				--it;
				res = (res << 8) | static_cast<codepoint>(*it);
			}
			return res;
		}
		/// \overload
		template <
			typename Encoding, typename It
		> inline codepoint align_iterator(It &i, const It &begin) {
			return align_iterator<It>(i, begin, Encoding::get_word_length());
		}

		/// UTF-8 encoding.
		///
		/// \sa https://en.wikipedia.org/wiki/UTF-8.
		class utf8 {
		public:
			constexpr static std::array<std::byte, 4>
				/// Masks for detecting codepoints with length 1, 2, 3, and 4.
				mask{ { std::byte(0x80), std::byte(0xE0), std::byte(0xF0), std::byte(0xF8) } },
				/// Expected pattern starting byte of codepoints with length 1, 2, 3, and 4, masked with \ref mask.
				patt{ { std::byte(0x00), std::byte(0xC0), std::byte(0xE0), std::byte(0xF0) } };
			constexpr static std::byte
				mask_cont{0xC0}, ///< Mask for detecting continuation bytes.
				patt_cont{0x80}; ///< Expected masked value of continuation bytes.

			/// Returns `UTF-8'.
			inline static std::u8string_view get_name() {
				return u8"UTF-8";
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-8.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}
			/// Returns the length of a single word.
			inline static std::size_t get_word_length() {
				return 1;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint to \p v, and returns
			/// whether it is valid. The caller is responsible of determining if <tt>i == end</tt> before this call.
			/// If the codepoint is not valid, \p v will contain the byte that \p i initially points to, and \p i
			/// will be moved to point to the next byte. This function does not check if the resulting codepoint lies
			/// between \ref invalid_min and \ref invalid_max.
			template <typename It> inline static bool next_codepoint(It &i, const It &end, codepoint &v) {
				std::byte fb = _get(i);
				if ((fb & mask[0]) == patt[0]) {
					v = static_cast<codepoint>(fb & ~mask[0]);
				} else if ((fb & mask[1]) == patt[1]) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask[1]) << 6;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask[2]) == patt[2]) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask[2]) << 12;
					v |= static_cast<codepoint>(_get(i) & ~mask_cont) << 6;
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						--i;
						v = static_cast<codepoint>(fb);
						return false;
					}
					v |= static_cast<codepoint>(_get(i) & ~mask_cont);
				} else if ((fb & mask[3]) == patt[3]) {
					if (++i == end || (_get(i) & mask_cont) != patt_cont) {
						v = static_cast<codepoint>(fb);
						return false;
					}
					v = static_cast<codepoint>(fb & ~mask[3]) << 18;
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
			template <typename It> inline static bool next_codepoint(It &i, const It &end) {
				std::byte fb = _get(i);
				if ((fb & mask[0]) != patt[0]) {
					if ((fb & mask[1]) == patt[1]) {
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							return false;
						}
					} else if ((fb & mask[2]) == patt[2]) {
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							return false;
						}
						if (++i == end || (_get(i) & mask_cont) != patt_cont) {
							--i;
							return false;
						}
					} else if ((fb & mask[3]) == patt[3]) {
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

			/// Moves the iterator to the previous codepoint, and stores the value of the codepoint in the given
			/// parameter. The caller is responsible of checking that <tt>i != beg</tt>.
			///
			/// \return Whether a valid codepoint was extracted.
			template <typename It> inline static bool previous_codepoint(It &i, const It &beg, codepoint &cp) {
				std::array<std::byte, 3> continuation_bytes;
				std::size_t count = 0; // number of continuation bytes
				while (true) {
					--i;
					if ((_get(i) & mask_cont) != patt_cont) {
						break;
					}
					if (i == beg || count == 3) {
						// invalid - first byte is a continuation byte, or too many continuation bytes
						for (; count > 0; ++i, --count) { // move the iterator back to the last byte
						}
						cp = static_cast<codepoint>(_get(i));
						return false;
					}
					continuation_bytes[count] = _get(i);
					++count;
				}
				if ((_get(i) & mask[count]) != patt[count]) { // invalid sequence
					for (; count > 0; ++i, --count) { // move the iterator back to the last byte
					}
					cp = static_cast<codepoint>(_get(i));
					return false;
				}
				// reconstruct the codepoint
				if (count == 0) {
					cp = static_cast<codepoint>(_get(i) & ~mask[0]);
					return unicode::is_valid_codepoint(cp);
				}
				cp = static_cast<codepoint>(continuation_bytes[0] & ~mask_cont);
				if (count == 1) {
					cp |= static_cast<codepoint>(_get(i) & ~mask[1]) << 6;
					return unicode::is_valid_codepoint(cp);
				}
				cp |= static_cast<codepoint>(continuation_bytes[1] & ~mask_cont) << 6;
				if (count == 2) {
					cp |= static_cast<codepoint>(_get(i) & ~mask[2]) << 12;
					return unicode::is_valid_codepoint(cp);
				}
				cp |= static_cast<codepoint>(continuation_bytes[2] & ~mask_cont) << 12;
				cp |= static_cast<codepoint>(_get(i) & ~mask[3]) << 18;
				return unicode::is_valid_codepoint(cp);
			}
			/// \overload
			template <typename It> inline static bool previous_codepoint(It &i, const It &beg) {
				std::size_t count = 0; // number of continuation bytes
				while (true) {
					--i;
					if ((_get(i) & mask_cont) != patt_cont) {
						break;
					}
					if (i == beg || count == 3) {
						// invalid - first byte is a continuation byte, or too many continuation bytes
						for (; count > 0; ++i, --count) { // move the iterator back to the last byte
						}
						return false;
					}
					++count;
				}
				if ((_get(i) & mask[count]) != patt[count]) { // invalid sequence
					for (; count > 0; ++i, --count) { // move the iterator back to the last byte
					}
					return false;
				}
				return true;
			}

			/// Returns the UTF-8 representation of a Unicode codepoint.
			template <
				typename Char = std::byte
			> inline static std::basic_string<Char> encode_codepoint(codepoint c) {
				if (c < 0x80) {
					return {std::bit_cast<Char>(static_cast<std::byte>(c) & ~mask[0])};
				}
				if (c < 0x800) {
					return {
						std::bit_cast<Char>((static_cast<std::byte>(c >> 6) & ~mask[1]) | patt[1]),
						std::bit_cast<Char>((static_cast<std::byte>(c) & ~mask_cont) | patt_cont)
					};
				}
				if (c < 0x10000) {
					return {
						std::bit_cast<Char>((static_cast<std::byte>(c >> 12) & ~mask[2]) | patt[2]),
						std::bit_cast<Char>((static_cast<std::byte>(c >> 6) & ~mask_cont) | patt_cont),
						std::bit_cast<Char>((static_cast<std::byte>(c) & ~mask_cont) | patt_cont)
					};
				}
				return {
					std::bit_cast<Char>((static_cast<std::byte>(c >> 18) & ~mask[3]) | patt[3]),
					std::bit_cast<Char>((static_cast<std::byte>(c >> 12) & ~mask_cont) | patt_cont),
					std::bit_cast<Char>((static_cast<std::byte>(c >> 6) & ~mask_cont) | patt_cont),
					std::bit_cast<Char>((static_cast<std::byte>(c) & ~mask_cont) | patt_cont)
				};
			}
			/// Shorthand for \ref encode_codepoint<char8_t>().
			inline static std::u8string encode_codepoint_u8(codepoint c) {
				return encode_codepoint<char8_t>(c);
			}
			/// Shorthand for \ref encode_codepoint<char>().
			inline static std::string encode_codepoint_char(codepoint c) {
				return encode_codepoint<char>(c);
			}
		protected:
			/// Extracts an element (char, unsigned char, etc.) from the given iterator, and converts it into a
			/// \p std::byte.
			template <typename It> inline static std::byte _get(It &&it) {
				return static_cast<std::byte>(*it);
			}
		};

		/// UTF-16 encoding.
		template <std::endian Endianness = std::endian::native> class utf16 {
		public:
			static_assert(Endianness == std::endian::little || Endianness == std::endian::big, "Unknown endianess");
			constexpr static std::uint16_t
				mask_pair = 0xDC00, ///< Mask for detecting surrogate pairs.
				patt_pair = 0xD800, ///< Expected masked value of the first unit of the surrogate pair.
				patt_pair_second = 0xDC00, ///< Expected masked value of the second unit of the surrogate pair.
				mask_data = 0x03FF; ///< Mask used for extracting data from surrogate pair words.

			/// Returns either `UTF-16 LE' or `UTF-16 BE', depending on the Endianness.
			inline static std::u8string_view get_name() {
				if constexpr (Endianness == std::endian::little) {
					return u8"UTF-16 LE";
				} else {
					return u8"UTF-16 BE";
				}
			}
			/// Maximum 4 bytes for any codepoint encoded in UTF-16.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}
			/// Returns the length of a single word.
			inline static std::size_t get_word_length() {
				return 2;
			}

			/// Moves the iterator to the next codepoint, extracting the current codepoint, and returns whether it is
			/// valid. The caller is responsible of determining if <tt>i == end</tt> before the call.
			template <typename It> inline static bool next_codepoint(It &i, const It end, codepoint &v) {
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
					v = (static_cast<codepoint>(word & mask_data) << 10) | (w2 & mask_data);
				} else {
					v = word;
					if ((word & mask_pair) == patt_pair_second) {
						return false;
					}
				}
				return true;
			}
			/// \overload
			template <typename It> inline static bool next_codepoint(It &i, const It &end) {
				std::uint16_t word;
				if (!_extract_word(i, end, word)) {
					return false;
				}
				if ((word & mask_pair) == patt_pair) {
					if (i == end) {
						return false;
					}
					std::uint16_t w2;
					if (!_extract_word(i, end, w2)) {
						--i;
						return false;
					}
					if ((w2 & mask_pair) != patt_pair_second) {
						--i;
						--i;
						return false;
					}
				} else {
					if ((word & mask_pair) == patt_pair_second) {
						return false;
					}
				}
				return true;
			}

			/// Moves the iterator to the previous codepoint, and stores the value of the codepoint in the given
			/// parameter. The caller is responsible of checking that <tt>i != beg</tt>.
			///
			/// \return Whether a valid codepoint was extracted.
			template <typename It> inline static bool previous_codepoint(It &i, const It &beg, codepoint &cp) {
				std::uint16_t word = _extract_word_backwards(i);
				if ((word & mask_pair) == patt_pair_second) {
					if (i == beg) {
						cp = word;
						return false;
					}
					std::uint16_t prev_word = _extract_word_backwards(i);
					if ((prev_word & mask_pair) != patt_pair) {
						++i;
						++i;
						cp = word;
						return false;
					}
					cp = (static_cast<codepoint>(prev_word & mask_data) << 10) | (word & mask_data);
					return true;
				}
				cp = word;
				return (word & mask_pair) != patt_pair;
			}
			/// \overload
			template <typename It> inline static bool previous_codepoint(It &i, const It &beg) {
				std::uint16_t word = _extract_word_backwards(i);
				if ((word & mask_pair) == patt_pair_second) {
					if (i == beg) {
						return false;
					}
					std::uint16_t prev_word = _extract_word_backwards(i);
					if ((prev_word & mask_pair) != patt_pair) {
						++i;
						++i;
						return false;
					}
					return true;
				}
				return (word & mask_pair) != patt_pair;
			}

			/// Returns the UTF-16 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if (c < 0x10000) {
					return _encode_word(static_cast<std::uint16_t>(c));
				}
				codepoint mined = c - 0x10000;
				return
					_encode_word(static_cast<std::uint16_t>((mined >> 10) | patt_pair)) +
					_encode_word(static_cast<std::uint16_t>((mined & mask_data) | patt_pair_second));
			}
		protected:
			/// Extracts a two-byte word from the given range of bytes, with the specified endianness.
			///
			/// \return A boolean indicating whether it was successfully extracted. This operation fails only if
			///         there are not enough bytes.
			template <typename It> inline static bool _extract_word(
				It &i, const It &end, std::uint16_t &word
			) {
				auto b1 = static_cast<std::byte>(*i);
				if (++i == end) {
					word = static_cast<std::uint16_t>(b1);
					return false;
				}
				auto b2 = static_cast<std::byte>(*i);
				++i;
				if constexpr (Endianness == std::endian::little) {
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
			/// Extracts a word going backwards.
			template <typename It> [[nodiscard]] inline static std::uint16_t _extract_word_backwards(It &i) {
				auto b2 = static_cast<std::byte>(*--i);
				auto b1 = static_cast<std::byte>(*--i);
				if constexpr (Endianness == std::endian::little) {
					return static_cast<std::uint16_t>(
						static_cast<std::uint16_t>(b1) | (static_cast<std::uint16_t>(b2) << 8)
					);
				} else {
					return static_cast<std::uint16_t>(
						static_cast<std::uint16_t>(b2) | (static_cast<std::uint16_t>(b1) << 8)
					);
				}
			}
			/// Rearranges the two bytes of the given word according to the current endianness.
			inline static std::basic_string<std::byte> _encode_word(std::uint16_t word) {
				if constexpr (Endianness == std::endian::little) {
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
		template <std::endian Endianness = std::endian::native> class utf32 {
		public:
			static_assert(Endianness == std::endian::little || Endianness == std::endian::big, "Unknown endianess");
			/// Returns either `UTF-32 LE' or `UTF-32 BE', depending on the Endianness.
			inline static std::u8string_view get_name() {
				if constexpr (Endianness == std::endian::little) {
					return u8"UTF-32 LE";
				} else {
					return u8"UTF-32 BE";
				}
			}
			/// 4 bytes of a codepoint.
			inline static std::size_t get_maximum_codepoint_length() {
				return 4;
			}
			/// Returns the length of a single word.
			inline static std::size_t get_word_length() {
				return 4;
			}

			/// Moves the iterator to the next codepoint and returns whether it is valid.
			/// The caller is responsible of determining if <tt>i == end</tt>.
			///
			/// \param i The `current' iterator.
			/// \param end The end of the string.
			template <typename It> inline static bool next_codepoint(It &i, const It &end, codepoint &c) {
				std::array<std::byte, 4> data;
				std::fill(data.begin(), data.end(), static_cast<std::byte>(0));
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
				c = _make_codepoint(data);
				return result && unicode::is_valid_codepoint(c);
			}
			/// \overload
			template <typename It> inline static bool next_codepoint(It &i, const It &end) {
				if (++i != end) {
					if (++i != end) {
						if (++i != end) {
							++i;
							return true;
						}
					}
				}
				return false;
			}

			/// Moves the iterator to the previous codepoint, and stores the value of the codepoint in the given
			/// parameter.
			///
			/// \return Whether a valid codepoint was extracted.
			template <typename It> inline static bool previous_codepoint(It &i, const It&, codepoint &cp) {
				std::array<std::byte, 4> data;
				std::fill(data.begin(), data.end(), static_cast<std::byte>(0));
				data[3] = *--i;
				data[2] = *--i;
				data[1] = *--i;
				data[0] = *--i;
				cp = _make_codepoint(data);
				return unicode::is_valid_codepoint(cp);
			}
			/// \overload
			template <typename It> inline static bool previous_codepoint(It &i, const It&) {
				--i;
				--i;
				--i;
				--i;
				return true;
			}

			/// Returns the UTF-32 representation of a Unicode codepoint.
			inline static std::basic_string<std::byte> encode_codepoint(codepoint c) {
				if constexpr (Endianness == std::endian::little) {
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
		protected:
			/// Constructs a codepoint from the given bytes, respecting the endianness.
			[[nodiscard]] inline static codepoint _make_codepoint(const std::array<std::byte, 4> &bytes) {
				if constexpr (Endianness == std::endian::little) {
					return
						static_cast<codepoint>(bytes[0]) |
						(static_cast<codepoint>(bytes[1]) << 8) |
						(static_cast<codepoint>(bytes[2]) << 16) |
						(static_cast<codepoint>(bytes[3]) << 24);
				} else {
					return
						(static_cast<codepoint>(bytes[0]) << 24) |
						(static_cast<codepoint>(bytes[1]) << 16) |
						(static_cast<codepoint>(bytes[2]) << 8) |
						static_cast<codepoint>(bytes[3]);
				}
			}
		};
	}
}
