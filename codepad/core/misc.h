// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous fundamental functionalities.

#include <cmath>
#include <exception>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <type_traits>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cstdlib>

#if __has_include(<filesystem>)
#	include <filesystem>
#elif __has_include(<experimental/filesystem>)
#	define CP_EXPERIMENTAL_FILESYSTEM
#	include <experimental/filesystem>
#else
#	error std::filesystem not found.
#endif

#include "encodings.h"

#ifdef CP_EXPERIMENTAL_FILESYSTEM
namespace std {
	namespace filesystem = experimental::filesystem;
}
#endif

namespace std {
	/// Specialize \p std::hash<std::filesystem::path> that std doesn't specialize for some reason.
	template <> struct hash<filesystem::path> {
		size_t operator()(const filesystem::path &p) const {
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
	> operator&=(Enum & lhs, Enum rhs) {
		return lhs = lhs & rhs;
	}
	/// Bitwise or for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator|=(Enum & lhs, Enum rhs) {
		return lhs = lhs | rhs;
	}
	/// Bitwise xor for enum classes.
	template <typename Enum> inline constexpr std::enable_if_t<
		std::is_enum_v<Enum> && enable_enum_bitwise_operators_v<Enum>, Enum&
	> operator^=(Enum & lhs, Enum rhs) {
		return lhs = lhs ^ rhs;
	}

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
		code_position(const char *fil, const char *func, int l) : file(fil), function(func), line(l) {
		}

		/// Prints a \ref code_position to a stream.
		friend std::ostream &operator<<(std::ostream &out, const code_position &cp) {
			return out << cp.function << " @" << cp.file << ":" << cp.line;
		}

		const char
			*file, ///< The file.
			*function; ///< The function.
		int line; ///< The line of the file.
	};
	/// A \ref codepad::code_position representing where it appears.
#define CP_HERE ::codepad::code_position(__FILE__, __func__, __LINE__)

	/// Returns the time point when the application was started.
	std::chrono::high_resolution_clock::time_point get_app_epoch();
	/// Returns the duration since the application started.
	std::chrono::duration<double> get_uptime();


