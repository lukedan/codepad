#pragma once

#include "../filesystem.h"
#include "window.h"

namespace codepad {
	namespace os {
		struct file {
		public:
			using native_handle_t = HANDLE;
			using size_type = unsigned long long;

			file() = default;
			file(const std::filesystem::path &path, access_rights acc, open_mode mode) :
				_handle(_open_impl(path, acc, mode)) {
			}
			file(const file&) = delete;
			file(file &&rhs) : _handle(rhs._handle) {
				rhs._handle = INVALID_HANDLE_VALUE;
			}
			file &operator=(const file&) = delete;
			file &operator=(file &&rhs) {
				std::swap(_handle, rhs._handle);
				return *this;
			}
			~file() {
				close();
			}

			void open(const std::filesystem::path &path, access_rights acc, open_mode mode) {
				close();
				_handle = _open_impl(path, acc, mode);
			}
			void close() {
				if (valid()) {
					winapi_check(CloseHandle(_handle));
					_handle = INVALID_HANDLE_VALUE;
				}
			}

			size_type get_size() const {
				if (valid()) {
					LARGE_INTEGER sz;
					winapi_check(GetFileSizeEx(_handle, &sz));
					return sz.QuadPart;
				}
				return 0;
			}

			native_handle_t get_native_handle() const {
				return _handle;
			}

			bool valid() const {
				return _handle != INVALID_HANDLE_VALUE;
			}
		protected:
			native_handle_t _handle = INVALID_HANDLE_VALUE;

			inline static native_handle_t _open_impl(
				const std::filesystem::path &path, access_rights acc, open_mode mode
			) {
				native_handle_t res = CreateFile(
					path.c_str(), _interpret_access_rights(acc),
					FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
					_interpret_open_mode(mode), FILE_ATTRIBUTE_NORMAL, nullptr
				);
				if (res == INVALID_HANDLE_VALUE) {
					logger::get().log_warning(CP_HERE, "CreateFile failed with error code ", GetLastError());
				}
				return res;
			}

			inline static DWORD _interpret_access_rights(access_rights acc) {
				return
					(test_bit_any(acc, access_rights::read) ? FILE_GENERIC_READ : 0) |
					(test_bit_any(acc, access_rights::write) ? FILE_GENERIC_WRITE : 0);
			}
			inline static DWORD _interpret_open_mode(open_mode mode) {
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
		};
		struct file_mapping {
		public:
			file_mapping() = default;
			file_mapping(const file &f, access_rights acc) {
				assert_true_usage(f.valid(), "cannot map an invalid file");
				_map_impl(f.get_native_handle(), acc);
			}
			file_mapping(const file_mapping&) = delete;
			file_mapping(file_mapping &&rhs) : _handle(rhs._handle), _ptr(rhs._ptr) {
				rhs._handle = nullptr;
				rhs._ptr = nullptr;
			}
			file_mapping &operator=(const file_mapping&) = delete;
			file_mapping &operator=(file_mapping &&rhs) {
				std::swap(_handle, rhs._handle);
				std::swap(_ptr, rhs._ptr);
				return *this;
			}
			~file_mapping() {
				unmap();
			}

			void map(const file &f, access_rights acc) {
				unmap();
				_map_impl(f.get_native_handle(), acc);
			}
			void unmap() {
				if (valid()) {
					winapi_check(UnmapViewOfFile(_ptr));
					winapi_check(CloseHandle(_handle));
					_ptr = nullptr;
					_handle = nullptr;
				}
			}

			// maybe smaller or bigger than the file
			size_t get_mapped_size() const {
				if (valid()) {
					MEMORY_BASIC_INFORMATION info;
					winapi_check(VirtualQuery(_ptr, &info, sizeof(info)));
					return info.RegionSize;
				}
				return 0;
			}

			void *get_mapped_pointer() const {
				return _ptr;
			}

			bool valid() const {
				return _ptr != nullptr;
			}
		protected:
			HANDLE _handle = nullptr;
			void *_ptr = nullptr;

			void _map_impl(file::native_handle_t file, access_rights acc) {
				_handle = CreateFileMapping(
					file, nullptr, acc == access_rights::read ? PAGE_READONLY : PAGE_READWRITE, 0, 0, nullptr
				);
				if (_handle != nullptr) {
					assert_true_usage(GetLastError() != ERROR_ALREADY_EXISTS, "cannot open multiple mappings to one file");
					_ptr = MapViewOfFile(_handle, acc == access_rights::read ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
					if (!_ptr) {
						logger::get().log_warning(CP_HERE, "MapViewOfFile failed with error code ", GetLastError());
						winapi_check(CloseHandle(_handle));
					}
				} else {
					logger::get().log_warning(CP_HERE, "CreateFileMapping failed with error code ", GetLastError());
				}
			}
		};
	}
}
