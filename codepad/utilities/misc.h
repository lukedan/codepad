#pragma once

#include <cmath>
#include <exception>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <type_traits>
#include <chrono>

#include "textproc.h"

namespace codepad {
	struct globals;

	template <typename T> struct vec2 {
		constexpr vec2() = default;
		constexpr vec2(T xx, T yy) : x(xx), y(yy) {
		}

		T x = 0, y = 0;

		T &operator[](size_t);
		const T &operator[](size_t) const;

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

		template <typename U> constexpr std::enable_if_t<std::is_arithmetic<U>::value, rect<U>> convert() const {
			return rect<U>(static_cast<U>(xmin), static_cast<U>(xmax), static_cast<U>(ymin), static_cast<U>(ymax));
		}
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic<U>::value, rect<U>> minimum_bounding_box() const {
			return rect<U>(
				static_cast<U>(std::floor(xmin)), static_cast<U>(std::ceil(xmax)),
				static_cast<U>(std::floor(ymin)), static_cast<U>(std::ceil(ymax))
				);
		}
		template <typename U> constexpr std::enable_if_t<std::is_arithmetic<U>::value, rect<U>> maximum_contained_box() const {
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

	template <typename T, size_t W, size_t H> struct matrix {
		typedef T row[W];

		row elem[H]{};

		void set_zero() {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] = 0;
				}
			}
		}
		void set_identity() {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] = (x == y ? 1 : 0);
				}
			}
		}

		row &operator[](size_t y) {
			return elem[y];
		}
		const row &operator[](size_t y) const {
			return elem[y];
		}

		matrix &operator+=(const matrix &rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] += rhs[y][x];
				}
			}
			return *this;
		}
		friend matrix operator+(matrix lhs, const matrix &rhs) {
			return lhs += rhs;
		}

		matrix &operator-=(const matrix &rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] -= rhs[y][x];
				}
			}
			return *this;
		}
		friend matrix operator-(matrix lhs, const matrix &rhs) {
			return lhs -= rhs;
		}

		matrix &operator*=(T rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] *= rhs;
				}
			}
			return *this;
		}
		friend matrix operator*(matrix lhs, T rhs) {
			return lhs *= rhs;
		}
		friend matrix operator*(T lhs, matrix rhs) {
			return rhs *= lhs;
		}

		matrix &operator/=(T rhs) {
			for (size_t y = 0; y < H; ++y) {
				for (size_t x = 0; x < W; ++x) {
					elem[y][x] /= rhs;
				}
			}
			return *this;
		}
		friend matrix operator/(matrix lhs, T rhs) {
			return lhs /= rhs;
		}

		std::enable_if_t<W == 3 && H == 3, vec2<T>> transform(vec2<T> v) const {
			return vec2<T>(
				elem[0][0] * v.x + elem[0][1] * v.y + elem[0][2],
				elem[1][0] * v.x + elem[1][1] * v.y + elem[1][2]
				);
		}

		inline static std::enable_if_t<W == 3 && H == 3, matrix> translate(vec2<T> off) {
			matrix res;
			res[0][0] = res[1][1] = res[2][2] = 1.0;
			res[0][2] = off.x;
			res[1][2] = off.y;
			return res;
		}
		inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_by_vector(vec2<T> center, vec2<T> rotv) {
			/*
			 * [1  0  cx] [rx  -ry  0] [1  0  -cx] [vx]
			 * [0  1  cy] [ry  rx   0] [0  1  -cy] [vy]
			 * [0  0  1 ] [0    0   1] [0  0   1 ] [1 ]
			 *            [rx  -ry  ry*cy-rx*cx ]
			 *            [ry  rx   -ry*cx-rx*cy]
			 *            [0    0         1     ]
			 * [rx  -ry  ry*cy-rx*cx+cx ]
			 * [ry  rx   -ry*cx-rx*cy+cy]
			 * [0    0          1       ]
			 */
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
		inline static std::enable_if_t<W == 3 && H == 3, matrix> rotate_clockwise(vec2<T> center, double radians) {
			return rotate_by_vector(center, vec2<T>(std::cos(radians), std::sin(radians)));
		}
		inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, vec2<T> scale) {
			/*
			* [1  0  cx] [sx  0   0] [1  0  -cx] [vx]
			* [0  1  cy] [0   sy  0] [0  1  -cy] [vy]
			* [0  0  1 ] [0   0   1] [0  0   1 ] [1 ]
			*            [sx  0   -cx*sx]
			*            [0   sy  -cy*sy]
			*            [0   0      1  ]
			* [sx  0   cx-cx*sx]
			* [0   sy  cy-cy*sy]
			* [0   0       1   ]
			*/
			matrix res;
			res[0][0] = scale.x;
			res[0][2] = center.x * (1.0 - scale.x);
			res[1][1] = scale.y;
			res[1][2] = center.y * (1.0 - scale.y);
			res[2][2] = 1.0;
			return res;
		}
		inline static std::enable_if_t<W == 3 && H == 3, matrix> scale(vec2<T> center, T uniscale) {
			return scale(center, vec2<T>(uniscale, uniscale));
		}
	};
	template <typename T, size_t M, size_t N, size_t P> inline matrix<T, P, M> operator*(
		const matrix<T, N, M> &lhs, const matrix<T, P, N> &rhs
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
	template <typename T> inline vec2<T> operator*(const matrix<T, 2, 2> &lhs, vec2<T> rhs) {
		return vec2<T>(lhs[0][0] * rhs.x + lhs[0][1] * rhs.y, lhs[1][0] * rhs.x + lhs[1][1] * rhs.y);
	}
	template <typename T> inline vec2<T> apply_transform(const matrix<T, 3, 3> &lhs, vec2<T> rhs) {
		return vec2<T>(lhs[0][0] * rhs.x + lhs[0][1] * rhs.y + lhs[0][2], lhs[1][0] * rhs.x + lhs[1][1] * rhs.y + lhs[1][2]);
	}
	typedef matrix<double, 2, 2> matd2x2;
	typedef matrix<double, 3, 3> matd3x3;

	template <typename T> struct color {
		static_assert(std::is_same<T, unsigned char>::value || std::is_floating_point<T>::value, "invalid color component type");
		constexpr static T max_value = std::is_floating_point<T>::value ? 1 : 255;

		color() = default;
		color(T rr, T gg, T bb, T aa) : r(rr), g(gg), b(bb), a(aa) {
		}

		T r = max_value, g = max_value, b = max_value, a = max_value;

		template <typename U> color<U> convert() const {
			if (!std::is_same<T, U>::value) {
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
				}
			}
			return color<U>(static_cast<U>(r), static_cast<U>(g), static_cast<U>(b), static_cast<U>(a));
		}

		friend bool operator==(color<T> lhs, color<T> rhs) {
			return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
		}
		friend bool operator!=(color<T> lhs, color<T> rhs) {
			return !(lhs == rhs);
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
	template <typename T> inline T lerp(T from, T to, double perc) {
		return from + (to - from) * perc;
	}

	template <typename S, typename U> inline S unsigned_diff(U lhs, U rhs) {
		static_assert(std::is_unsigned<U>::value, "must pass unsigned value");
		static_assert(std::is_signed<S>::value, "must pass signed value");
		return lhs > rhs ? static_cast<S>(lhs - rhs) : -static_cast<S>(rhs - lhs);
	}
	template <typename S, typename U> inline U add_unsigned_diff(U lhs, S rhs) {
		static_assert(std::is_unsigned<U>::value, "must pass unsigned value");
		static_assert(std::is_signed<S>::value, "must pass signed value");
		return rhs > 0 ? lhs + static_cast<U>(rhs) : lhs - static_cast<U>(-rhs);
	}
	template <typename S, typename U> inline U subtract_unsigned_diff(U lhs, S rhs) {
		static_assert(std::is_unsigned<U>::value, "must pass unsigned value");
		static_assert(std::is_signed<S>::value, "must pass signed value");
		return rhs > 0 ? lhs - static_cast<U>(rhs) : lhs + static_cast<U>(-rhs);
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
		v = v & static_cast<T>(~static_cast<T>(bit));
	}
	template <typename T, typename U> inline T with_bit_set(T v, U bit) {
		return v | static_cast<T>(bit);
	}
	template <typename T, typename U> inline T with_bit_unset(T v, U bit) {
		return v & static_cast<T>(~static_cast<T>(bit));
	}


	template <typename St, typename T> inline void print_to(St &s, T &&arg) {
		s << std::forward<T>(arg);
	}
	template <typename St, typename First, typename ...Others> inline void print_to(St &s, First &&f, Others &&...args) {
		print_to(s, std::forward<First>(f));
		print_to(s, std::forward<Others>(args)...);
	}

	enum class log_level {
		other,
		info,
		warning,
		error
	};
	struct code_position {
		code_position(const char *fil, const char *func, int l) : file(fil), function(func), line(l) {
		}

		friend std::ostream &operator<<(std::ostream &out, const code_position &cp) {
			return out << cp.function << " @" << cp.file << ":" << cp.line;
		}

		const char *file, *function;
		int line;
	};
#define CP_HERE ::codepad::code_position(__FILE__, __func__, __LINE__)
	struct logger {
	public:
		logger() : logger("default.log") {
		}
		explicit logger(const std::string &s) : _fout(s, std::ios::app), _epoch(std::chrono::high_resolution_clock::now()) {
			log_raw("\n\n####################\n\n");
			log_info(CP_HERE, "new logger \"", s, "\" created");
		}
		~logger() {
			log_info(CP_HERE, "session shutdown");
		}

		template <typename ...Args> void log(log_level level, const code_position &cp, Args &&...args) {
			switch (level) {
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
		template <typename ...Args> void log_info(const code_position &cp, Args &&...args) {
			_log_fmt("INFO", cp, std::forward<Args>(args)...);
		}
		template <typename ...Args> void log_warning(const code_position &cp, Args &&...args) {
			_log_fmt("WARNING", cp, std::forward<Args>(args)...);
		}
		template <typename ...Args> void log_error(const code_position &cp, Args &&...args) {
			_log_fmt("ERROR", cp, std::forward<Args>(args)...);
			_fout.flush();
		}
		template <typename ...Args> void log_error_with_stacktrace(const code_position &cp, Args &&...args) {
			log_error(cp, std::forward<Args>(args)...);
			log_stacktrace();
			_fout.flush();
		}

		template <typename ...Args> void log_custom(Args &&...args) {
			char ss[9];
			std::snprintf(
				ss, sizeof(ss) / sizeof(char),
				"%8.02f", std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - _epoch).count()
			);
			log_raw(ss, "|", std::forward<Args>(args)..., "\n");
		}
		template <typename ...Args> void log_raw(Args &&...args) {
			print_to(_fout, std::forward<Args>(args)...);
			print_to(std::cout, std::forward<Args>(args)...);
		}

		void log_stacktrace();

		void flush() {
			_fout.flush();
		}

		static logger &get();
	protected:
		std::ofstream _fout;
		std::chrono::time_point<std::chrono::high_resolution_clock> _epoch;

		template <typename ...Args> void _log_fmt(const char *header, const code_position &cp, Args &&...args) {
			log_custom(header, "|", cp, "|", std::forward<Args>(args)...);
		}
	};


	enum class error_level {
		system_error, // errors with system api, opengl, etc. nothing we can do
		usage_error, // wrong usage of internal classes, which can also be treated as exceptions
		logical_error // logical errors in codepad which basically shouldn't happen
	};

#ifdef NDEBUG
#	define CP_ERROR_LEVEL 2
#else
#	define CP_ERROR_LEVEL 3
#endif

#if CP_ERROR_LEVEL > 0
#	define CP_DETECT_SYSTEM_ERRORS
#endif
#if CP_ERROR_LEVEL > 1
#		define CP_DETECT_USAGE_ERRORS
#endif
#if CP_ERROR_LEVEL > 2
#	define CP_DETECT_LOGICAL_ERRORS
#endif

	template <error_level Lvl> inline void assert_true(bool v, const char *msg) {
		if (!v) {
			throw std::runtime_error(msg);
		}
	}

#ifdef CP_DETECT_SYSTEM_ERRORS
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
#ifdef CP_DETECT_USAGE_ERRORS
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
#ifdef CP_DETECT_LOGICAL_ERRORS
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

	inline void assert_true_sys(bool v, const char *msg) {
		assert_true<error_level::system_error>(v, msg);
	}
	inline void assert_true_sys(bool v) {
		assert_true<error_level::system_error>(v, "default system error message");
	}
	inline void assert_true_usage(bool v, const char *msg) {
		assert_true<error_level::usage_error>(v, msg);
	}
	inline void assert_true_usage(bool v) {
		assert_true<error_level::usage_error>(v, "default usage error message");
	}
	inline void assert_true_logical(bool v, const char *msg) {
		assert_true<error_level::logical_error>(v, msg);
	}
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
		}

// memory leak detection
#if defined(CP_PLATFORM_WINDOWS) && defined(_MSC_VER)
#	ifdef CP_DETECT_USAGE_ERRORS
#		define CP_CAN_DETECT_MEMORY_LEAKS
#		define _CRTDBG_MAP_ALLOC
#		include <crtdbg.h>
namespace codepad {
	inline void enable_mem_checking() {
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	}
}
#	endif
#endif

// demangle
#ifdef _MSC_VER
namespace codepad {
	inline std::string demangle(std::string s) {
		return s;
	}
}
#elif defined(__GNUC__)
#	include <cxxabi.h>
namespace codepad {
	inline std::string demangle(const std::string &s) {
		int st;
		char *result = abi::__cxa_demangle(s.c_str(), nullptr, nullptr, &st);
		assert_true_sys(st == 0, "demangling failed");
		std::string res(result);
		std::free(result);
		return res;
	}
}
#endif

#ifdef CP_LOG_STACKTRACE
#	if defined(CP_PLATFORM_WINDOWS) && defined(_MSC_VER)
#		include <Windows.h>
#		pragma comment(lib, "dbghelp.lib")
#		ifdef UNICODE
#			define DBGHELP_TRANSLATE_TCHAR
#		endif
#		include <DbgHelp.h>
#		undef min
#		undef max
namespace codepad {
	inline void logger::log_stacktrace() {
		constexpr static DWORD max_frames = 1000;
		constexpr static size_t max_symbol_length = 1000;

		log_custom("STACKTRACE");
		void *frames[max_frames];
		HANDLE proc = GetCurrentProcess();
		unsigned char symmem[sizeof(SYMBOL_INFO) + max_symbol_length * sizeof(TCHAR)];
		PSYMBOL_INFO syminfo = reinterpret_cast<PSYMBOL_INFO>(symmem);
		syminfo->MaxNameLen = max_symbol_length;
		syminfo->SizeOfStruct = sizeof(SYMBOL_INFO);
		IMAGEHLP_LINE64 lineinfo;
		lineinfo.SizeOfStruct = sizeof(lineinfo);
		DWORD line_disp;
		assert_true_sys(
			SymInitialize(GetCurrentProcess(), nullptr, true),
			"failed to initialize symbols"
		);
		WORD numframes = CaptureStackBackTrace(0, max_frames, frames, nullptr);
		for (WORD i = 0; i < numframes; ++i) {
			DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
			std::string func, file, line;
			if (SymFromAddr(proc, addr, nullptr, syminfo)) {
				func = utf8_to_chars(convert_to_utf8(reinterpret_cast<const char16_t*>(syminfo->Name)));
			} else {
				func = "??";
			}
			if (SymGetLineFromAddr64(proc, addr, &line_disp, &lineinfo)) {
				file = utf8_to_chars(convert_to_utf8(reinterpret_cast<const char16_t*>(lineinfo.FileName)));
				line = std::to_string(lineinfo.LineNumber);
			} else {
				file = "??";
				line = "??";
			}
			log_custom("    ", func, "(0x", frames[i], ") @", file, ":", line);
		}
		assert_true_sys(SymCleanup(GetCurrentProcess()), "failed to clean up symbols");
		log_custom("STACKTRACE|END");
	}
}
#	elif defined(CP_PLATFORM_UNIX) && defined(__GNUC__)
#		include <execinfo.h>
namespace codepad {
	inline void logger::log_stacktrace() {
		constexpr static int max_frames = 1000;

		void *frames[max_frames];
		int numframes = backtrace(frames, max_frames);
		char **symbols = backtrace_symbols(frames, numframes);
		assert_true_sys(symbols, "backtrace_symbols() failed");
		log_custom("STACKTRACE");
		for (int i = 0; i < numframes; ++i) {
			log_custom("    ", symbols[i]);
		}
		log_custom("STACKTRACE|END");
		free(symbols);
	}
}
#	else
namespace codepad {
	inline void logger::log_stacktrace() {
		log_warning(CP_HERE, "stacktrace logging is not supported with this configuration");
	}
}
#	endif
#else
namespace codepad {
	inline void logger::log_stacktrace() {
		log_warning(CP_HERE, "stacktrace logging has been disabled");
	}
}
#endif
