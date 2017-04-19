#pragma once

#include <cmath>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace codepad {
	template <typename T> struct vec2 {
		vec2() = default;
		vec2(T xx, T yy) : x(xx), y(yy) {
		}

		T x = 0, y = 0;

		T &operator[](std::size_t sub) {
			assert(sub < 2);
			return (&x)[sub];
		}
		const T &operator[](std::size_t sub) const {
			assert(sub < 2);
			return (&x)[sub];
		}

		template <typename U> vec2<U> convert() const {
			return vec2<U>(static_cast<U>(x), static_cast<U>(y));
		}

		T length_sqr() const {
			return x * x + y * y;
		}
		T length() const {
			return std::sqrt(length_sqr());
		}

		vec2 &operator+=(vec2 rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		friend vec2 operator+(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		friend vec2 operator-(vec2 v) {
			return vec2(-v.x, -v.y);
		}
		vec2 &operator-=(vec2 rhs) {
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}
		friend vec2 operator-(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x - rhs.x, lhs.y - rhs.y);
		}

		vec2 &operator*=(T rhs) {
			x *= rhs;
			y *= rhs;
			return *this;
		}
		friend vec2 operator*(vec2 lhs, T rhs) {
			return vec2(lhs.x * rhs, lhs.y * rhs);
		}
		friend vec2 operator*(T lhs, vec2 rhs) {
			return vec2(lhs * rhs.x, lhs * rhs.y);
		}

		vec2 &operator/=(T rhs) {
			x /= rhs;
			y /= rhs;
			return *this;
		}
		friend vec2 operator/(vec2 lhs, T rhs) {
			return vec2(lhs.x / rhs, lhs.y / rhs);
		}
	};
	typedef vec2<double> vec2d;
	typedef vec2<float> vec2f;
	typedef vec2<int> vec2i;
	typedef vec2<unsigned int> vec2u;

	template <typename T> struct rect {
		rect() = default;
		rect(T minx, T maxx, T miny, T maxy) : xmin(minx), xmax(maxx), ymin(miny), ymax(maxy) {
		}

		T xmin = 0, xmax = 0, ymin = 0, ymax = 0;

		T width() const {
			return xmax - xmin;
		}
		T height() const {
			return ymax - ymin;
		}
		T size() const {
			return width() * height();
		}
		bool positive_size() const {
			return xmax > xmin && ymax > ymin;
		}
		bool nonnegative_size() const {
			return xmax >= xmin && ymax >= ymin;
		}

		template <typename U> typename std::enable_if<std::is_integral<T>::value, rect<U>>::type convert() const {
			return rect<U>(static_cast<U>(xmin), static_cast<U>(xmax), static_cast<U>(ymin), static_cast<U>(ymax));
		}
		template <typename U> typename std::enable_if<std::is_integral<U>::value, rect<U>>::type minimum_bounding_box() const {
			return rect<U>(
				static_cast<U>(std::floor(xmin)), static_cast<U>(std::ceil(xmax)),
				static_cast<U>(std::floor(ymin)), static_cast<U>(std::ceil(ymax))
				);
		}
		template <typename U> typename std::enable_if<std::is_integral<U>::value, rect<U>>::type maximum_contained_box() const {
			return rect<U>(
				static_cast<U>(std::ceil(xmin)), static_cast<U>(std::floor(xmax)),
				static_cast<U>(std::ceil(ymin)), static_cast<U>(std::floor(ymax))
				);
		}

		rect translated(vec2<T> diff) const {
			return rect(xmin + diff.x, xmax + diff.x, ymin + diff.y, ymax + diff.y);
		}

		inline static rect common_part(rect lhs, rect rhs) {
			return rect(
				std::max(lhs.xmin, rhs.xmin), std::min(lhs.xmax, rhs.xmax),
				std::max(lhs.ymin, rhs.ymin), std::min(lhs.ymax, rhs.ymax)
			);
		}
		inline static rect bounding_box(rect lhs, rect rhs) {
			return rect(
				std::min(lhs.xmin, rhs.xmin), std::max(lhs.xmax, rhs.xmax),
				std::min(lhs.ymin, rhs.ymin), std::max(lhs.ymax, rhs.ymax)
			);
		}

		inline static rect from_xywh(T x, T y, T w, T h) {
			return rect(x, x + w, y, y + h);
		}
	};
	typedef rect<double> rectd;
	typedef rect<float> rectf;
	typedef rect<int> recti;
	typedef rect<unsigned int> rectu;

	template <typename T> struct color {
		static_assert(std::is_same<T, unsigned char>::value || std::is_floating_point<T>::value, "invalid color component type");
		constexpr static T max_value = std::is_floating_point<T>::value ? 1 : 255;

		color() = default;
		color(T rr, T gg, T bb, T aa) : r(rr), g(gg), b(bb), a(aa) {
		}

		T r = max_value, g = max_value, b = max_value, a = max_value;

		template <typename U> color<U> convert() const {
			static_assert(!std::is_same<T, U>::value, "invalid conversion");
			if (std::is_same<U, unsigned char>::value) {
				return color<U>(
					static_cast<U>(std::round(r * 255.0)), static_cast<U>(std::round(g * 255.0)),
					static_cast<U>(std::round(b * 255.0)), static_cast<U>(std::round(a * 255.0))
					);
			} else if (std::is_same<T, unsigned char>::value) {
				return color<U>(
					static_cast<U>(r / 255.0), static_cast<U>(g / 255.0),
					static_cast<U>(b / 255.0), static_cast<U>(a / 255.0)
					);
			} else {
				return color<U>(static_cast<U>(r), static_cast<U>(g), static_cast<U>(b), static_cast<U>(a));
			}
		}

		color &operator+=(color val) {
			r += val.r;
			g += val.g;
			b += val.b;
			a += val.a;
			return *this;
		}
		friend color operator+(color lhs, color rhs) {
			return lhs += rhs;
		}

		color &operator-=(color val) {
			r -= val.r;
			g -= val.g;
			b -= val.b;
			a -= val.a;
			return *this;
		}
		friend color operator-(color lhs, color rhs) {
			return lhs -= rhs;
		}

		color &operator*=(T val) {
			r *= val;
			g *= val;
			b *= val;
			a *= val;
			return *this;
		}
		color &operator*=(color val) {
			r *= val.r;
			g *= val.g;
			b *= val.b;
			a *= val.a;
			return *this;
		}
		friend color operator*(color lhs, T rhs) {
			return lhs *= rhs;
		}
		friend color operator*(T lhs, color rhs) {
			return rhs *= lhs;
		}
		friend color operator*(color lhs, color rhs) {
			return lhs *= rhs;
		}

		color &operator/=(T val) {
			r /= val;
			g /= val;
			b /= val;
			a /= val;
			return *this;
		}
		friend color operator/(color lhs, T rhs) {
			return lhs /= rhs;
		}
	};
	typedef color<double> colord;
	typedef color<float> colorf;
	typedef color<unsigned char> colori;

	template <typename T> inline T clamp(T v, T min, T max) {
		if (v < min) {
			return min;
		}
		return v > max ? max : v;
	}
}
