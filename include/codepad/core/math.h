// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Basic mathmatics.

#include <cmath>
#include <type_traits>

#include "assert.h"

namespace codepad {
	/// Tests if the difference between the two floating point numbers are below the given threshold.
	template <typename Real> inline std::enable_if_t<std::is_floating_point_v<Real>, bool> approximately_equals(
		Real lhs, Real rhs, Real eps = 1e-6f
	) {
		return std::abs(lhs - rhs) < eps;
	}

	/// Represents vectors, points, sizes, etc.
	///
	/// \tparam T The type of \ref x and \ref y components.
	template <typename T> struct APIGEN_EXPORT_RECURSIVE vec2 {
		/// Default constructor.
		constexpr vec2() = default;
		/// Constructor that initializes \ref x and \ref y.
		constexpr vec2(T xx, T yy) : x(xx), y(yy) {
		}

		T
			x = 0, ///< The x coordinate.
			y = 0; ///< The y coordinate.

		/// Index components by numbers, 0 for \ref x and 1 for \ref y.
		T &operator[](std::size_t);
		/// Const version of operator[](std::size_t).
		const T &operator[](std::size_t) const;

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

	template <typename T> inline T &vec2<T>::operator[](std::size_t sub) {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}
	template <typename T> inline const T &vec2<T>::operator[](std::size_t sub) const {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}

	/// Represnts a rectangular area.
	///
	/// \tparam T Type of all coordinates.
	template <typename T> struct APIGEN_EXPORT_RECURSIVE rect {
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
		constexpr bool has_positive_area() const {
			return xmax > xmin && ymax > ymin;
		}
		/// Returns true if both \ref xmax >= \ref xmin and \ref ymax >= \ref ymin.
		constexpr bool has_nonnegative_area() const {
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
		inline static constexpr rect from_corners(vec2<T> min_corner, vec2<T> max_corner) {
			return rect(min_corner.x, max_corner.x, min_corner.y, max_corner.y);
		}
		/// Constructs a rectangle given the position of its top-left corner and its size.
		inline static constexpr rect from_corner_and_size(vec2<T> min_corner, vec2<T> size) {
			return rect(min_corner.x, min_corner.x + size.x, min_corner.y, min_corner.y + size.y);
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
	template <typename T, std::size_t W, std::size_t H> struct APIGEN_EXPORT_RECURSIVE matrix {
		/// A row of the matrix.
		using row = T[W];

		row elem[H]{}; ///< Elements of the matrix.

		/// Sets all elements to zero.
		constexpr void set_zero() {
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					elem[y][x] = 0;
				}
			}
		}
		/// Sets all elements on the diagonal line to 1, and all other elements to 0.
		constexpr void set_identity() {
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					elem[y][x] = static_cast<T>(x == y ? 1 : 0);
				}
			}
		}
		/// Returns an identity matarix.
		inline static constexpr matrix identity() {
			matrix m;
			m.set_identity();
			return m;
		}

		/// Converts all values to another type.
		///
		/// \tparam U The desired type.
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic_v<U>, matrix<U, W, H>> convert() const {
			matrix<U, W, H> res;
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					res[y][x] = static_cast<U>(elem[y][x]);
				}
			}
			return res;
		}

	private:
		inline static void _dot(T ax, T ay, T az, T bx, T by, T bz, T &rx, T &ry, T &rz) {
			rx = ay * bz - az * by;
			ry = az * bx - ax * bz;
			rz = ax * by - ay * bx;
		}
	public:
		/// Returns the transpose of this matrix.
		constexpr matrix<T, H, W> transpose() const {
			matrix<T, H, W> res;
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					res[x][y] = elem[y][x];
				}
			}
			return res;
		}
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
		row &operator[](std::size_t y) {
			return elem[y];
		}
		/// Const version of \ref operator[].
		const row &operator[](std::size_t y) const {
			return elem[y];
		}

		/// Addition.
		matrix &operator+=(const matrix &rhs) {
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					elem[y][x] += rhs[y][x];
				}
			}
			return *this;
		}
		/// Addition.
		friend matrix operator+(matrix lhs, const matrix &rhs) {
			return lhs += rhs;
		}

		/// Subtraction.
		matrix &operator-=(const matrix &rhs) {
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					elem[y][x] -= rhs[y][x];
				}
			}
			return *this;
		}
		/// Subtraction.
		friend matrix operator-(matrix lhs, const matrix &rhs) {
			return lhs -= rhs;
		}

		/// In-place matrix multiplication, only for square matrices.
		std::enable_if_t<W == H, matrix&> operator*=(const matrix &rhs) {
			matrix res;
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
					for (std::size_t k = 0; k < W; ++k) {
						res[y][x] = elem[y][k] * rhs[k][x];
					}
				}
			}
			return *this = res;
		}
		/// Scalar multiplication.
		matrix &operator*=(T rhs) {
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
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
			for (std::size_t y = 0; y < H; ++y) {
				for (std::size_t x = 0; x < W; ++x) {
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
		std::enable_if_t<W == 3 && H == 3, vec2<T>> transform_position(vec2<T> v) const {
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


		/// Checks if the matrix has any rotation or non-rigid (scale, skew) components. If this function returns
		/// \p false, then the matrix is a pure translation matrix.
		constexpr std::enable_if_t<H == W, bool> has_rotation_or_nonrigid(T eps = 1e-2f) const {
			for (std::size_t y = 0; y < H - 1; ++y) {
				for (std::size_t x = 0; x < W - 1; ++x) {
					if (!approximately_equals<T>(elem[y][x], x == y ? 1 : 0, eps)) {
						return true;
					}
				}
			}
			return false;
		}
	};
	/// Matrix multiplication.
	template <typename T, std::size_t M, std::size_t N, std::size_t P> inline matrix<T, P, M> operator*(
		const matrix<T, N, M> &lhs, const matrix<T, P, N> &rhs
		) {
		matrix<T, P, M> result;
		for (std::size_t y = 0; y < M; ++y) {
			for (std::size_t x = 0; x < P; ++x) {
				for (std::size_t k = 0; k < N; ++k) {
					result[y][x] += lhs[y][k] * rhs[k][x];
				}
			}
		}
		return result;
	}
	/// Multiplication of 2x2 matrices with \ref vec2 "vec2s".
	template <typename T> inline vec2<T> operator*(const matrix<T, 2, 2> &lhs, vec2<T> rhs) {
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
}
