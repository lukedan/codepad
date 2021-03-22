// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/filesystem.h"

/// \file
/// Filesystem implementation for the windows platform.

#include "codepad/os/windows/misc.h"

namespace codepad::os {
	/// Transforms the given \ref access_rights into flags used by \p CreateFile().
	inline DWORD _interpret_access_rights(access_rights acc) {
		return
			((acc & access_rights::read) != access_rights::zero ? FILE_GENERIC_READ : 0) |
			((acc & access_rights::write) != access_rights::zero ? FILE_GENERIC_WRITE : 0);
	}

	/// Transforms the given \ref open_mode into flags used by \p CreateFile().
	inline DWORD _interpret_open_mode(open_mode mode) {
		switch (mode) {
		case open_mode::create:
			return CREATE_NEW;
		case open_mode::create_or_truncate:
			return CREATE_ALWAYS;
		case open_mode::open:
			return OPEN_EXISTING;
		case open_mode::open_and_truncate:
			return TRUNCATE_EXISTING;
		case open_mode::open_or_create:
			return OPEN_ALWAYS;
		}
		assert_true_usage(false, "invalid open mode");
		return OPEN_EXISTING;
	}

	/// Translates the given \ref seek_mode into a \p FILE_ flag.
	inline DWORD _interpret_seek_mode(seek_mode mode) {
		switch (mode) {
		case seek_mode::begin:
			return FILE_BEGIN;
		case seek_mode::current:
			return FILE_CURRENT;
		case seek_mode::end:
			return FILE_END;
		}
		assert_true_usage(false, "invalid seek mode");
		return FILE_CURRENT;
	}


	file_mapping::file_mapping(file_mapping &&rhs) noexcept :
		_ptr(std::exchange(rhs._ptr, nullptr)), _handle(std::exchange(rhs._handle, nullptr)) {
	}

	file_mapping &file_mapping::operator=(file_mapping &&rhs) {
		unmap();
		_ptr = std::exchange(rhs._ptr, nullptr);
		_handle = std::exchange(rhs._handle, nullptr);
		return *this;
	}

	std::error_code file_mapping::_unmap_impl() {
		HANDLE h = _handle;
		_handle = nullptr;
		if (UnmapViewOfFile(_ptr)) {
			if (CloseHandle(h)) {
				return std::error_code();
			}
		}
		return _details::get_error_code();
	}

	result<std::size_t> file_mapping::get_mapped_size() const {
		if (!is_empty_handle()) {
			MEMORY_BASIC_INFORMATION info;
			if (!VirtualQuery(_ptr, &info, sizeof(info))) {
				return _details::make_error_result<std::size_t>();
			}
			return info.RegionSize;
		}
		return std::error_code();
	}


	result<file::native_handle_t> file::_open_impl(
		const std::filesystem::path &path, access_rights acc, open_mode mode
	) {
		native_handle_t res = CreateFile(
			path.c_str(), _interpret_access_rights(acc),
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			_interpret_open_mode(mode), FILE_ATTRIBUTE_NORMAL, nullptr
		);
		if (res == INVALID_HANDLE_VALUE) {
			return _details::make_error_result<file::native_handle_t>();
		}
		return res;
	}

	std::error_code file::_close_impl(native_handle_t handle) {
		return _details::check_and_return_error_code(CloseHandle(handle));
	}

	result<file::pos_type> file::_get_size_impl() const {
		LARGE_INTEGER sz;
		if (!GetFileSizeEx(_handle, &sz)) {
			return _details::make_error_result<pos_type>();
		}
		return sz.QuadPart;
	}

	result<file::pos_type> file::read(file::pos_type sz, void *buf) {
		assert_true_sys(sz <= std::numeric_limits<DWORD>::max(), "too many bytes to read");
		DWORD res = 0;
		if (!ReadFile(_handle, buf, static_cast<DWORD>(sz), &res, nullptr)) {
			return _details::make_error_result<pos_type>();
		}
		return static_cast<file::pos_type>(res);
	}

	result<file::pos_type> file::write(const void *data, pos_type sz) {
		assert_true_sys(sz <= std::numeric_limits<DWORD>::max(), "too many bytes to write");
		DWORD written; // no need to init
		// written != sz may happen "when writing to a non-blocking, byte-mode pipe handle with
		// insufficient buffer space"
		if (!WriteFile(_handle, data, static_cast<DWORD>(sz), &written, nullptr)) {
			return _details::make_error_result<pos_type>();
		}
		return written;
	}

	result<file::pos_type> file::tell() const {
		LARGE_INTEGER offset, res;
		offset.QuadPart = 0;
		if (!SetFilePointerEx(_handle, offset, &res, FILE_CURRENT)) {
			return _details::make_error_result<pos_type>();
		}
		return res.QuadPart;
	}

	result<file::pos_type> file::seek(seek_mode mode, file::difference_type diff) {
		LARGE_INTEGER offset, res;
		offset.QuadPart = diff;
		if (!SetFilePointerEx(_handle, offset, &res, _interpret_seek_mode(mode))) {
			return _details::make_error_result<pos_type>();
		}
		return res.QuadPart;
	}

	result<file_mapping> file::map(access_rights acc) const {
		HANDLE handle = CreateFileMapping(
			get_native_handle(), nullptr,
			acc == access_rights::read ? PAGE_READONLY : PAGE_READWRITE,
			0, 0, nullptr
		);
		if (handle == nullptr) {
			return _details::make_error_result<file_mapping>();
		}
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			return std::error_code(ERROR_ALREADY_EXISTS, std::system_category());
		}
		void *ptr = MapViewOfFile(_handle, acc == access_rights::read ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
		if (!ptr) {
			CloseHandle(_handle); // no point checking the result
			return _details::make_error_result<file_mapping>();
		}
		file_mapping result;
		result._handle = handle;
		result._ptr = ptr;
		return std::move(result);
	}


	result<pipe> pipe::create() {
		HANDLE read, write;
		if (!CreatePipe(&read, &write, nullptr, 0)) {
			return _details::make_error_result<pipe>();
		}

		pipe result;
		result.read = file(read);
		result.write = file(write);
		return std::move(result);
	}
}
