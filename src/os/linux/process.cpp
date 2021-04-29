// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/os/process.h"

/// \file
/// Implementation of process-related functions for Linux.

#include "codepad/os/linux/misc.h"

namespace codepad::os {
	std::error_code process::start_process(
		const std::filesystem::path &executable, const std::vector<std::u8string_view> &args,
		const file &stdin_redirect, const file &stdout_redirect, const file &stderr_redirect
	) {
		pid_t pid = ::fork();
		if (pid == -1) {
			return _details::get_error_code_errno();
		}
		if (pid == 0) {
			auto error = [&](const char *action) {
				printf("process::start_process(%s): %s", executable.c_str(), action);
				std::abort();
			};
			if (!stdin_redirect.is_empty_handle()) {
				if (::dup2(stdin_redirect.get_native_handle(), STDIN_FILENO) == -1) {
					error("failed to redirect stdin");
				}
			}
			if (!stdout_redirect.is_empty_handle()) {
				if (::dup2(stdout_redirect.get_native_handle(), STDOUT_FILENO) == -1) {
					error("failed to redirect stdout");
				}
			}
			if (!stderr_redirect.is_empty_handle()) {
				if (::dup2(stderr_redirect.get_native_handle(), STDERR_FILENO) == -1) {
					error("failed to redirect stderr");
				}
			}
			// first copy filename and arguments to a piece of memory we can modify
			std::vector<std::string> mod_args(args.size() + 1);
			mod_args[0] = executable.filename().string();
			for (std::size_t i = 0; i < args.size(); ++i) {
				mod_args[i + 1] = std::string(reinterpret_cast<const char*>(args[i].data()), args[i].size());
			}
			// then collect the pointers of these arguments
			std::vector<char*> argv(args.size() + 2);
			for (std::size_t i = 0; i < mod_args.size(); ++i) {
				argv[i] = mod_args[i].data();
			}
			argv.back() = nullptr;
			execvp(executable.c_str(), argv.data());
			error("execvp() failed");
		}
		return std::error_code();
	}

	process::id_t process::get_current_process_id() {
		return getpid();
	}
}
