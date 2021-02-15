// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A backend that starts an executable and communicates with it through its standard input and output.

#include <filesystem>

#include <Windows.h>

#include "codepad/os/windows/misc.h"

#include "../backend.h"

namespace codepad::lsp {
	/// A backend that starts an executable and communicates with it through its standard input and output.
	class stdio_backend : public backend {
	public:
		/// Starts the executable and sets up input/output.
		explicit stdio_backend(std::wstring cmd) {
			SECURITY_ATTRIBUTES sec_attr;
			sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
			sec_attr.bInheritHandle = true;
			sec_attr.lpSecurityDescriptor = nullptr;

			HANDLE stdin_read;
			os::_details::winapi_check(CreatePipe(&stdin_read, &_stdin_write_pipe, &sec_attr, 0));
			os::_details::winapi_check(SetHandleInformation(_stdin_write_pipe, HANDLE_FLAG_INHERIT, 0));

			HANDLE stdout_write;
			os::_details::winapi_check(CreatePipe(&_stdout_read_pipe, &stdout_write, &sec_attr, 0));
			os::_details::winapi_check(SetHandleInformation(_stdout_read_pipe, HANDLE_FLAG_INHERIT, 0));

			HANDLE stderr_read, stderr_write;
			os::_details::winapi_check(CreatePipe(&stderr_read, &stderr_write, &sec_attr, 0));
			os::_details::winapi_check(SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0));

			STARTUPINFO start_info{};
			start_info.cb = sizeof(start_info);
			start_info.dwFlags = STARTF_USESTDHANDLES;
			start_info.hStdInput = stdin_read;
			start_info.hStdOutput = stdout_write;
			start_info.hStdError = stderr_write;
			PROCESS_INFORMATION process_info{};
			os::_details::winapi_check(CreateProcess(
				nullptr, cmd.data(), nullptr, nullptr, true, 0, nullptr, nullptr, &start_info, &process_info
			));

			os::_details::winapi_check(CloseHandle(process_info.hProcess));
			os::_details::winapi_check(CloseHandle(process_info.hThread));
			os::_details::winapi_check(CloseHandle(stdin_read));
			os::_details::winapi_check(CloseHandle(stdout_write));
			os::_details::winapi_check(CloseHandle(stderr_write));
		}
		/// Closes handles.
		~stdio_backend() {
			os::_details::winapi_check(CloseHandle(_stdin_write_pipe));
			os::_details::winapi_check(CloseHandle(_stdout_read_pipe));
		}

		/// Peeks \ref _stdout_read_pipe.
		bool check_message() override {
			DWORD num_bytes = 0;
			os::_details::winapi_check(PeekNamedPipe(_stdout_read_pipe, nullptr, 0, nullptr, &num_bytes, nullptr));
			return num_bytes > 0;
		}
	protected:
		HANDLE
			_stdin_write_pipe = nullptr, ///< Pipe used to write to the standard input of the application.
			_stdout_read_pipe = nullptr; ///< Pipe used to read from the standard output of the application.

		/// Writes the message to \ref _stdin_write_pipe.
		void _send_bytes(const void *data, std::size_t len) override {
			DWORD length = static_cast<DWORD>(len), bytes_written = 0;
			os::_details::winapi_check(WriteFile(
				_stdin_write_pipe, data, length, &bytes_written, nullptr
			));
			assert_true_sys(bytes_written == length, "incomplete write");
		}
		/// Receives a message from \ref _stdout_read_pipe.
		std::size_t _receive_bytes(void *data, std::size_t len) override {
			DWORD bytes_read = 0;
			os::_details::winapi_check(ReadFile(
				_stdout_read_pipe, data, static_cast<DWORD>(len), &bytes_read, nullptr
			));
			return bytes_read;
		}
	};
}
