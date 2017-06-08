#pragma once

#include <cmath>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace codepad {
	template <typename T> struct vec2 {
		constexpr vec2() = default;
		constexpr vec2(T xx, T yy) : x(xx), y(yy) {
		}

		T x = 0, y = 0;

		T &operator[](size_t sub) {
			assert(sub < 2);
			return (&x)[sub];
		}
		const T &operator[](size_t sub) const {
			assert(sub < 2);
			return (&x)[sub];
		}

		template <typename U> constexpr vec2<U> convert() const {
			return vec2<U>(static_cast<U>(x), static_cast<U>(y));
		}

		constexpr T length_sqr() const {
			return x * x + y * y;
		}
		constexpr T length() const {
			return std::sqrt(length_sqr());
		}

		vec2 &operator+=(vec2 rhs) {
			x += rhs.x;
			y += rhs.y;
			return *this;
		}
		constexpr friend vec2 operator+(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x + rhs.x, lhs.y + rhs.y);
		}

		constexpr friend vec2 operator-(vec2 v) {
			return vec2(-v.x, -v.y);
		}
		vec2 &operator-=(vec2 rhs) {
			x -= rhs.x;
			y -= rhs.y;
			return *this;
		}
		constexpr friend vec2 operator-(vec2 lhs, vec2 rhs) {
			return vec2(lhs.x - rhs.x, lhs.y - rhs.y);
		}

		vec2 &operator*=(T rhs) {
			x *= rhs;
			y *= rhs;
			return *this;
		}
		constexpr friend vec2 operator*(vec2 lhs, T rhs) {
			return vec2(lhs.x * rhs, lhs.y * rhs);
		}
		constexpr friend vec2 operator*(T lhs, vec2 rhs) {
			return vec2(lhs * rhs.x, lhs * rhs.y);
		}

		vec2 &operator/=(T rhs) {
			x /= rhs;
			y /= rhs;
			return *this;
		}
		constexpr friend vec2 operator/(vec2 lhs, T rhs) {
			return vec2(lhs.x / rhs, lhs.y / rhs);
		}
	};
	typedef vec2<double> vec2d;
	typedef vec2<float> vec2f;
	typedef vec2<int> vec2i;
	typedef vec2<unsigned int> vec2u;

	template <typename T> struct rect {
		constexpr rect() = default;
		constexpr rect(T minx, T maxx, T miny, T maxy) : xmin(minx), xmax(maxx), ymin(miny), ymax(maxy) {
		}

		T xmin = 0, xmax = 0, ymin = 0, ymax = 0;

		constexpr T width() const {
			return xmax - xmin;
		}
		constexpr T height() const {
			return ymax - ymin;
		}
		constexpr vec2<T> size() const {
			return vec2<T>(width(), height());
		}
		constexpr vec2<T> xmin_ymin() const {
			return vec2<T>(xmin, ymin);
		}
		constexpr vec2<T> xmax_ymin() const {
			return vec2<T>(xmax, ymin);
		}
		constexpr vec2<T> xmin_ymax() const {
			return vec2<T>(xmin, ymax);
		}
		constexpr vec2<T> xmax_ymax() const {
			return vec2<T>(xmax, ymax);
		}

		constexpr double centerx() const {
			return (xmin + xmax) * 0.5f;
		}
		constexpr double centery() const {
			return (ymin + ymax) * 0.5f;
		}
		constexpr vec2<T> center() const {
			return vec2<T>(centerx(), centery());
		}

		constexpr bool positive_area() const {
			return xmax > xmin && ymax > ymin;
		}
		constexpr bool nonnegative_area() const {
			return xmax >= xmin && ymax >= ymin;
		}

		constexpr bool contains(vec2<T> v) const {
			return v.x >= xmin && v.x <= xmax && v.y >= ymin && v.y <= ymax;
		}
		constexpr bool fully_contains(vec2<T> v) const {
			return v.x > xmin && v.x < xmax && v.y > ymin && v.y < ymax;
		}

		void make_valid_min() {
			if (xmin > xmax) {
				xmin = xmax;
			}
			if (ymin > ymax) {
				ymin = ymax;
			}
		}
		void make_valid_max() {
			if (xmin > xmax) {
				xmax = xmin;
			}
			if (ymin > ymax) {
				ymax = ymin;
			}
		}

		template <typename U> constexpr typename std::enable_if<std::is_integral<T>::value, rect<U>>::type convert() const {
			return rect<U>(static_cast<U>(xmin), static_cast<U>(xmax), static_cast<U>(ymin), static_cast<U>(ymax));
		}
		template <typename U> constexpr typename std::enable_if<std::is_integral<U>::value, rect<U>>::type minimum_bounding_box() const {
			return rect<U>(
				static_cast<U>(std::floor(xmin)), static_cast<U>(std::ceil(xmax)),
				static_cast<U>(std::floor(ymin)), static_cast<U>(std::ceil(ymax))
				);
		}
		template <typename U> constexpr typename std::enable_if<std::is_integral<U>::value, rect<U>>::type maximum_contained_box() const {
			return rect<U>(
				static_cast<U>(std::ceil(xmin)), static_cast<U>(std::floor(xmax)),
				static_cast<U>(std::ceil(ymin)), static_cast<U>(std::floor(ymax))
				);
		}

		constexpr rect translated(vec2<T> diff) const {
			return rect(xmin + diff.x, xmax + diff.x, ymin + diff.y, ymax + diff.y);
		}

		inline static constexpr rect common_part(rect lhs, rect rhs) {
			return rect(
				std::max(lhs.xmin, rhs.xmin), std::min(lhs.xmax, rhs.xmax),
				std::max(lhs.ymin, rhs.ymin), std::min(lhs.ymax, rhs.ymax)
			);
		}
		inline static constexpr rect bounding_box(rect lhs, rect rhs) {
			return rect(
				std::min(lhs.xmin, rhs.xmin), std::max(lhs.xmax, rhs.xmax),
				std::min(lhs.ymin, rhs.ymin), std::max(lhs.ymax, rhs.ymax)
			);
		}

		inline static constexpr rect from_xywh(T x, T y, T w, T h) {
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
					static_cast<U>(0.5 + r * 255.0), static_cast<U>(0.5 + g * 255.0),
					static_cast<U>(0.5 + b * 255.0), static_cast<U>(0.5 + a * 255.0)
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

	template <typename T, typename U> inline bool test_bit_all(T v, U bit) {
		return (v & static_cast<T>(bit)) == static_cast<T>(bit);
	}
	template <typename T, typename U> inline bool test_bit_any(T v, U bit) {
		return (v & static_cast<T>(bit)) != 0;
	}
	template <typename T, typename U> inline void set_bit(T &v, U bit) {
		v = v | static_cast<T>(bit);
	}
	template <typename T, typename U> inline void unset_bit(T &v, U bit) {
		v = v & ~static_cast<T>(bit);
	}
	template <typename T, typename U> inline T with_bit_set(T v, U bit) {
		return v | static_cast<T>(bit);
	}
	template <typename T, typename U> inline T with_bit_unset(T v, U bit) {
		return v & static_cast<T>(~static_cast<T>(bit));
	}
}

namespace codepad {
	template <typename T> inline void print_to_cout(T &&arg) {
		std::cout << std::forward<T>(arg);
	}
	template <typename First, typename ...Others> inline void print_to_cout(First &&f, Others &&...args) {
		print_to_cout(std::forward<First>(f));
		print_to_cout(std::forward<Others>(args)...);
	}
}
#define CP_INFO(...) ::codepad::print_to_cout("INFO|", __func__, ":", __LINE__, "|", __VA_ARGS__, "\n")

#if defined(_MSC_VER) && !defined(NDEBUG)
#	define _CRTDBG_MAP_ALLOC
#	include <stdlib.h>
#	include <crtdbg.h>
namespace codepad {
	inline void enable_mem_checking() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	}
}
#endif
