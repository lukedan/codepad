// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous fundamental functionalities.

#include <filesystem>
#include <optional>
#include <cmath>

#include "../apigen_definitions.h"

#include "encodings.h"

namespace std {
	/// Specialize \p std::hash<std::filesystem::path> that std doesn't specialize for some reason.
	template <> struct hash<filesystem::path> {
		std::size_t operator()(const filesystem::path &p) const {
			return filesystem::hash_value(p);
		}
	};
}

namespace codepad {
	/// Indicates if all following bitwise operators are enabled for a type. Enum classes can create specializations
	/// to enable them.
	template <typename> struct enable_enum_bitwise_operators : public std::false_type {
	};
	/// Indicates if all following bitwise operators are enabled for a type.
	///
	/// \sa enable_enum_bitwise_operators
	template <typename T> constexpr static bool enable_enum_bitwise_operators_v =
		enable_enum_bitwise_operators<T>::value;

	/// Bitwise and for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator&(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) & static_cast<_base>(rhs));
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator|(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) | static_cast<_base>(rhs));
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator^(Enum lhs, Enum rhs) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(static_cast<_base>(lhs) ^ static_cast<_base>(rhs));
	}
	/// Bitwise not for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum
	> operator~(Enum v) {
		using _base = std::underlying_type_t<Enum>;
		return static_cast<Enum>(~static_cast<_base>(v));
	}

	/// Bitwise and for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator&=(Enum &lhs, Enum rhs) {
		return lhs = lhs & rhs;
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator|=(Enum &lhs, Enum rhs) {
		return lhs = lhs | rhs;
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator^=(Enum &lhs, Enum rhs) {
		return lhs = lhs ^ rhs;
	}


	/// Parses enums from strings. Specialize this class to provide parsing for a specific enum type.
	template <typename Enum> struct enum_parser {
		static std::optional<Enum> parse(str_view_t);
	};


	/// Used to obtain certain attributes of member pointers.
	template <typename> struct member_pointer_traits;
	/// Specialization for member object pointers.
	template <typename Owner, typename Val> struct member_pointer_traits<Val Owner::*> {
		using value_type = Val; ///< The type of the pointed-to object.
		using owner_type = Owner; ///< The type of the owner.
	};


	/// Information about a position in the code of codepad.
	struct code_position {
		/// Constructs the \ref code_position with the corresponding information.
		code_position(std::string_view fil, std::string_view func, int l) : file(fil), function(func), line(l) {
		}

		/// Prints a \ref code_position to a stream.
		friend std::ostream &operator<<(std::ostream &out, const code_position &cp) {
			return out << cp.function << " @" << cp.file << ":" << cp.line;
		}

		std::string_view
			file, ///< The file.
			function; ///< The function.
		int line = 0; ///< The line of the file.

		/// Equality.
		friend bool operator==(const code_position &lhs, const code_position &rhs) {
			return lhs.file == rhs.file && lhs.function == rhs.function && lhs.line == rhs.line;
		}
		/// Inequality.
		friend bool operator!=(const code_position &lhs, const code_position &rhs) {
			return !(lhs == rhs);
		}
	};
	/// A \ref codepad::code_position representing where it appears.
#define CP_HERE ::codepad::code_position(__FILE__, __func__, __LINE__)
}
namespace std {
	/// Hash specialization for \ref codepad::code_position.
	template <> struct hash<codepad::code_position> {
		/// The implementation.
		std::size_t operator()(const codepad::code_position &pos) const {
			std::size_t res = hash<int>()(pos.line);
			hash<string_view> viewhasher;
			res ^= viewhasher(pos.function) + 0x9e3779b9 + (res << 6) + (res >> 2);
			res ^= viewhasher(pos.file) + 0x9e3779b9 + (res << 6) + (res >> 2);
			return res;
		}
	};
}

