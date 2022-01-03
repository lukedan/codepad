// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Custom \p asssert_* functions.

#include "misc.h"
#include "logging.h"

namespace codepad {
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

	/// Assertion, with a given error message. The main purpose of declaring this as a function is that expressions
	/// used as the predicates will always be evaluated, unlike \p assert. This function should only be used when
	/// these conditions lead to unrecoverable errors.
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
			logger::get().log_error() << "System error encountered: " << msg << logger::stacktrace;
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
			logger::get().log_error() << "Usage error encountered: " << msg << logger::stacktrace;
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
			logger::get().log_error() << "Logical error encountered: " << msg << logger::stacktrace;
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


	/// Attempts to \p dynamic_cast the pointer, logging a warning if fails.
	template <typename Desired, typename Base> [[nodiscard]] Desired *checked_dynamic_cast(
		Base *b, std::u8string_view usage = u8""
	) {
		static_assert(std::is_base_of_v<Base, Desired>, "dynamic_cast to non-derived class");
		if (auto *res = dynamic_cast<Desired*>(b)) {
			return res;
		}
		if (b) { // incorrect type
			auto log = logger::get().log_warning();
			log << "dynamic_cast to " << demangle(typeid(Desired).name());
			if (!usage.empty()) {
				log << " (" << usage << ")";
			}
			log << " failed for " << b << " (type: " << demangle(typeid(*b).name()) << ")";
		}
		return nullptr;
	}
}
