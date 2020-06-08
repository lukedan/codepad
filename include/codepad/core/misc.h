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
		static std::optional<Enum> parse(std::u8string_view);
	};


	/// Used to obtain certain attributes of member pointers.
	template <typename> struct member_pointer_traits;
	/// Specialization for member object pointers.
	template <typename Owner, typename Val> struct member_pointer_traits<Val Owner::*> {
		using value_type = Val; ///< The type of the pointed-to object.
		using owner_type = Owner; ///< The type of the owner.
	};


	/// Information about a position in the code of codepad.
	///
	/// \todo Use std::source_location.
	struct code_position {
		/// Constructs the \ref code_position with the corresponding information.
		code_position(const char *fil, const char *func, int l) : file(fil), function(func), line(l) {
		}

		/// Prints a \ref code_position to a stream.
		friend std::ostream &operator<<(std::ostream &out, const code_position &cp) {
			return out << cp.function << " @" << cp.file << ":" << cp.line;
		}

		const char
			*file = nullptr, ///< The file.
			*function = nullptr; ///< The function.
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
	/// Gathers bits from a string and returns the result. Each bit is represented by a character. If the string
	/// starts with the specified `negate' character, then the result will contain all but the specified bits.
	template <typename T, T All = T::all, typename C> inline T get_bitset_from_string_with_negate(
		std::initializer_list<std::pair<C, T>> list, std::basic_string_view<C> str, C negate = static_cast<C>('-')
	) {
		if (!str.empty() && str[0] == negate) {
			str.remove_prefix(1);
			return All ^ get_bitset_from_string(list, str);
		}
		return get_bitset_from_string(list, str);
	}


	/// CRTP base for a RAII wrapper of a reference-counted handle.
	template <typename Derived, typename T> struct reference_counted_handle {
	public:
		/// Default constructor.
		reference_counted_handle() = default;
		/// Copy constructor.
		reference_counted_handle(const reference_counted_handle &src) : _handle(src._handle) {
			_check_add_ref();
		}
		/// Move constructor.
		reference_counted_handle(reference_counted_handle &&src) noexcept : _handle(src._handle) {
			src._handle = nullptr;
		}
		/// Copy assignment.
		reference_counted_handle &operator=(const reference_counted_handle &src) {
			set_share(src._handle);
			return *this;
		}
		/// Move assignment.
		reference_counted_handle &operator=(reference_counted_handle &&src) noexcept {
			set_give(src._handle);
			src._handle = nullptr;
			return *this;
		}
		/// Calls \ref _check_release().
		~reference_counted_handle() {
			_check_release();
		}

		/// Sets the underlying pointer and increment the reference count.
		reference_counted_handle &set_share(T handle) {
			_check_release();
			_handle = handle;
			_check_add_ref();
			return *this;
		}
		/// Sets the underlying pointer without incrementing the reference count.
		reference_counted_handle &set_give(T handle) {
			_check_release();
			_handle = handle;
			return *this;
		}
		/// Releases the currently holding object.
		reference_counted_handle &reset() {
			return set_give(Derived::empty_handle);
		}

		/// Returns the underlying pointer.
		T get() const {
			return _handle;
		}
		/// Convenience operator.
		T operator->() const {
			return get();
		}

		/// Returns whether this wrapper holds no objects.
		bool empty() const {
			return _handle == Derived::empty_handle;
		}
		/// Returns whether this brush has any content.
		explicit operator bool() const {
			return !empty();
		}
	protected:
		T _handle{}; ///< The underlying handle.

		/// Adds a reference to the handle by calling \p _do_add_ref() if necessary.
		void _check_add_ref() {
			if (!empty()) {
				static_cast<Derived*>(this)->_do_add_ref();
			}
		}
		/// Removes a reference to the handle by calling \p _do_release() if necessary. This function also resets
		/// \ref _handle.
		void _check_release() {
			if (!empty()) {
				static_cast<Derived*>(this)->_do_release();
				_handle = nullptr;
			}
		}
	};


	/// Initializes the program by calling \ref os::initialize first and then performing several other
	/// initialization steps.
	void initialize(int, char**);
}
