// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic filesystem related enums and classes.

#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#elif defined(CP_PLATFORM_UNIX)
#	include <sys/types.h>
#endif

#include "../core/misc.h"
#include "../core/assert.h"

namespace codepad::os {
	/// Specifies what operations are to be performed on a file.
	enum class access_rights {
		zero = 0, ///< Zero value for comparisons.
		read = 1, ///< The file is to be read from only.
		write = 2, ///< The file is to be written to only.
		read_write = read | write ///< The file is to be read from and written to.
	};
	/// Specifies how a file is to be opened.
	enum class open_mode {
		zero = 0, ///< Zero value for comparisons.
		open = 1, ///< The file must exist.
		create = 2, ///< The file must not exist, and will be created.
		open_and_truncate = 4, ///< The file must exist, and will be truncated after it's opened.
		/// The file is created if it doesn't exist.
		open_or_create = open | create,
		/// The file is created if it doesn't exist, or truncated if it does.
		create_or_truncate = create | open_and_truncate
	};
	/// Specifies the starting point for moving the file pointer.
	enum class seek_mode {
		begin, ///< The beginning of the file.
		current, ///< The current position.
		end ///< The end of the file.
	};
}

namespace codepad {
	/// Enables bitwise operators for \ref os::access_rights.
	template <> struct enable_enum_bitwise_operators<os::access_rights> : public std::true_type {
	};
	/// Enables bitwise operators for \ref os::open_mode.
	template <> struct enable_enum_bitwise_operators<os::open_mode> : public std::true_type {
	};
}

namespace codepad::os {
	/// Represents an opened file.
	struct file {
	public:
		/// The type of native handles.
		using native_handle_t =
#if defined(CP_PLATFORM_WINDOWS)
			HANDLE;
#elif defined(CP_PLATFORM_UNIX)
			int;
#endif
		/// The type used to represent positions. This type is not necessarily unsigned.
		using pos_type =
#if defined(CP_PLATFORM_WINDOWS)
			LONGLONG;
#elif defined(CP_PLATFORM_UNIX)
			off_t;
#endif
		/// The type used to represent differences of \ref pos_type. This type is signed.
		using difference_type =
#if defined(CP_PLATFORM_WINDOWS)
			LONGLONG;
#endif

		/// The value of an empty handle.
		const static native_handle_t empty_handle;

		/// Initializes the \ref file to empty.
		file() = default;
		/// Opens the given file with the specified \ref access_rights and \ref open_mode.
		/// If the operation fails, the \ref file remains empty.
		file(const std::filesystem::path &path, access_rights acc, open_mode mode) :
			_handle(_open_impl(path, acc, mode)) {
		}
		/// Move constructor.
		file(file &&rhs) noexcept : _handle(rhs._handle) {
			rhs._handle = empty_handle;
		}
		/// No copy construction.
		file(const file&) = delete;
		/// Move assignment.
		file &operator=(file &&rhs) noexcept {
			std::swap(_handle, rhs._handle);
			return *this;
		}
		/// No copy assignment.
		file &operator=(const file&) = delete;
		/// Calls close() to close the file.
		~file() {
			close();
		}

		/// Opens the given file with the specified \ref access_rights and \ref open_mode.
		/// If there is a previously opened file, calls close() first.
		/// If the operation fails, the \ref file remains empty.
		void open(const std::filesystem::path &path, access_rights acc, open_mode mode) {
			close();
			_handle = _open_impl(path, acc, mode);
		}
		/// If there is a currently open file, closes it and resets the \ref file to empty.
		void close() {
			if (valid()) {
				_close_impl();
				_handle = empty_handle;
			}
		}

		/// Gets and returns the size of the opened file. Returns 0 if the file is empty.
		pos_type get_size() const {
			if (valid()) {
				return _get_size_impl();
			}
			return 0;
		}

		/// Reads a given amount of bytes into the given buffer.
		///
		/// \return The number of bytes read.
		pos_type read(pos_type, void*);
		/// Writes the given data to the file.
		void write(const void*, pos_type);

		/// Returns the position of the file pointer.
		pos_type tell() const;
		/// Moves the file pointer.
		pos_type seek(seek_mode, difference_type);

		/// Returns the native handle.
		native_handle_t get_native_handle() const {
			return _handle;
		}

		/// Returns whether the \ref file is non-empty.
		bool valid() const {
			return _handle != empty_handle;
		}
	protected:
		native_handle_t _handle = empty_handle; ///< The underlying native handle.

		/// Opens the given file with the specified \ref access_rights and \ref open_mode,
		/// and returns the resulting file handle. If the operation fails, returns \ref empty_handle.
		static native_handle_t _open_impl(const std::filesystem::path&, access_rights, open_mode);
		/// Closes the file. Assumes that it's valid.
		void _close_impl();
		/// Returns the size of the opened file. Assumes that it's valid.
		pos_type _get_size_impl() const;
	};

	/// Represents a memory-mapped file.
	struct file_mapping {
	public:
		/// Initializes the \ref file_mapping to empty.
		file_mapping() = default;
		/// Maps the given \ref file with the specified \ref access_rights.
		/// If the operation fails, the \ref file_mapping remains empty.
		file_mapping(const file &f, access_rights acc) {
			assert_true_usage(f.valid(), "cannot map an invalid file");
			_map_impl(f, acc);
		}
		/// Move constructor.
		file_mapping(file_mapping&&);
		/// No copy constructor.
		file_mapping(const file_mapping&) = delete;
		/// Move assignment.
		file_mapping &operator=(file_mapping&&);
		/// No copy assignment.
		file_mapping &operator=(const file_mapping&) = delete;
		/// Calls unmap() to unmap the file.
		~file_mapping() {
			unmap();
		}

		/// Maps the given \ref file with the specified \ref access_rights.
		/// If there is a previously mapped file, calls unmap() first.
		/// If the operation fails, the \ref file_mapping remains empty.
		void map(const file &f, access_rights acc) {
			unmap();
			_map_impl(f, acc);
		}
		/// If there is a mapped file, unmaps it and resets the \ref file_mapping to empty.
		void unmap() {
			if (valid()) {
				_unmap_impl();
				_ptr = nullptr;
			}
		}

		/// Returns the mapped size of the file. This could be larger than the file's actual size.
		///
		/// \todo Return rounded size.
		size_t get_mapped_size() const;

		/// Returns the pointer to the mapped memory region.
		void *get_mapped_pointer() const {
			return _ptr;
		}

		/// Returns whether the \ref file_mapping is non-empty.
		bool valid() const {
			return _ptr != nullptr;
		}
	protected:
		void *_ptr = nullptr; ///< Pointer to the mapped memory region.
#ifdef CP_PLATFORM_WINDOWS
		HANDLE _handle = nullptr; ///< The handle of the file mapping.
#elif defined(CP_PLATFORM_UNIX)
		size_t _len = 0; ///< The size of the mapped region in bytes.
#endif

		/// Maps the given \ref file with the specified \ref access_rights.
		/// If the operation fails, resets the \ref file_mapping to empty.
		void _map_impl(const file&, access_rights);
		/// Unmaps the file and resets corresponding fields. Assumes that the file's valid.
		void _unmap_impl();
	};
}
