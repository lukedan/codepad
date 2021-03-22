// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Generic filesystem related enums and classes.

#include <optional>
#include <filesystem>

#ifdef CP_PLATFORM_WINDOWS
#	include <Windows.h>
#elif defined(CP_PLATFORM_UNIX)
#	include <sys/types.h>
#endif

#include "../core/misc.h"
#include "../core/assert.h"
#include "misc.h"

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
	struct file;
	struct pipe;

	/// Represents a memory-mapped file.
	struct file_mapping {
		friend file;
	public:
		/// Initializes the \ref file_mapping to empty.
		file_mapping() = default;
		/// Move constructor.
		file_mapping(file_mapping&&) noexcept;
		/// Move assignment.
		file_mapping &operator=(file_mapping&&);
		/// Calls unmap() to unmap the file.
		~file_mapping() {
			auto result = unmap();
			if (result) {
				logger::get().log_error(CP_HERE) << "file_mapping::unmap() failed with error: " << result;
			}
		}

		/// If there is a mapped file, unmaps it and resets the \ref file_mapping to empty.
		std::error_code unmap() {
			if (!is_empty_handle()) {
				std::error_code result = _unmap_impl();
				_ptr = nullptr;
				return result;
			}
			return std::error_code();
		}

		/// Returns the mapped size of the file. This could be larger than the file's actual size.
		///
		/// \todo Return rounded size.
		[[nodiscard]] result<std::size_t> get_mapped_size() const;

		/// Returns the pointer to the mapped memory region.
		[[nodiscard]] void *get_mapped_pointer() const {
			return _ptr;
		}

		/// Returns \p true if the \ref file_mapping does not refer a file.
		[[nodiscard]] bool is_empty_handle() const {
			return _ptr == nullptr;
		}
	protected:
		void *_ptr = nullptr; ///< Pointer to the mapped memory region.
#ifdef CP_PLATFORM_WINDOWS
		HANDLE _handle = nullptr; ///< The handle of the file mapping.
#elif defined(CP_PLATFORM_UNIX)
		std::size_t _len = 0; ///< The size of the mapped region in bytes.
#endif

		/// Unmaps the file and resets corresponding fields. Assumes that the file's valid.
		[[nodiscard]] std::error_code _unmap_impl();
	};

	/// Represents an opened file.
	struct file {
		friend pipe;
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
#elif defined(CP_PLATFORM_UNIX)
			off_t;
#endif

		/// The value of an empty handle.
		constexpr static native_handle_t empty_handle =
#if defined(CP_PLATFORM_WINDOWS)
			INVALID_HANDLE_VALUE;
#elif defined(CP_PLATFORM_UNIX)
			-1;
#endif

		/// Initializes the \ref file to empty.
		file() = default;
		/// Move constructor.
		file(file &&rhs) noexcept : _handle(std::exchange(rhs._handle, empty_handle)) {
		}
		/// Move assignment.
		file &operator=(file &&rhs) noexcept {
			close();
			_handle = std::exchange(rhs._handle, empty_handle);
			return *this;
		}
		/// Calls close() to close the file.
		~file() {
			auto result = close();
			if (result) {
				logger::get().log_error(CP_HERE) << "file::close() failed with error: " << result;
			}
		}

		/// Tries to open the given file with the specified \ref access_rights and \ref open_mode.
		[[nodiscard]] inline static result<file> open(
			const std::filesystem::path &path, access_rights acc, open_mode mode
		) {
			return _open_impl(path, acc, mode).into([](native_handle_t h) {
				return file(h);
			});
		}

		/// If there is a currently open file, closes it and resets the \ref file to empty.
		std::error_code close() {
			if (!is_empty_handle()) {
				auto result = _close_impl(_handle);
				_handle = empty_handle;
				return result;
			}
			return std::error_code();
		}

		/// Gets and returns the size of the opened file. Returns 0 if the file is empty.
		[[nodiscard]] result<pos_type> get_size() const {
			if (!is_empty_handle()) {
				return _get_size_impl();
			}
			return std::error_code();
		}

		/// Reads a given amount of bytes into the given buffer.
		///
		/// \return The number of bytes read.
		[[nodiscard]] result<pos_type> read(pos_type, void*);
		/// Writes the given data to the file.
		///
		/// \return The number of bytes written. The caller should check that the entire buffer has been successfully
		///         written.
		[[nodiscard]] result<pos_type> write(const void*, pos_type);

		/// Returns the position of the file pointer.
		[[nodiscard]] result<pos_type> tell() const;
		/// Moves the file pointer.
		///
		/// \return The position of the file pointer after this operation, relative to the beginning of the file.
		result<pos_type> seek(seek_mode, difference_type);

		/// Maps this \ref file with the specified \ref access_rights.
		[[nodiscard]] result<file_mapping> map(access_rights) const;

		/// Returns the native handle.
		[[nodiscard]] native_handle_t get_native_handle() const {
			return _handle;
		}

		/// Returns \p true if the \ref file does not refer a file.
		[[nodiscard]] bool is_empty_handle() const {
			return _handle == empty_handle;
		}
	protected:
		native_handle_t _handle = empty_handle; ///< The underlying native handle.

		/// Initializes this object directly using the handle.
		explicit file(native_handle_t h) : _handle(h) {
		}

		/// Opens the given file with the specified \ref access_rights and \ref open_mode,
		/// and returns the resulting file handle. If the operation fails, returns \ref empty_handle.
		[[nodiscard]] static result<native_handle_t> _open_impl(
			const std::filesystem::path&, access_rights, open_mode
		);
		/// Closes the given file. The handle must not be empty.
		[[nodiscard]] static std::error_code _close_impl(native_handle_t);
		/// Returns the size of the opened file. Assumes that it's valid.
		[[nodiscard]] result<pos_type> _get_size_impl() const;
	};


	/// Read and write ends of a pipe.
	struct pipe {
		file
			read, ///< The read end of the pipe.
			write; ///< The write end of the pipe.

		/// Creates a new pipe.
		[[nodiscard]] static result<pipe> create();
	};
}
