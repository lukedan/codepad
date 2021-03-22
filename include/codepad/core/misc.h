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


	/// Combines two hash values.
	[[nodiscard]] constexpr std::size_t combine_hashes(std::size_t a, std::size_t b) {
		return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
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


	/// Hash used by \p std::unordered_ containers to allow for \p std::basic_string_view or <tt>const Char*</tt> to
	/// be used as keys when the type of the key is \p std::basic_string.
	template <typename Char = char8_t> struct string_hash {
		/// Indicates that this hasher accepts multiple key types.
		using is_transparent = void;
		using hash_type = std::hash<std::basic_string_view<Char>>; ///< Underlying \p std::hash type.

		/// Computes hash for a C-style string.
		std::size_t operator()(const Char *str) const {
			// FIXME this will compute the length of the string which is O(n). however, since we don't use this
			//       function often this is probably fine
			return hash_type{}(str);
		}
		/// Computes hash for a \p std::basic_string_view.
		std::size_t operator()(std::basic_string_view<Char> str) const {
			return hash_type{}(str);
		}
		/// Computes hash for a \p std::basic_string.
		std::size_t operator()(const std::basic_string<Char> &str) const {
			return hash_type{}(str);
		}
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
			res = codepad::combine_hashes(res, viewhasher(pos.function));
			res = codepad::combine_hashes(res, viewhasher(pos.file));
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

	/// Custom linear interpolation. For floating-point types, uses \p std::lerp(); for integral types, uses
	/// \p std::lerp() then rounds the value back to integer or a enum; otherwise returns \p from directly. Note that
	/// by default enums cannot be lerped although it's implemented here; they need to manually specialize
	/// \ref can_lerp to enable this behavior.
	template <typename T> inline constexpr T lerp(T from, T to, double perc) {
		using _value_type = std::decay_t<T>;
		if constexpr (std::is_floating_point_v<_value_type>) {
			return std::lerp(from, to, static_cast<_value_type>(perc));
		} else if constexpr (std::is_integral_v<_value_type> || std::is_enum_v<_value_type>) {
			return static_cast<T>(std::round(std::lerp(static_cast<double>(from), static_cast<double>(to), perc)));
		} else {
			return from * (1.0 - perc) + to * perc;
		}
	}
	/// Used to test if it's valid to call \ref lerp() on a specific type. Classes that specialize \ref lerp() should
	/// also specialize this class.
	template <typename T> struct can_lerp {
	protected:
		// SFINAE stubs used to test if the necessary operators are implemented
		/// Matches if all necessary operators are implemented.
		template <typename U = T> static std::true_type test(decltype(
			std::declval<U>() + (std::declval<U>() - std::declval<U>()) * 0.5, 0
			));
		/// Matches otherwise.
		template <typename U = T> static std::false_type test(...);
	public:
		/// The result obtained using SFINAE.
		constexpr static bool value = decltype(test(0))::value;
	};

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

	/// Calls the callback function for all substrings in the given string separated by the given codepoint,
	/// terminating once the callback returns \p false, if the callback returns a value.
	template <typename Callback> inline void split_string(codepoint split, std::u8string_view str, Callback &&cb) {
		using _callback_return_t = std::invoke_result_t<Callback&&, std::u8string_view>;

		auto it = str.begin(), last = str.begin();
		while (it != str.end()) {
			auto char_beg = it;
			codepoint cp;
			if (!encodings::utf8::next_codepoint(it, str.end(), cp)) {
				cp = encodings::replacement_character;
			}
			if (cp == split) {
				std::u8string_view substring = str.substr(last - str.begin(), char_beg - last);
				if constexpr (std::is_same_v<_callback_return_t, void>) {
					cb(substring);
				} else {
					if (!cb(substring)) {
						return;
					}
				}
				last = it;
			}
		}
		cb(str.substr(last - str.begin(), str.end() - last));
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
		reference_counted_handle(reference_counted_handle &&src) noexcept :
			_handle(std::exchange(src._handle, nullptr)) {
		}
		/// Copy assignment.
		reference_counted_handle &operator=(const reference_counted_handle &src) {
			set_share(src._handle);
			return *this;
		}
		/// Move assignment.
		reference_counted_handle &operator=(reference_counted_handle &&src) noexcept {
			set_give(std::exchange(src._handle, nullptr));
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
