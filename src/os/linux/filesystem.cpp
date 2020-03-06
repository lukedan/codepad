// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "../filesystem.h"

/// \file
/// Filesystem implementation for the linux platform.

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../core/misc.h"

using namespace std;

namespace codepad::os {
	const file::native_handle_t file::empty_handle = -1;

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


	file::native_handle_t file::_open_impl(
		const filesystem::path &path, access_rights acc, open_mode mode
	) {
		if ((mode & open_mode::create) == open_mode::zero) { // file mustn't exist
			if (::access(path.c_str(), F_OK) == 0) { // file exists
				return empty_handle;
			}
		}
		native_handle_t res = ::open(
			path.c_str(), _interpret_access_rights(acc) | _interpret_open_mode(mode)
		);
		if (res < 0) {
			logger::get().log_warning(CP_HERE) << "open failed with error code " << errno;
			res = empty_handle;
		}
		return res;
	}

	void file::_close_impl(native_handle_t handle) {
		assert_true_sys(::close(handle) == 0, "failed to close the file");
	}

	file::pos_type file::_get_size_impl() const {
		struct stat filestat{};
		assert_true_sys(fstat(_handle, &filestat) == 0, "unable to get file size");
		return filestat.st_size;
	}

	file::pos_type file::read(file::pos_type sz, void *buf) {
		ssize_t res = ::read(_handle, buf, static_cast<size_t>(sz));
		assert_true_sys(res >= 0, "failed to read from file");
		return static_cast<pos_type>(res);
	}

	void file::write(const void *data, pos_type sz) {
		ssize_t res = ::write(_handle, data, static_cast<size_t>(sz));
		assert_true_sys(res == static_cast<ssize_t>(sz), "failed to write all data");
	}

	file::pos_type file::tell() const {
		pos_type pos = lseek(_handle, 0, SEEK_CUR);
		assert_true_sys(pos >= 0, "lseek() failed");
		return pos;
	}

	file::pos_type file::seek(seek_mode mode, difference_type offset) {
		pos_type pos = lseek(_handle, offset, _interpret_seek_mode(mode));
		assert_true_sys(pos >= 0, "lseek() failed");
		return pos;
	}


	file_mapping::file_mapping(file_mapping &&rhs) : _ptr(rhs._ptr), _len(rhs._len) {
		rhs._ptr = nullptr;
		rhs._len = 0;
	}

	file_mapping &file_mapping::operator=(file_mapping &&rhs) {
		std::swap(_ptr, rhs._ptr);
		std::swap(_len, rhs._len);
		return *this;
	}

	/// \todo Return rounded size.
	std::size_t file_mapping::get_mapped_size() const {
		return _len;
	}

	void file_mapping::_map_impl(const file &f, access_rights acc) {
		_len = static_cast<std::size_t>(f.get_size());
		_ptr = mmap(
			nullptr, _len,
			((acc & access_rights::read) != access_rights::zero ? PROT_READ : 0) |
			((acc & access_rights::write) != access_rights::zero ? PROT_WRITE : 0),
			MAP_SHARED, f.get_native_handle(), 0
		);
		if (_ptr == MAP_FAILED) {
			logger::get().log_warning(CP_HERE) << "mmap failed with error code " << errno;
			_ptr = nullptr;
		}
	}

	void file_mapping::_unmap_impl() {
		assert_true_sys(munmap(_ptr, _len) == 0, "cannot unmap the file");
		_len = 0;
	}
}
