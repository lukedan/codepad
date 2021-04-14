// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// A backend that starts an executable and communicates with it through its standard input and output.

#include <filesystem>

#include <Windows.h>

#include <codepad/os/filesystem.h>
#include <codepad/os/process.h>

#include "codepad/lsp/backend.h"

namespace codepad::lsp {
	/// A backend that starts an executable and communicates with it through its standard input and output.
	class stdio_backend : public backend {
	public:
		/// Starts the executable and sets up input/output.
		stdio_backend(const std::filesystem::path &exec, const std::vector<std::u8string_view> &args) {
			auto stdin_pipes = os::pipe::create();
			if (!stdin_pipes) {
				logger::get().log_error(CP_HERE) <<
					"failed to create pipe for stdin: " << stdin_pipes.error_code();
				return;
			}
			auto stdout_pipes = os::pipe::create();
			if (!stdout_pipes) {
				logger::get().log_error(CP_HERE) <<
					"failed to create pipe for stdout: " << stdout_pipes.error_code();
				return;
			}

			_stdin_write_pipe = std::move(stdin_pipes->write);
			_stdout_read_pipe = std::move(stdout_pipes->read);

			if (auto err = os::process::start_process(exec, args, stdin_pipes->read, stdout_pipes->write)) {
				logger::get().log_error(CP_HERE) << "failed to spawn server process: " << err;
			}
		}
	protected:
		os::file
			_stdin_write_pipe, ///< Pipe used to write to the standard input of the application.
			_stdout_read_pipe; ///< Pipe used to read from the standard output of the application.

		/// Writes the message to \ref _stdin_write_pipe.
		void _send_bytes(const void *data, std::size_t len) override {
			constexpr std::size_t _num_retries = 10;

			// since a single write may not successfully write all data, repeat until all data has been written
			// although we technically won't run into this case (yet)
			for (std::size_t i = 0; len > 0 && i < _num_retries; ++i) {
				auto res = _stdin_write_pipe.write(data, len);
				if (!res) {
					logger::get().log_error(CP_HERE) << "failed to send LSP message: " << res.error_code();
					return;
				}
				data = static_cast<const std::byte*>(data) + res.value();
				len -= res.value();
			}
			if (len > 0) {
				logger::get().log_error(CP_HERE) << "failed to send LSP message: too many attempts";
			}
		}
		/// Receives a message from \ref _stdout_read_pipe.
		std::size_t _receive_bytes(void *data, std::size_t len) override {
			auto res = _stdout_read_pipe.read(len, data);
			if (!res) {
				logger::get().log_error(CP_HERE) << "failed to receive LSP message: " << res.error_code();
				return 0;
			}
			return res.value();
		}
	};
}
