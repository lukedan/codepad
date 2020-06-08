// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Implementation of colors.

#include <type_traits>

#include "../apigen_definitions.h"
#include "json/misc.h"

namespace codepad {
	/// RGBA color representation.
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

	namespace json {
		/// Parser for \ref colord.
		template <> struct default_parser<colord> {
			/// Parses \ref colord. The object can take the following formats: <tt>["hsl", h, s, l(, a)]</tt> for
			/// HSL format colors, and <tt>[r, g, b(, a)]</tt> for RGB format colors.
			template <typename Value> std::optional<colord> operator()(const Value&) const;
		};
	}
}