namespace codepad {
	/// Color representation.
	///
	/// \tparam T Type of the four components. Must be a floating point type or <tt>unsigned char</tt>.
	template <typename T> struct APIGEN_EXPORT_RECURSIVE color {
		static_assert(std::is_same_v<T, unsigned char> || std::is_floating_point_v<T>, "invalid color component type");

		using value_type = T; ///< The type used to store all components.

		/// Maximum value for a component. 1.0 for floating point types, and 255 for <tt>unsigned char</tt>.
		constexpr static T max_value = std::is_floating_point_v<T> ? 1 : 255;

		/// Default constructor, initializes the color to white.
		color() = default;
		/// Initializes the color with given component values.
		color(T rr, T gg, T bb, T aa) : r(rr), g(gg), b(bb), a(aa) {
		}

		T
			r = max_value, ///< The red component.
			g = max_value, ///< The green component.
			b = max_value, ///< The blue component.
			a = max_value; ///< The alpha component.

		/// Returns the (approximately) same color with all components converted to another type.
		///
		/// \tparam U The target type of components.
		template <typename U> color<U> convert() const {
			if constexpr (!std::is_same_v<T, U>) {
				if constexpr (std::is_same_v<U, unsigned char>) {
					return color<U>(
						static_cast<U>(0.5 + r * 255.0), static_cast<U>(0.5 + g * 255.0),
						static_cast<U>(0.5 + b * 255.0), static_cast<U>(0.5 + a * 255.0)
						);
				} else if constexpr (std::is_same_v<T, unsigned char>) {
					return color<U>(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
				} else {
					return color<U>(
						static_cast<U>(r), static_cast<U>(g), static_cast<U>(b), static_cast<U>(a)
						);
				}
			} else {
				return *this;
			}
		}

		/// Equality.
		friend bool operator==(color<T> lhs, color<T> rhs) {
			return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
		}
		/// Inequality.
		friend bool operator!=(color<T> lhs, color<T> rhs) {
			return !(lhs == rhs);
		}

		/// Addition.
		color &operator+=(color val) {
			r += val.r;
			g += val.g;
			b += val.b;
			a += val.a;
			return *this;
		}
		/// Addition.
		friend color operator+(color lhs, color rhs) {
			return lhs += rhs;
		}

		/// Subtraction.
		color &operator-=(color val) {
			r -= val.r;
			g -= val.g;
			b -= val.b;
			a -= val.a;
			return *this;
		}
		/// Subtraction.
		friend color operator-(color lhs, color rhs) {
			return lhs -= rhs;
		}

		/// Scalar multiplication.
		color &operator*=(T val) {
			r *= val;
			g *= val;
			b *= val;
			a *= val;
			return *this;
		}
		/// Multiplication. Each component is multiplied by the corresponding component of \p val.
		color &operator*=(color val) {
			r *= val.r;
			g *= val.g;
			b *= val.b;
			a *= val.a;
			return *this;
		}
		/// Scalar multiplication.
		friend color operator*(color lhs, T rhs) {
			return lhs *= rhs;
		}
		/// Scalar multiplication.
		friend color operator*(T lhs, color rhs) {
			return rhs *= lhs;
		}
		/// Multiplication.
		///
		/// \sa operator*=(color)
		friend color operator*(color lhs, color rhs) {
			return lhs *= rhs;
		}

		/// Scalar division.
		color &operator/=(T val) {
			r /= val;
			g /= val;
			b /= val;
			a /= val;
			return *this;
		}
		/// Scalar division.
		friend color operator/(color lhs, T rhs) {
			return lhs /= rhs;
		}

		/// Converts the HSL representation of a color to RGB representation.
		/// Can only be used for floating point types.
		template <typename U = T> inline static std::enable_if_t<
			std::is_floating_point_v<U> && std::is_floating_point_v<T>, color
		> from_hsl(U h, U s, U l, U alpha = 1) {
			h = (h - 360 * std::floor(h / 360)) / 60;
			U
				c = (1 - std::abs(2 * l - 1)) * s,
				x = c * (1 - std::abs(std::fmod(h, 2) - 1));
			if (h < 1) {
				return color(c, x, 0, alpha);
			}
			if (h < 2) {
				return color(x, c, 0, alpha);
			}
			if (h < 3) {
				return color(0, c, x, alpha);
			}
			if (h < 4) {
				return color(0, x, c, alpha);
			}
			if (h < 5) {
				return color(x, 0, c, alpha);
			}
			return color(c, 0, x, alpha);
		}
	};
	/// Color whose components are of type \p double.
	using colord = color<double>;
	/// Color whose components are of type \p float.
	using colorf = color<float>;
	/// Color whose components are of type <tt>unsigned char</tt>.
	using colori = color<unsigned char>;

	/// Returns the index of the highest bit in the given integer. Returns 64 if \p v is 0.
	inline constexpr std::size_t high_bit_index(std::uint64_t v) {
		if (v == 0) {
			return 64;
		}
		std::size_t res = 0;
		if ((v & 0xFFFFFFFF00000000) == 0) {
			res += 32;
			v <<= 32;
		}
		if ((v & 0xFFFF000000000000) == 0) {
			res += 16;
			v <<= 16;
		}
		if ((v & 0xFF00000000000000) == 0) {
			res += 8;
			v <<= 8;
		}
		if ((v & 0xF000000000000000) == 0) {
			res += 4;
			v <<= 4;
		}
		if ((v & 0xC000000000000000) == 0) {
			res += 2;
			v <<= 2;
		}
		if ((v & 0x7000000000000000) == 0) {
			res += 1;
			v <<= 1;
		}
		return 63 - res;
	}
	/// Returns the index of the lowest bit in the given integer. Returns 64 if \p v is 0.
	inline constexpr std::size_t low_bit_index(std::uint64_t v) {
		if (v == 0) {
			return 64;
		}
		std::size_t res = 0;
		if ((v & 0x00000000FFFFFFFF) == 0) {
			res += 32;
			v >>= 32;
		}
		if ((v & 0x000000000000FFFF) == 0) {
			res += 16;
			v >>= 16;
		}
		if ((v & 0x00000000000000FF) == 0) {
			res += 8;
			v >>= 8;
		}
		if ((v & 0x000000000000000F) == 0) {
			res += 4;
			v >>= 4;
		}
		if ((v & 0x0000000000000003) == 0) {
			res += 2;
			v >>= 2;
		}
		if ((v & 0x0000000000000001) == 0) {
			res += 1;
			v >>= 1;
		}
		return res;
	}

	/// Linear interpolation.
	///
	/// \param from Returned if \p perc = 0.
	/// \param to Returned if \p perc = 1.
	/// \param perc Determines how close the return value is to \p to.
	/// \todo C++20: Use std implementation.
	template <typename T> inline constexpr T lerp(T from, T to, double perc) {
		return from + (to - from) * perc;
	}

	/// Gathers bits from a string and returns the result. Each bit is represented by a character.
	///
	/// \param list A list of character-bit relationships.
	/// \param str The string to gather bits from.
	template <typename T, typename C> inline T get_bitset_from_string(
		std::initializer_list<std::pair<C, T>> list, std::basic_string_view<C> str
	) {
		T result{};
		for (C c : str) {
			for (auto j = list.begin(); j != list.end(); ++j) {
				if (c == j->first) {
					result |= j->second;
					break;
				}
			}
		}
		return result;
	}


	/// Initializes the program by calling \ref os::initialize first and then performing several other
	/// initialization steps.
	void initialize(int, char**);
}
