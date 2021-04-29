// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/process.h"

/// \file
/// Implementation of process utilities for Windows.

#include "codepad/os/windows/misc.h"

namespace codepad::os {
	std::error_code process::start_process(
		const std::filesystem::path &app, const std::vector<std::u8string_view> &args,
		const file &stdin_redirect, const file &stdout_redirect, const file &stderr_redirect
	) {
		std::size_t num_inherited_handles = 0;

		STARTUPINFO start_info{};
		start_info.cb = sizeof(start_info);
		// the documentation doesn't say if using nullptr is ok, but it works fine
		if (!stdin_redirect.is_empty_handle()) {
			start_info.hStdInput = stdin_redirect.get_native_handle();
			if (!SetHandleInformation(start_info.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
				return _details::get_error_code();
			}
			++num_inherited_handles;
		}
		if (!stdout_redirect.is_empty_handle()) {
			start_info.hStdOutput = stdout_redirect.get_native_handle();
			if (!SetHandleInformation(start_info.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
				return _details::get_error_code();
			}
			++num_inherited_handles;
		}
		if (!stderr_redirect.is_empty_handle()) {
			start_info.hStdError = stderr_redirect.get_native_handle();
			if (!SetHandleInformation(start_info.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
				return _details::get_error_code();
			}
			++num_inherited_handles;
		}
		if (num_inherited_handles > 0) { // use redirected handles
			start_info.dwFlags |= STARTF_USESTDHANDLES;
		}

		// TODO only inherit specific handles using a LPPROC_THREAD_ATTRIBUTE_LIST

		std::wstring command_line;
		_details::quote_cmd_arg(app.u8string(), command_line);
		for (std::u8string_view arg : args) {
			command_line += L' ';
			_details::quote_cmd_arg(arg, command_line);
		}

		PROCESS_INFORMATION process_info{};
		auto res = CreateProcess(
			app.c_str(), command_line.data(), nullptr, nullptr,
			num_inherited_handles > 0, 0, nullptr, nullptr, &start_info, &process_info
		);
		if (!res) {
			return _details::get_error_code();
		}

		// we don't report these errors as they're less interesting
		if (!CloseHandle(process_info.hProcess)) {
			logger::get().log_error(CP_HERE) << "error while closing process handle";
		}
		if (!CloseHandle(process_info.hThread)) {
			logger::get().log_error(CP_HERE) << "error while closing thread handle";
		}
		return std::error_code();
	}

	process::id_t process::get_current_process_id() {
		return GetCurrentProcessId();
	}
}
