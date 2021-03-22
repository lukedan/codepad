// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Wrappers for types in tree-sitter.

#include <tree_sitter/api.h>

namespace codepad::tree_sitter {
	/// A basic wrapper around a pointer that prohibits copying.
	template <typename T, auto Dtor> struct pointer_wrapper {
	public:
		/// Default constructor.
		pointer_wrapper() = default;
		/// Directly initializes this pointer.
		explicit pointer_wrapper(T *ptr) : _ptr(ptr) {
		}
		/// Move construction.
		pointer_wrapper(pointer_wrapper &&src) : _ptr(std::exchange(src._ptr, nullptr)) {
		}
		/// No copy construction.
		pointer_wrapper(const pointer_wrapper&) = delete;
		/// Move assignment.
		pointer_wrapper &operator=(pointer_wrapper &&src) {
			_delete();
			_ptr = std::exchange(src._ptr, nullptr);
			return *this;
		}
		/// No copy assignment.
		pointer_wrapper &operator=(const pointer_wrapper&) = delete;
		/// Destructor.
		~pointer_wrapper() {
			_delete();
		}

		/// If \ref _ptr is non-null, frees it and resets it to \p nullptr.
		void reset() {
			_delete();
			_ptr = nullptr;
		}

		/// Returns the underlying pointer.
		T *get() const {
			return _ptr;
		}
		void set(T *ptr) {
			_delete();
			_ptr = ptr;
		}

		/// Returns \p true if \ref _ptr is \p nullptr.
		bool empty() const {
			return _ptr == nullptr;
		}
		/// Used to determine if this is empty.
		[[nodiscard]] explicit operator bool() const {
			return !empty();
		}
	private:
		T *_ptr = nullptr; ///< The underlying pointer.

		/// Deletes \ref _ptr without resetting it to \p nullptr.
		void _delete() {
			if (_ptr) {
				Dtor(_ptr);
			}
		}
	};

	using tree_ptr = pointer_wrapper<TSTree, ts_tree_delete>; ///< Wrapper for \p TSTree.
	using parser_ptr = pointer_wrapper<TSParser, ts_parser_delete>; ///< Wrapper for \p TSParser.
	using query_ptr = pointer_wrapper<TSQuery, ts_query_delete>; ///< Wrapper for \p TSQuery.
	/// Wrapper for \p TSQueryCursor.
	using query_cursor_ptr = pointer_wrapper<TSQueryCursor, ts_query_cursor_delete>;
}
