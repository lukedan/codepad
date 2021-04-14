// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Loader of dynamic libraries.

#include <filesystem>

#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#endif

#include "codepad/core/encodings.h"
#include "codepad/core/assert.h"

namespace codepad::os {
	/// A reference to a dynamic library.
	struct dynamic_library {
	public:
		/// Native handle type.
		using native_handle_t =
#if defined(CP_PLATFORM_WINDOWS)
			HMODULE;
#elif defined(CP_PLATFORM_UNIX)
			void*;
#endif
		/// The type of the returned symbol.
		using symbol_t =
#if defined(CP_PLATFORM_WINDOWS)
			FARPROC;
#elif defined(CP_PLATFORM_UNIX)
			void*;
#endif

		const static native_handle_t empty_handle; ///< Empty handle.

		/// Default constructor.
		dynamic_library() = default;
		/// Move constructor.
		dynamic_library(dynamic_library &&src) noexcept : _handle(std::exchange(src._handle, empty_handle)) {
		}
		/// No copy construction.
		dynamic_library(const dynamic_library&) = delete;
		/// Move assignment.
		dynamic_library &operator=(dynamic_library &&src) noexcept {
			unload();
			_handle = std::exchange(src._handle, empty_handle);
			return *this;
		}
		/// No copy assignment.
		dynamic_library &operator=(const dynamic_library&) = delete;
		/// Calls \ref unload().
		~dynamic_library() {
			unload();
		}

		/// Tries to load a dynamic library.
		inline static std::optional<dynamic_library> load(const std::filesystem::path &path) {
			native_handle_t handle = _load_impl(path);
			if (handle == empty_handle) {
				return std::nullopt;
			}
			return dynamic_library(handle);
		}

		/// Unloads the current library if necessary.
		void unload() {
			if (!empty()) {
				_unload_impl(_handle);
				_handle = empty_handle;
			}
		}

		/// Finds the symbol, then casts it to the desired type and returns it.
		template <typename FuncPtr> FuncPtr find_symbol(const std::u8string &name) const {
			return reinterpret_cast<FuncPtr>(find_symbol_raw(name));
		}
		/// Finds and returns the symbol without checking if the handle is valid or casting the result.
		symbol_t find_symbol_raw(const std::u8string&) const;

		/// Returns the native handle.
		native_handle_t get_native_handle() const {
			return _handle;
		}

		/// Returns whether the handle is empty.
		bool empty() const {
			return _handle == empty_handle;
		}
	protected:
		native_handle_t _handle = empty_handle; ///< The handle.

		/// Initializes this dynamic library directly using the handle.
		explicit dynamic_library(native_handle_t h) : _handle(h) {
		}

		/// Loads the library at the given path. If the loading failed, this function should return
		/// \ref empty_handle.
		static native_handle_t _load_impl(const std::filesystem::path&);
		/// Unloads the given library. The library must not be empty.
		static void _unload_impl(native_handle_t);
	};
}