	/// Represents vectors, points, sizes, etc.
	///
	/// \tparam T The type of \ref x and \ref y components.
	template <typename T> struct vec2 {
		/// Default constructor.
		constexpr vec2() = default;
		/// Constructor that initializes \ref x and \ref y.
		constexpr vec2(T xx, T yy) : x(xx), y(yy) {
		}

		T
			x = 0, ///< The x coordinate.
			y = 0; ///< The y coordinate.

		/// Index components by numbers, 0 for \ref x and 1 for \ref y.
		T &operator[](size_t);
		/// Const version of operator[](size_t).
		const T &operator[](size_t) const;

		/// Converts all components to another type and returns the result.
		///
		/// \tparam U The desired type of components.
		template <typename U> constexpr vec2<U> convert() const {
			return vec2<U>(static_cast<U>(x), static_cast<U>(y));
		}

		/// Returns the squared length of the vector.
		constexpr T length_sqr() const {
			return x * x + y * y;
		}
		/// Returns the length of the vector.
		constexpr T length() const {
			return std::sqrt(length_sqr());
		}

		/// Addition.
		vec2 &operator+=(vec2 rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		/// Addition.
		constexpr friend vec2 operator+(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		/// Subtraction.
		constexpr friend vec2 operator-(vec2 v) {
			return vec2(-v.x, -v.y);
		}
		/// Subtraction.
		vec2 &operator-=(vec2 rhs) {
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}
		/// Subtraction.
		constexpr friend vec2 operator-(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x - rhs.x, lhs.y - rhs.y);
		}

		/// Scalar multiplication.
		vec2 &operator*=(T rhs) {
			x *= rhs;
			y *= rhs;
			return *this;
		}
		/// Scalar multiplication.
		constexpr friend vec2 operator*(vec2 lhs, T rhs) {
			return vec2(lhs.x * rhs, lhs.y * rhs);
		}
		/// Scalar multiplication.
		constexpr friend vec2 operator*(T lhs, vec2 rhs) {
			return vec2(lhs * rhs.x, lhs * rhs.y);
		}

		/// Scalar division.
		vec2 &operator/=(T rhs) {
			x /= rhs;
			y /= rhs;
			return *this;
		}
		/// Scalar division.
		constexpr friend vec2 operator/(vec2 lhs, T rhs) {
			return vec2(lhs.x / rhs, lhs.y / rhs);
		}
	};
	/// Shorthand for vectors whose components are of type \p double.
	using vec2d = vec2<double>;
	/// Shorthand for vectors whose components are of type \p float.
	using vec2f = vec2<float>;
	/// Shorthand for vectors whose components are of type \p int.
	using vec2i = vec2<int>;
	/// Shorthand for vectors whose components are of type <tt>unsigned int</tt>.
	using vec2u = vec2<unsigned int>;

	/// Represnts a rectangular area.
	///
	/// \tparam T Type of all coordinates.
	template <typename T> struct rect {
		/// Default constructor.
		constexpr rect() = default;
		/// Constructs a \ref rect with the given coordinates.
		constexpr rect(T minx, T maxx, T miny, T maxy) : xmin(minx), xmax(maxx), ymin(miny), ymax(maxy) {
		}

		T
			xmin = 0, ///< Minimum x coordinate.
			xmax = 0, ///< Maximum x coordinate.
			ymin = 0, ///< Minimum y coordinate.
			ymax = 0; ///< Maximum y coordinate.

		/// Returns the width of the rectangle, i.e., \ref xmax - \ref xmin.
		constexpr T width() const {
			return xmax - xmin;
		}
		/// Returns the height of the rectangle, i.e., \ref ymax - \ref ymin.
		constexpr T height() const {
			return ymax - ymin;
		}
		/// Returns the area of the rectangle, i.e., width() * height().
		constexpr vec2<T> size() const {
			return vec2<T>(width(), height());
		}
		/// Returns the vector (\ref xmin, \ref ymin).
		constexpr vec2<T> xmin_ymin() const {
			return vec2<T>(xmin, ymin);
		}
		/// Returns the vector (\ref xmax, \ref ymin).
		constexpr vec2<T> xmax_ymin() const {
			return vec2<T>(xmax, ymin);
		}
		/// Returns the vector (\ref xmin, \ref ymax).
		constexpr vec2<T> xmin_ymax() const {
			return vec2<T>(xmin, ymax);
		}
		/// Returns the vector (\ref xmax, \ref ymax).
		constexpr vec2<T> xmax_ymax() const {
			return vec2<T>(xmax, ymax);
		}

		/// Returns the x coordinate of the rectangle's center, i.e., \f$\frac{1}{2}\f$ (\ref xmin + \ref xmax).
		constexpr double centerx() const {
			return (xmin + xmax) * 0.5f;
		}
		/// Returns the y coordinate of the rectangle's center, i.e., \f$\frac{1}{2}\f$ (\ref ymin + \ref ymax).
		constexpr double centery() const {
			return (ymin + ymax) * 0.5f;
		}
		/// Returns the coordinate of the rectangle's center, i.e., (centerx(), centery()).
		constexpr vec2<T> center() const {
			return vec2<T>(centerx(), centery());
		}

		/// Returns true if both \ref xmax > \ref xmin and \ref ymax > \ref ymin.
		constexpr bool positive_area() const {
			return xmax > xmin && ymax > ymin;
		}
		/// Returns true if both \ref xmax >= \ref xmin and \ref ymax >= \ref ymin.
		constexpr bool nonnegative_area() const {
			return xmax >= xmin && ymax >= ymin;
		}

		/// Returns true if a given point lies in the rectangle or on its boundary.
		///
		/// \param v The given point.
		constexpr bool contains(vec2<T> v) const {
			return v.x >= xmin && v.x <= xmax && v.y >= ymin && v.y <= ymax;
		}
		/// Returns true if a given point completely lies in the rectangle, i.e., does not touch the boundary.
		///
		/// \param v The given point.
		constexpr bool fully_contains(vec2<T> v) const {
			return v.x > xmin && v.x < xmax && v.y > ymin && v.y < ymax;
		}

		/// Adjusts \ref xmin and \ref ymin so that nonnegative_area() returns \p true. This version uses the smaller
		/// value.
		void make_valid_min() {
			if (xmin > xmax) {
				xmin = xmax;
			}
			if (ymin > ymax) {
				ymin = ymax;
			}
		}
		/// Adjusts \ref xmax and \ref ymax so that nonnegative_area() returns \p true. This version uses the larger
		/// value.
		void make_valid_max() {
			if (xmin > xmax) {
				xmax = xmin;
			}
			if (ymin > ymax) {
				ymax = ymin;
			}
		}
		/// Adjusts \ref xmax and \ref ymax so that nonnegative_area() returns \p true. This version uses the average
		/// value.
		void make_valid_average() {
			if (xmin > xmax) {
				xmin = xmax = 0.5 * (xmin + xmax);
			}
			if (ymin > ymax) {
				ymin = ymax = 0.5 * (ymin + ymax);
			}
		}

		/// Converts all coordinates to another type.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic_v<U>, rect<U>> convert() const {
			return rect<U>(static_cast<U>(xmin), static_cast<U>(xmax), static_cast<U>(ymin), static_cast<U>(ymax));
		}
		/// Converts all coordinates to another type, rounding them towards out of the rectangle.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic_v<U>, rect<U>> fit_grid_enlarge() const {
			return rect<U>(
				static_cast<U>(std::floor(xmin)), static_cast<U>(std::ceil(xmax)),
				static_cast<U>(std::floor(ymin)), static_cast<U>(std::ceil(ymax))
				);
		}
		/// Converts all coordinates to another type, rounding them towards inside of the rectangle.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic_v<U>, rect<U>> fit_grid_shrink() const {
			return rect<U>(
				static_cast<U>(std::ceil(xmin)), static_cast<U>(std::floor(xmax)),
				static_cast<U>(std::ceil(ymin)), static_cast<U>(std::floor(ymax))
				);
		}

		/// Returns the rectangle translated by a vector.
		///
		/// \param diff The desired offset.
		constexpr rect translated(vec2<T> diff) const {
			return rect(xmin + diff.x, xmax + diff.x, ymin + diff.y, ymax + diff.y);
		}
		/// Returns the rectangle scaled with respect to a given point by a given scalar.
		///
		/// \param center The center of scaling.
		/// \param scale The desired scale.
		constexpr rect scaled(vec2<T> center, double scale) const {
			return rect(
				center.x + static_cast<T>(scale * (xmin - center.x)),
				center.x + static_cast<T>(scale * (xmax - center.x)),
				center.y + static_cast<T>(scale * (ymin - center.y)),
				center.y + static_cast<T>(scale * (ymax - center.y))
			);
		}
		/// Returns the rectangle scaled with respect to (0, 0).
		///
		/// \param scale The desired scale.
		constexpr rect coordinates_scaled(double scale) const {
			return rect(
				static_cast<T>(scale * xmin),
				static_cast<T>(scale * xmax),
				static_cast<T>(scale * ymin),
				static_cast<T>(scale * ymax)
			);
		}

		/// Returns the largest rectangle that's fully contained by both given rectangles.
		/// If the two rectangles don't intersect, the returned rectangle will have negative area.
		inline static constexpr rect common_part(rect lhs, rect rhs) {
			return rect(
				std::max(lhs.xmin, rhs.xmin), std::min(lhs.xmax, rhs.xmax),
				std::max(lhs.ymin, rhs.ymin), std::min(lhs.ymax, rhs.ymax)
			);
		}
		/// Returns the smallest rectangle that fully contains both given rectangles.
		inline static constexpr rect bounding_box(rect lhs, rect rhs) {
			return rect(
				std::min(lhs.xmin, rhs.xmin), std::max(lhs.xmax, rhs.xmax),
				std::min(lhs.ymin, rhs.ymin), std::max(lhs.ymax, rhs.ymax)
			);
		}

		/// Constructs a rectangle given its top-left corner, width, and height.
		inline static constexpr rect from_xywh(T x, T y, T w, T h) {
			return rect(x, x + w, y, y + h);
		}
		/// Constructs a rectangle given the position of its top-left corner and its bottom-right corner.
		inline static constexpr rect from_corners(vec2<T> topleft, vec2<T> bottomright) {
			return rect(topleft.x, bottomright.x, topleft.y, bottomright.y);
		}
	};
	/// Rectangles with coordinates of type \p double.
	using rectd = rect<double>;
	/// Rectangles with coordinates of type \p float.
	using rectf = rect<float>;
	/// Rectangles with coordinates of type \p int.
	using recti = rect<int>;
	/// Rectangles with coordinates of type <tt>unsigned int</tt>.
	using rectu = rect<unsigned int>;

	/// Matrix struct.
	///
	/// \tparam T Type of matrix elements.
	/// \tparam W Width of the matrix.
	/// \tparam H Height of the matrix.
	template <typename T, size_t W, size_t H> struct matrix {
		/// A row of the matrix.
		using row = T[W];

		row elem[H]{}; ///< Elements of the matrix.

		/// Sets all elements to zero.
		void set_zero() {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] = 0;
				}
			}
		}
		/// Sets all elements on the diagonal line to 1, and all other elements to 0.
		void set_identity() {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] = static_cast<T>(x == y ? 1 : 0);
				}
			}
		}

		/// Converts all values to another type.
		///
		/// \tparam U The desired type.
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic_v<U>, matrix<U, W, H>> convert() const {
			matrix<U, W, H> res;
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					res[y][x] = static_cast<U>(elem[y][x]);
				}
			}
			return res;
		}

	private:
		inline static void _dot(T ax, T ay, T az, T bx, T by, T bz, T & rx, T & ry, T & rz) {
			rx = ay * bz - az * by;
			ry = az * bx - ax * bz;
			rz = ax * by - ay * bx;
		}
	public:
		/// Calculates the inverse matrix of a 3x3 matrix.
		///
		/// \todo Necessary for nxn matrices?
		constexpr std::enable_if_t<W == 3 && H == 3, matrix> inverse() const {
			matrix res;
			_dot(
				elem[0][1], elem[1][1], elem[2][1],
				elem[0][2], elem[1][2], elem[2][2],
				res[0][0], res[0][1], res[0][2]
			);
			T det = elem[0][0] * res[0][0] + elem[1][0] * res[0][1] + elem[2][0] * res[0][2];
			_dot(
				elem[0][2], elem[1][2], elem[2][2],
				elem[0][0], elem[1][0], elem[2][0],
				res[1][0], res[1][1], res[1][2]
			);
			_dot(
				elem[0][0], elem[1][0], elem[2][0],
				elem[0][1], elem[1][1], elem[2][1],
				res[2][0], res[2][1], res[2][2]
			);
			return res * (1.0 / det); // not gonna work for integer matrices, but who would use them?
		}

		/// Returns the requested row of the matrix.
		///
		/// \param y 0-based index of the row.
		row &operator[](size_t y) {
			return elem[y];
		}
		/// Const version of \ref operator[].
		const row &operator[](size_t y) const {
			return elem[y];
		}

		/// Addition.
		matrix &operator+=(const matrix & rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] += rhs[y][x];
				}
			}
			return *this;
		}
		/// Addition.
		friend matrix operator+(matrix lhs, const matrix & rhs) {
			return lhs += rhs;
		}

		/// Subtraction.
		matrix &operator-=(const matrix & rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] -= rhs[y][x];
				}
			}
			return *this;
		}
		/// Subtraction.
		friend matrix operator-(matrix lhs, const matrix & rhs) {
			return lhs -= rhs;
		}

		/// In-place matrix multiplication, only for square matrices.
		std::enable_if_t<W == H, matrix&> operator*=(const matrix & rhs) {
			matrix res;
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					for (size_t k = 0; k < W; ++k) {
						res[y][x] = elem[y][k] * rhs[k][x];
					}
				}
			}
			return *this = res;
		}
		/// Scalar multiplication.
		matrix &operator*=(T rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] *= rhs;
				}
			}
			return *this;
		}
		/// Scalar multiplication.
		friend matrix operator*(matrix lhs, T rhs) {
			return lhs *= rhs;
		}
		/// Scalar multiplication.
		friend matrix operator*(T lhs, matrix rhs) {
			return rhs *= lhs;
		}

		/// Scalar division.
		matrix &operator/=(T rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] /= rhs;
				}
			}
			return *this;
		}
		/// Scalar division.
		friend matrix operator/(matrix lhs, T rhs) {
			return lhs /= rhs;
		}

		/// Given a \ref vec2 (x, y), returns the vector obtained by multiplying this matrix with (x, y, 1).
		std::enable_if_t<W == 3 && H == 3, vec2<T>> transform(vec2<T> v) const {
			return vec2<T>(
				elem[0][0] * v.x + elem[0][1] * v.y + elem[0][2],
				elem[1][0] * v.x + elem[1][1] * v.y + elem[1][2]
				);
		}

		/// Returns a matrix that translates vectors by a given offset.
		///
		/// \param off The desired offset.
		inline static std::enable_if_t<W == 3 && H == 3, matrix> translate(vec2<T> off) {
			matrix res;
			res[0][0] = res[1][1] = res[2][2] = 1.0;
			res[0][2] = off.x;
			res[1][2] = off.y;
			return res;
		}
		/// Returns a matrix that rotates vectors around \p center by \p rotv,
		/// where \p rotv is a vector of the form \f$(\cos{\alpha}, \sin{\alpha})\f$.
		/// Scale can also be combined by multiplying \p rotv with the scale factor.
		inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_by_vector(vec2<T> center, vec2<T> rotv) {
			// [1  0  cx] [rx  -ry  0] [1  0  -cx] [vx]
			// [0  1  cy] [ry  rx   0] [0  1  -cy] [vy]
			// [0  0  1 ] [0    0   1] [0  0   1 ] [1 ]
			//            [rx  -ry  ry*cy-rx*cx ]
			//            [ry  rx   -ry*cx-rx*cy]
			//            [0    0         1     ]
			// [rx  -ry  ry*cy-rx*cx+cx ]
			// [ry  rx   -ry*cx-rx*cy+cy]
			// [0    0          1       ]
			matrix res;
			res[0][0] = rotv.x;
			res[0][1] = -rotv.y;
			res[0][2] = center.x + rotv.y * center.y - rotv.x * center.x;
			res[1][0] = rotv.y;
			res[1][1] = rotv.x;
			res[1][2] = center.y - rotv.y * center.x - rotv.x * center.y;
			res[2][2] = 1.0;
			return res;
		}
		/// Returns a matrix that rotates vectors around \p center by a certain number of radians.
		inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_clockwise(vec2<T> center, double radians) {
			return rotate_by_vector(center, vec2<T>(std::cos(radians), std::sin(radians)));
		}
		/// Returns a matrix that scales vectors around \p center by \p scale.
		/// The x coordinate is scaled by \p scale.x and y by \p scale.y.
		inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, vec2<T> scale) {
			// [1  0  cx] [sx  0   0] [1  0  -cx] [vx]
			// [0  1  cy] [0   sy  0] [0  1  -cy] [vy]
			// [0  0  1 ] [0   0   1] [0  0   1 ] [1 ]
			//            [sx  0   -cx*sx]
			//            [0   sy  -cy*sy]
			//            [0   0      1  ]
			// [sx  0   cx-cx*sx]
			// [0   sy  cy-cy*sy]
			// [0   0       1   ]
			matrix res;
			res[0][0] = scale.x;
			res[0][2] = center.x * (1.0 - scale.x);
			res[1][1] = scale.y;
			res[1][2] = center.y * (1.0 - scale.y);
			res[2][2] = 1.0;
			return res;
		}
		/// Returns a matrix that scales vectors around \p center by \p uniscale.
		inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, T uniscale) {
			return scale(center, vec2<T>(uniscale, uniscale));
		}
	};
	/// Matrix multiplication.
	template <typename T, size_t M, size_t N, size_t P> inline matrix<T, P, M> operator*(
		const matrix<T, N, M> & lhs, const matrix<T, P, N> & rhs
		) {
		matrix<T, P, M> result;
		for (size_t y = 0; y < M; ++y) {
			for (size_t x = 0; x < P; ++x) {
				for (size_t k = 0; k < N; ++k) {
					result[y][x] += lhs[y][k] * rhs[k][x];
				}
			}
		}
		return result;
	}
	/// Multiplication of 2x2 matrices with \ref vec2 "vec2s".
	template <typename T> inline vec2<T> operator*(const matrix<T, 2, 2> & lhs, vec2<T> rhs) {
		return vec2<T>(lhs[0][0] * rhs.x + lhs[0][1] * rhs.y, lhs[1][0] * rhs.x + lhs[1][1] * rhs.y);
	}
	/// 2x2 matrices whose elements are of type \p float.
	using matf2x2 = matrix<float, 2, 2>;
	/// 2x2 matrices whose elements are of type \p double.
	using matd2x2 = matrix<double, 2, 2>;
	/// 3x3 matrices whose elements are of type \p float.
	using matf3x3 = matrix<float, 3, 3>;
	/// 3x3 matrices whose elements are of type \p double.
	using matd3x3 = matrix<double, 3, 3>;

	/// Color representation.
	///
	/// \tparam T Type of the four components. Must be a floating point type or <tt>unsigned char</tt>.
	template <typename T> struct color {
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
	inline constexpr size_t high_bit_index(std::uint64_t v) {
		if (v == 0) {
			return 64;
		}
		size_t res = 0;
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
	inline constexpr size_t low_bit_index(std::uint64_t v) {
		if (v == 0) {
			return 64;
		}
		size_t res = 0;
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


	/// Semaphore class implemented using \p std::mutex and \p std::condition_variable.
	/// Copied from https://stackoverflow.com/questions/4792449/c0x-has-no-semaphores-how-to-synchronize-threads
	struct semaphore {
	public:
		/// Default constructor.
		semaphore() = default;
		/// Constructs the semaphore as unlocked with the given count.
		explicit semaphore(unsigned v) : _v(v) {
		}

		/// The signal function.
		void signal() {
			_unique_lock_t lock(_mtx);
			++_v;
			_cv.notify_one();
		}
		/// The wait function.
		void wait() {
			_unique_lock_t lock(_mtx);
			while (_v == 0) { // handle spurious wake-ups
				_cv.wait(lock);
			}
			--_v;
		}
		/// Non-blocking version of wait().
		///
		/// \return \p false if wait() would have blocked.
		bool try_wait() {
			_unique_lock_t lock(_mtx);
			if (_v > 0) {
				--_v;
				return true;
			}
			return false;
		}
	protected:
		/// Type of the lock.
		using _unique_lock_t = std::unique_lock<std::mutex>;

		std::mutex _mtx; ///< The mutex.
		std::condition_variable _cv; ///< The condition variable.
		unsigned _v = 0; ///< The counter.
	};


	/// End of recursion.
	template <typename St, typename T> inline void print_to(St & s, T && arg) {
		s << std::forward<T>(arg);
	}
	/// Prints objects to a stream with \p operator<<.
	///
	/// \param s The stream.
	/// \param f The first object to print.
	/// \param args Other following objects.
	template <typename St, typename First, typename ...Others>
	inline std::enable_if_t<(sizeof...(Others) > 0), void> print_to(St & s, First && f, Others && ...args) {
		print_to(s, std::forward<First>(f));
		print_to(s, std::forward<Others>(args)...);
	}

	/// Enumeration used to specify the level of logging and the type of individual log entries.
	enum class log_level {
		verbose, ///< Very detailed logging that can be used to debug the program itself.
		info, ///< Detailed logging that contains helpful information about the state of the program.
		warning, ///< Notification of exceptions that are not enough to crash the program.
		error, ///< Notification of internal program errors.

		other ///< Log that does not belong to any of the categories above.
	};

	/// Struct used to format and produce log.
	struct logger {
	public:
		/// Default constructor, log is written to \p default.log.
		logger() : logger("default.log") {
		}
		/// Constructs the \ref logger and with the specified log file. Log is appended to the file.
		///
		/// \param s The file name of the log file.
		/// \todo Change param type to filesystem::path when libstdc++ supports it.
		explicit logger(const
#ifdef __GNUC__
			std::string
#else
			std::filesystem::path
#endif
			&s) : _fout(s, std::ios::app) {
			log_raw("\n\n####################\n\n");
			log_info(CP_HERE, "new logger \"", s, "\" created");
		}
		/// Destructor. Usually marks the end of a session.
		~logger() {
			log_info(CP_HERE, "session shutdown");
		}

		/// Logs the given information with the given log level.
		///
		/// \param level The log level to be used.
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log(log_level level, const code_position &cp, Args &&...args) {
			switch (level) {
			case log_level::verbose:
				log_verbose(cp, std::forward<Args>(args)...);
				break;
			case log_level::info:
				log_info(cp, std::forward<Args>(args)...);
				break;
			case log_level::warning:
				log_warning(cp, std::forward<Args>(args)...);
				break;
			case log_level::error:
				log_error(cp, std::forward<Args>(args)...);
				break;
			default:
				log_raw(std::forward<Args>(args)...);
				break;
			}
		}

		/// Logs the given information with the `verbose' log level.
		///
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log_verbose(const code_position &cp, Args &&...args) {
			_log_fmt("       ", cp, std::forward<Args>(args)...);
		}
		/// Logs the given information with the `info' log level.
		///
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log_info(const code_position &cp, Args &&...args) {
			_log_fmt(" INFO  ", cp, std::forward<Args>(args)...);
		}
		/// Logs the given information with the `warning' log level.
		///
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log_warning(const code_position &cp, Args &&...args) {
			_log_fmt("\033[33mWARNING\033[0m", cp, std::forward<Args>(args)...);
		}
		/// Logs the given information with the `error' log level, and flushes the streams.
		///
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log_error(const code_position &cp, Args &&...args) {
			_log_fmt("\033[31m ERROR \033[0m", cp, std::forward<Args>(args)...);
			_fout.flush();
		}
		/// Logs the given information and stacktrace with the `error' log level, and flushes the streams.
		///
		/// \param cp The position indicating where the log was produced.
		/// \param args The log information.
		template <typename ...Args> void log_error_with_stacktrace(const code_position &cp, Args &&...args) {
			log_error(cp, std::forward<Args>(args)...);
			log_stacktrace();
			_fout.flush();
		}

		/// Logs a custom message, without log level or source position.
		template <typename ...Args> void log_custom(Args &&...args) {
			char ss[9];
			std::snprintf(ss, sizeof(ss) / sizeof(char), "%8.02f", get_uptime().count());
			log_raw(ss, "|", std::forward<Args>(args)..., "\n");
		}
		/// Logs a custom message, without any formatting (uptime or newline).
		template <typename ...Args> void log_raw(Args &&...args) {
			print_to(_fout, std::forward<Args>(args)...);
			print_to(std::cerr, std::forward<Args>(args)...);
		}

		/// Logs the current stacktrace.
		void log_stacktrace();

		/// Flushes the output file.
		void flush() {
			_fout.flush();
		}

		/// Gets the global \ref logger object.
		static logger &get();
	protected:
		std::ofstream _fout; ///< File output.

		/// Logs an entry with the given 7-char header.
		template <typename ...Args> void _log_fmt(const char *header, const code_position &cp, Args &&...args) {
			log_custom(header, "|", cp, "|", std::forward<Args>(args)...);
		}
	};


	/// Struct that monitors the beginning, ending, and duration of its lifespan.
	struct performance_monitor {
	public:
		using clock_t = std::chrono::high_resolution_clock; ///< The clock used for measuring performance.

		/// Determines when and how should the measured running time be logged.
		enum class log_condition : unsigned char {
			always, ///< Always log time.
			late_only, ///< Only when the execution time exceeds the expected time.
			never ///< Never log time.
		};

		/// Constructs the \ref performance_monitor from the given label.
		explicit performance_monitor(str_view_t lbl, log_condition cond = log_condition::late_only) :
			_label(lbl), _beg_time(clock_t::now()), _cond(cond) {
		}
		/// Constructs the \ref performance_monitor from the given label and expected running time.
		template <typename Dur> performance_monitor(
			str_view_t lbl, Dur exp, log_condition cond = log_condition::late_only
		) : performance_monitor(lbl, cond) {
			_expected = std::chrono::duration_cast<clock_t::duration>(exp);
		}
		/// Destructor. Logs if the running time exceeds the expected time.
		~performance_monitor() {
			auto dur = clock_t::now() - _beg_time;
			// TODO print duration directly after C++20
			auto sdur = std::chrono::duration_cast<std::chrono::duration<double>>(dur);
			if (dur > _expected && _cond != log_condition::never) {
				logger::get().log_warning(
					CP_HERE, "operation took longer(", sdur.count(), "s) than expected(",
					_expected.count(), "): ", _label
				);
			} else if (_cond == log_condition::always) {
				logger::get().log_verbose(
					CP_HERE, "operation took ", sdur.count(), "s: ", _label
				);
			}
		}

		/// Logs the time since the creation of this object so far.
		void log_time() {
			auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(clock_t::now() - _beg_time);
			logger::get().log_verbose(CP_HERE, _label, ": operation has been running for ", dur.count(), "s");
		}
	protected:
		str_view_t _label; ///< The label for this performance monitor.
		clock_t::time_point _beg_time; ///< The time when this object is constructed, obtained from get_uptime().
		clock_t::duration _expected = clock_t::duration::max(); ///< The expected running time.
		log_condition _cond = log_condition::late_only; ///< The condition under which to log the running time.
	};


	/// Specifies the type of an error.
	enum class error_level {
		system_error, ///< Unexpected errors with system API, OpenGL, FreeType, etc. Nothing we can do.
		usage_error, ///< Wrong usage of codepad components, which can also be treated as exceptions.
		logical_error ///< Internal logical errors in codepad. Basically bugs of the program.
	};

#ifdef NDEBUG
#	define CP_ERROR_LEVEL 3
#else
#	define CP_ERROR_LEVEL 3
#endif

#if CP_ERROR_LEVEL > 0
#	define CP_CHECK_SYSTEM_ERRORS
#endif
#if CP_ERROR_LEVEL > 1
#	define CP_CHECK_USAGE_ERRORS
#endif
#if CP_ERROR_LEVEL > 2
#	define CP_CHECK_LOGICAL_ERRORS
#endif

	/// Assertion, with a given error message. The main purpose of declaring this as a function is that
	/// expressions used as the predicates will always be evaluated, unlike \p assert.
	///
	/// \tparam Lvl An \ref error_level enumeration that indicates the type of the assertion.
	template <error_level Lvl> inline void assert_true(bool v, const char *msg) {
		if (!v) {
			throw std::runtime_error(msg);
		}
	}

#ifdef CP_CHECK_SYSTEM_ERRORS
	/// \ref assert_true specialization of error_level::system_error.
	template <> inline void assert_true<error_level::system_error>(bool v, const char *msg) {
		if (!v) {
			logger::get().log_error_with_stacktrace(CP_HERE, "System error encountered: ", msg);
			std::abort();
	}
}
#else
	template <> inline void assert_true<error_level::system_error>(bool, const char*) {
	}
#endif
#ifdef CP_CHECK_USAGE_ERRORS
	/// \ref assert_true specialization of error_level::usage_error.
	template <> inline void assert_true<error_level::usage_error>(bool v, const char *msg) {
		if (!v) {
			logger::get().log_error_with_stacktrace(CP_HERE, "Usage error encountered: ", msg);
			std::abort();
	}
	}
#else
	template <> inline void assert_true<error_level::usage_error>(bool, const char*) {
	}
#endif
#ifdef CP_CHECK_LOGICAL_ERRORS
	/// \ref assert_true specialization of error_level::logical_error.
	template <> inline void assert_true<error_level::logical_error>(bool v, const char *msg) {
		if (!v) {
			logger::get().log_error_with_stacktrace(CP_HERE, "Logical error encountered: ", msg);
			std::abort();
	}
	}
#else
	template <> inline void assert_true<error_level::logical_error>(bool, const char*) {
	}
#endif

	/// Shorthand for \ref assert_true<error_level::system_error>.
	inline void assert_true_sys(bool v, const char *msg) {
		assert_true<error_level::system_error>(v, msg);
	}
	/// Shorthand for \ref assert_true<error_level::system_error> with no custom message.
	inline void assert_true_sys(bool v) {
		assert_true<error_level::system_error>(v, "default system error message");
	}
	/// Shorthand for \ref assert_true<error_level::usage_error>.
	inline void assert_true_usage(bool v, const char *msg) {
		assert_true<error_level::usage_error>(v, msg);
	}
	/// Shorthand for \ref assert_true<error_level::usage_error> with no custom message.
	inline void assert_true_usage(bool v) {
		assert_true<error_level::usage_error>(v, "default usage error message");
	}
	/// Shorthand for \ref assert_true<error_level::logical_error>.
	inline void assert_true_logical(bool v, const char *msg) {
		assert_true<error_level::logical_error>(v, msg);
	}
	/// Shorthand for \ref assert_true<error_level::logical_error> with no custom message.
	inline void assert_true_logical(bool v) {
		assert_true<error_level::logical_error>(v, "default logical error message");
	}

	template <typename T> inline T &vec2<T>::operator[](size_t sub) {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}
	template <typename T> inline const T &vec2<T>::operator[](size_t sub) const {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}

	/// Initializes the program by calling \ref os::initialize first and then performing several other
	/// initialization steps.
	void initialize(int, char**);
		}

// demangle
#ifdef __GNUC__
#	include <cxxabi.h>
#endif
namespace codepad {
	/// Demangles a given type name for more human-readable output.
	inline std::string demangle(std::string s) {
#ifdef _MSC_VER
		return s;
#elif defined(__GNUC__)
		int st;
		char *result = abi::__cxa_demangle(s.c_str(), nullptr, nullptr, &st);
		assert_true_sys(st == 0, "demangling failed");
		std::string res(result);
		std::free(result);
		return res;
#endif
	}
}

#ifndef CP_LOG_STACKTRACE
namespace codepad {
	inline void logger::log_stacktrace() {
		log_warning(CP_HERE, "stacktrace logging has been disabled");
	}
}
#elif !defined(_MSC_VER) // windows version in os/windows/windows.cpp
#	if defined(CP_PLATFORM_UNIX) && defined(__GNUC__)
#		include <execinfo.h>
#	endif
namespace codepad {
	inline void logger::log_stacktrace() {
#	if defined(CP_PLATFORM_UNIX) && defined(__GNUC__)
		constexpr static int max_frames = 1000;

		void *frames[max_frames];
		int numframes = backtrace(frames, max_frames);
		char **symbols = backtrace_symbols(frames, numframes);
		assert_true_sys(symbols != nullptr, "backtrace_symbols() failed");
		log_custom("STACKTRACE");
		for (int i = 0; i < numframes; ++i) {
			log_custom("    ", symbols[i]);
		}
		log_custom("STACKTRACE|END");
		free(symbols);
#	else
		log_warning(CP_HERE, "stacktrace logging is not supported with this configuration");
#	endif
}
	}
#endif
