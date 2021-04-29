// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/filesystem.h"

/// \file
/// Filesystem implementation for the linux platform.

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "codepad/core/misc.h"
#include "codepad/os/linux/misc.h"

namespace codepad::os {
	/// Transforms the given \ref access_rights into flags used by \p open().
	inline int _interpret_access_rights(access_rights acc) {
		switch (acc) {
		case access_rights::read:
			return O_RDONLY;
		case access_rights::write:
			return O_WRONLY;
		case access_rights::read_write:
			return O_RDWR;
		default:
			assert_true_logical(false, "invalid access_rights");
			return 0;
		}
	}

	/// Transforms the given \ref open_mode into flags used by \p open().
	inline int _interpret_open_mode(open_mode mode) {
		int openflags =
			((mode & open_mode::create) != open_mode::zero ? O_CREAT : 0) |
			((mode & open_mode::open_and_truncate) != open_mode::zero ? O_TRUNC : 0);
		if (mode == open_mode::create) {
			openflags |= O_EXCL;
		}
		return openflags;
	}

	/// Transforms the given \ref seek_mode into flags used by \p lseek().
	inline int _interpret_seek_mode(seek_mode mode) {
		switch (mode) {
		case seek_mode::begin:
			return SEEK_SET;
		case seek_mode::current:
			return SEEK_CUR;
		case seek_mode::end:
			return SEEK_END;
		}
		assert_true_logical(false, "invalid seek mode");
		return SEEK_CUR;
	}


	file_mapping::file_mapping(file_mapping &&rhs) noexcept :
		_ptr(std::exchange(rhs._ptr, nullptr)), _len(std::exchange(rhs._len, 0)) {
	}

	file_mapping &file_mapping::operator=(file_mapping &&rhs) {
		unmap();
		_ptr = std::exchange(rhs._ptr, nullptr);
		_len = std::exchange(rhs._len, 0);
		return *this;
	}

	/// \todo Return rounded size.
	result<std::size_t> file_mapping::get_mapped_size() const {
		return _len;
	}

	std::error_code file_mapping::_unmap_impl() {
		bool fail = munmap(_ptr, _len) != 0;
		_len = 0;
		if (fail) {
			return _details::get_error_code_errno();
		}
		return std::error_code();
	}


	result<file::pos_type> file::read(file::pos_type sz, void *buf) {
		ssize_t res = ::read(_handle, buf, static_cast<size_t>(sz));
		if (res < 0) {
			return _details::get_error_code_errno();
		}
		return static_cast<pos_type>(res);
	}

	result<file::pos_type> file::write(const void *data, pos_type sz) {
		ssize_t res = ::write(_handle, data, static_cast<size_t>(sz));
		if (res < 0) {
			return _details::get_error_code_errno();
		}
		return static_cast<pos_type>(sz);
	}

	result<file::pos_type> file::tell() const {
		off_t pos = lseek(_handle, 0, SEEK_CUR);
		if (pos == static_cast<off_t>(-1)) {
			return _details::get_error_code_errno();
		}
		return static_cast<pos_type>(pos);
	}

	result<file::pos_type> file::seek(seek_mode mode, difference_type offset) {
		off_t pos = lseek(_handle, offset, _interpret_seek_mode(mode));
		if (pos == static_cast<off_t>(-1)) {
			return _details::get_error_code_errno();
		}
		return static_cast<pos_type>(pos);
	}

	result<file_mapping> file::map(access_rights acc) const {
		auto size = get_size();
		if (!size) {
			return size.error_code();
		}
		auto len = static_cast<std::size_t>(size.value());
		void *ptr = mmap(
			nullptr, len,
			((acc & access_rights::read) != access_rights::zero ? PROT_READ : 0) |
				((acc & access_rights::write) != access_rights::zero ? PROT_WRITE : 0),
			MAP_SHARED, get_native_handle(), 0
		);
		if (ptr == MAP_FAILED) {
			return _details::get_error_code_errno();
		}
		file_mapping result;
		result._len = len;
		result._ptr = ptr;
		return result;
	}

	result<file::native_handle_t> file::_open_impl(
		const std::filesystem::path &path, access_rights acc, open_mode mode
	) {
		if (mode == open_mode::create) { // file mustn't exist
			if (::access(path.c_str(), F_OK) == 0) { // file exists
				return _details::get_error_code_errno_custom(EEXIST);
			} else {
				if (errno != ENOENT) { // some other error happened
					return _details::get_error_code_errno();
				}
			}
		}
		native_handle_t res = ::open(
			path.c_str(), _interpret_access_rights(acc) | _interpret_open_mode(mode)
		);
		if (res < 0) {
			return _details::get_error_code_errno();
		}
		return res;
	}

	std::error_code file::_close_impl(native_handle_t handle) {
		if (::close(handle) != 0) {
			return _details::get_error_code_errno();
		}
		return std::error_code();
	}

	result<file::pos_type> file::_get_size_impl() const {
		struct stat filestat{};
		int result = fstat(_handle, &filestat);
		if (result != 0) {
			return _details::get_error_code_errno();
		}
		return static_cast<pos_type>(filestat.st_size);
	}


	result<pipe> pipe::create() {
		int pipefd[2];
		int error = ::pipe(pipefd);
		if (error != 0) {
			return _details::get_error_code_errno();
		}
		pipe result;
		result.read._handle = pipefd[0];
		result.write._handle = pipefd[1];
		return result;
	}
}
