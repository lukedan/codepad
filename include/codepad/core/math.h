// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Basic mathmatics.

#include <cmath>
#include <type_traits>

#include "json/misc.h"
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
		[[nodiscard]] T &operator[](std::size_t);
		/// Const version of operator[](std::size_t).
		[[nodiscard]] const T &operator[](std::size_t) const;

		/// Converts all components to another type and returns the result.
		///
		/// \tparam U The desired type of components.
		template <typename U> [[nodiscard]] constexpr vec2<U> convert() const {
			return vec2<U>(static_cast<U>(x), static_cast<U>(y));
		}

		/// Returns the squared length of the vector.
		[[nodiscard]] constexpr T length_sqr() const {
			return x * x + y * y;
		}
		/// Returns the length of the vector.
		[[nodiscard]] constexpr T length() const {
			return std::sqrt(length_sqr());
		}

		/// Addition.
		vec2 &operator+=(vec2 rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		/// Addition.
		[[nodiscard]] constexpr friend vec2 operator+(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		/// Subtraction.
		[[nodiscard]] constexpr friend vec2 operator-(vec2 v) {
			return vec2(-v.x, -v.y);
		}
		/// Subtraction.
		vec2 &operator-=(vec2 rhs) {
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}
		/// Subtraction.
		[[nodiscard]] constexpr friend vec2 operator-(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x - rhs.x, lhs.y - rhs.y);
		}

		/// Scalar multiplication.
		vec2 &operator*=(T rhs) {
			x *= rhs;
			y *= rhs;
			return *this;
		}
		/// Scalar multiplication.
		[[nodiscard]] constexpr friend vec2 operator*(vec2 lhs, T rhs) {
			return vec2(lhs.x * rhs, lhs.y * rhs);
		}
		/// Scalar multiplication.
		[[nodiscard]] constexpr friend vec2 operator*(T lhs, vec2 rhs) {
			return vec2(lhs * rhs.x, lhs * rhs.y);
		}

		/// Scalar division.
		vec2 &operator/=(T rhs) {
			x /= rhs;
			y /= rhs;
			return *this;
		}
		/// Scalar division.
		[[nodiscard]] constexpr friend vec2 operator/(vec2 lhs, T rhs) {
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

	template <typename T> [[nodiscard]] inline T &vec2<T>::operator[](std::size_t sub) {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}
	template <typename T> [[nodiscard]] inline const T &vec2<T>::operator[](std::size_t sub) const {
		assert_true_usage(sub < 2, "invalid subscript");
		return (&x)[sub];
	}

	namespace json {
		/// Parser for \ref vec2d.
		template <> struct default_parser<vec2d> {
			/// Parses \ref vec2d. The object must be of the following format: <tt>[x, y]</tt>
			template <typename Value> std::optional<vec2d> operator()(const Value&) const;
		};
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
		[[nodiscard]] constexpr T width() const {
			return xmax - xmin;
		}
		/// Returns the height of the rectangle, i.e., \ref ymax - \ref ymin.
		[[nodiscard]] constexpr T height() const {
			return ymax - ymin;
		}
		/// Returns the area of the rectangle, i.e., width() * height().
		[[nodiscard]] constexpr vec2<T> size() const {
			return vec2<T>(width(), height());
		}
		/// Returns the vector (\ref xmin, \ref ymin).
		[[nodiscard]] constexpr vec2<T> xmin_ymin() const {
			return vec2<T>(xmin, ymin);
		}
		/// Returns the vector (\ref xmax, \ref ymin).
		[[nodiscard]] constexpr vec2<T> xmax_ymin() const {
			return vec2<T>(xmax, ymin);
		}
		/// Returns the vector (\ref xmin, \ref ymax).
		[[nodiscard]] constexpr vec2<T> xmin_ymax() const {
			return vec2<T>(xmin, ymax);
		}
		/// Returns the vector (\ref xmax, \ref ymax).
		[[nodiscard]] constexpr vec2<T> xmax_ymax() const {
			return vec2<T>(xmax, ymax);
		}

		/// Returns the x coordinate of the rectangle's center, i.e., \f$\frac{1}{2}\f$ (\ref xmin + \ref xmax).
		[[nodiscard]] constexpr double centerx() const {
			return (xmin + xmax) * 0.5f;
		}
		/// Returns the y coordinate of the rectangle's center, i.e., \f$\frac{1}{2}\f$ (\ref ymin + \ref ymax).
		[[nodiscard]] constexpr double centery() const {
			return (ymin + ymax) * 0.5f;
		}
		/// Returns the coordinate of the rectangle's center, i.e., (centerx(), centery()).
		[[nodiscard]] constexpr vec2<T> center() const {
			return vec2<T>(centerx(), centery());
		}

		/// Returns true if both \ref xmax > \ref xmin and \ref ymax > \ref ymin.
		[[nodiscard]] constexpr bool has_positive_area() const {
			return xmax > xmin && ymax > ymin;
		}
		/// Returns true if both \ref xmax >= \ref xmin and \ref ymax >= \ref ymin.
		[[nodiscard]] constexpr bool has_nonnegative_area() const {
			return xmax >= xmin && ymax >= ymin;
		}

		/// Returns true if a given point lies in the rectangle or on its boundary.
		///
		/// \param v The given point.
		[[nodiscard]] constexpr bool contains(vec2<T> v) const {
			return v.x >= xmin && v.x <= xmax && v.y >= ymin && v.y <= ymax;
		}
		/// Returns true if a given point completely lies in the rectangle, i.e., does not touch the boundary.
		///
		/// \param v The given point.
		[[nodiscard]] constexpr bool fully_contains(vec2<T> v) const {
			return v.x > xmin && v.x < xmax && v.y > ymin && v.y < ymax;
		}

		/// Returns whether any field of this struct is \p NaN.
		template <
			typename Dummy = int, std::enable_if_t<std::is_floating_point_v<T>, Dummy> = 0
		> [[nodiscard]] bool contains_nan() const {
			return std::isnan(xmin) || std::isnan(xmax) || std::isnan(ymin) || std::isnan(ymax);
		}

		/// Returns this rectangle adjusted so that \ref xmax > \ref xmin and \ref ymax > \ref ymin. If a condition
		/// is not met, the average value is assigned to the bounds.
		[[nodiscard]] constexpr rect made_positive_average() const {
			rect res = *this;
			if (res.xmin > res.xmax) {
				res.xmin = res.xmax = 0.5 * (res.xmin + res.xmax);
			}
			if (res.ymin > res.ymax) {
				res.ymin = res.ymax = 0.5 * (res.ymin + res.ymax);
			}
			return res;
		}
		/// Similar to \ref make_positive_average() but uses the minimum bound of the range.
		[[nodiscard]] constexpr rect made_positive_min() const {
			rect res = *this;
			if (res.xmin > res.xmax) {
				res.xmin = res.xmax;
			}
			if (res.ymin > res.ymax) {
				res.ymin = res.ymax;
			}
			return res;
		}
		/// Similar to \ref make_positive_average() but uses the maximum bound of the range.
		[[nodiscard]] constexpr rect made_positive_max() const {
			rect res = *this;
			if (res.xmin > res.xmax) {
				res.xmax = res.xmin;
			}
			if (res.ymin > res.ymax) {
				res.ymax = res.ymin;
			}
			return res;
		}
		/// Similar to \ref make_positive_average() but swaps the endpoints of the range.
		[[nodiscard]] constexpr rect made_positive_swap() const {
			rect res = *this;
			if (res.xmin > res.xmax) {
				std::swap(res.xmin, res.xmax);
			}
			if (res.ymin > res.ymax) {
				std::swap(res.ymin, res.ymax);
			}
			return res;
		}

		/// Converts all coordinates to another type.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> [[nodiscard]] constexpr std::enable_if_t<
			std::is_arithmetic_v<U>, rect<U>
		> convert() const {
			return rect<U>(static_cast<U>(xmin), static_cast<U>(xmax), static_cast<U>(ymin), static_cast<U>(ymax));
		}
		/// Converts all coordinates to another type, rounding them towards out of the rectangle.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> [[nodiscard]] constexpr std::enable_if_t<
			std::is_arithmetic_v<U>, rect<U>
		> fit_grid_enlarge() const {
			return rect<U>(
				static_cast<U>(std::floor(xmin)), static_cast<U>(std::ceil(xmax)),
				static_cast<U>(std::floor(ymin)), static_cast<U>(std::ceil(ymax))
				);
		}
		/// Converts all coordinates to another type, rounding them towards inside of the rectangle.
		///
		/// \tparam U The desired type of the coordinates.
		template <typename U> [[nodiscard]] constexpr std::enable_if_t<
			std::is_arithmetic_v<U>, rect<U>
		> fit_grid_shrink() const {
			return rect<U>(
				static_cast<U>(std::ceil(xmin)), static_cast<U>(std::floor(xmax)),
				static_cast<U>(std::ceil(ymin)), static_cast<U>(std::floor(ymax))
				);
		}

		/// Returns the rectangle translated by a vector.
		///
		/// \param diff The desired offset.
		[[nodiscard]] constexpr rect translated(vec2<T> diff) const {
			return rect(xmin + diff.x, xmax + diff.x, ymin + diff.y, ymax + diff.y);
		}
		/// Returns the rectangle scaled with respect to a given point by a given scalar.
		///
		/// \param center The center of scaling.
		/// \param scale The desired scale.
		[[nodiscard]] constexpr rect scaled(vec2<T> center, double scale) const {
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
		[[nodiscard]] constexpr rect coordinates_scaled(double scale) const {
			return rect(
				static_cast<T>(scale * xmin),
				static_cast<T>(scale * xmax),
				static_cast<T>(scale * ymin),
				static_cast<T>(scale * ymax)
			);
		}

		/// Returns the largest rectangle that's fully contained by both given rectangles.
		/// If the two rectangles don't intersect, the returned rectangle will have negative area.
		[[nodiscard]] inline static constexpr rect common_part(rect lhs, rect rhs) {
			return rect(
				std::max(lhs.xmin, rhs.xmin), std::min(lhs.xmax, rhs.xmax),
				std::max(lhs.ymin, rhs.ymin), std::min(lhs.ymax, rhs.ymax)
			);
		}
		/// Returns the smallest rectangle that fully contains both given rectangles.
		[[nodiscard]] inline static constexpr rect bounding_box(rect lhs, rect rhs) {
			return rect(
				std::min(lhs.xmin, rhs.xmin), std::max(lhs.xmax, rhs.xmax),
				std::min(lhs.ymin, rhs.ymin), std::max(lhs.ymax, rhs.ymax)
			);
		}

		/// Constructs a rectangle given its top-left corner, width, and height.
		[[nodiscard]] inline static constexpr rect from_xywh(T x, T y, T w, T h) {
			return rect(x, x + w, y, y + h);
		}
		/// Constructs a rectangle given the position of its top-left corner and its bottom-right corner.
		[[nodiscard]] inline static constexpr rect from_corners(vec2<T> min_corner, vec2<T> max_corner) {
			return rect(min_corner.x, max_corner.x, min_corner.y, max_corner.y);
		}
		/// Constructs a rectangle given the position of its top-left corner and its size.
		[[nodiscard]] inline static constexpr rect from_corner_and_size(vec2<T> min_corner, vec2<T> size) {
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
		[[nodiscard]] inline static constexpr matrix identity() {
			matrix m;
			m.set_identity();
			return m;
		}

		/// Converts all values to another type.
		///
		/// \tparam U The desired type.
		template <typename U> [[nodiscard]] constexpr std::enable_if_t<
			std::is_arithmetic_v<U>, matrix<U, W, H>
		> convert() const {
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
		[[nodiscard]] constexpr matrix<T, H, W> transpose() const {
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
		[[nodiscard]] constexpr std::enable_if_t<W == 3 && H == 3, matrix> inverse() const {
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
		[[nodiscard]] row &operator[](std::size_t y) {
			return elem[y];
		}
		/// Const version of \ref operator[].
		[[nodiscard]] const row &operator[](std::size_t y) const {
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
		[[nodiscard]] friend matrix operator+(matrix lhs, const matrix &rhs) {
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
		[[nodiscard]] friend matrix operator-(matrix lhs, const matrix &rhs) {
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
		[[nodiscard]] friend matrix operator*(matrix lhs, T rhs) {
			return lhs *= rhs;
		}
		/// Scalar multiplication.
		[[nodiscard]] friend matrix operator*(T lhs, matrix rhs) {
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
		[[nodiscard]] friend matrix operator/(matrix lhs, T rhs) {
			return lhs /= rhs;
		}

		/// Given a \ref vec2 (x, y), returns the vector obtained by multiplying this matrix with (x, y, 1).
		[[nodiscard]] std::enable_if_t<W == 3 && H == 3, vec2<T>> transform_position(vec2<T> v) const {
			return vec2<T>(
				elem[0][0] * v.x + elem[0][1] * v.y + elem[0][2],
				elem[1][0] * v.x + elem[1][1] * v.y + elem[1][2]
				);
		}

		/// Returns a matrix that translates vectors by a given offset.
		///
		/// \param off The desired offset.
		[[nodiscard]] inline static std::enable_if_t<W == 3 && H == 3, matrix> translate(vec2<T> off) {
			matrix res;
			res[0][0] = res[1][1] = res[2][2] = 1.0;
			res[0][2] = off.x;
			res[1][2] = off.y;
			return res;
		}
		/// Returns a matrix that rotates vectors around \p center by \p rotv,
		/// where \p rotv is a vector of the form \f$(\cos{\alpha}, \sin{\alpha})\f$.
		/// Scale can also be combined by multiplying \p rotv with the scale factor.
		[[nodiscard]] inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_by_vector(
			vec2<T> center, vec2<T> rotv
		) {
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
		[[nodiscard]] inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_clockwise(
			vec2<T> center, double radians
		) {
			return rotate_by_vector(center, vec2<T>(std::cos(radians), std::sin(radians)));
		}
		/// Returns a matrix that scales vectors around \p center by \p scale.
		/// The x coordinate is scaled by \p scale.x and y by \p scale.y.
		[[nodiscard]] inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, vec2<T> scale) {
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
		[[nodiscard]] inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, T uniscale) {
			return scale(center, vec2<T>(uniscale, uniscale));
		}


		/// Checks if the matrix has any rotation or non-rigid (scale, skew) components. If this function returns
		/// \p false, then the matrix is a pure translation matrix.
		[[nodiscard]] constexpr std::enable_if_t<H == W, bool> has_rotation_or_nonrigid(T eps = 1e-2f) const {
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
	template <
		typename T, std::size_t M, std::size_t N, std::size_t P
	> [[nodiscard]] inline matrix<T, P, M> operator*(const matrix<T, N, M> &lhs, const matrix<T, P, N> &rhs) {
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
	template <typename T> [[nodiscard]] inline vec2<T> operator*(const matrix<T, 2, 2> &lhs, vec2<T> rhs) {
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
