// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Logging sink that outputs the log to the console.

#include <iostream>
#include <chrono>
#include <iomanip>

#include "logging.h"

/// Common sinks used by the logger.
namespace codepad::logger_sinks {
	/// A sink that prints all logging information to the console.
	struct console_sink {
	public:
		/// Initializes \ref _creation.
		console_sink() : _creation(std::chrono::high_resolution_clock::now()) {
		}

		/// Prints the logging message.
		void operator()(const code_position pos, log_level level, str_view_t text) {
			auto secs = std::chrono::duration<double>(
				std::chrono::high_resolution_clock::now() - _creation
				).count();
			_color_bg(37); // white
			_color_fg(30); // black
			std::cout <<
				std::setiosflags(std::ios::fixed) << std::setw(_time_width) << std::setprecision(2) << secs;

			size_t w = std::max(_get_console_width(), _time_width) - _time_width;

			std::stringstream ss;
			ss << pos.function << " @ " << pos.file << ":" << pos.line;
			_print_w(ss.str(), 30, 36, 30, 37, level, w); // black & cyan

			str_view_t msg = " ";
			switch (level) {
			case log_level::debug:
				msg = u8"DEBUG";
				break;
			case log_level::info:
				msg = u8"INFO";
				break;
			case log_level::warning:
				msg = u8"WARNING";
				break;
			case log_level::error:
				msg = u8"ERROR";
				break;
			}
			_print_left(level, 37, _entry_color(level), msg);
			_print_w(text, 37, 30, 37, _entry_color(level), level, w); // white & black
		}
	protected:
		std::chrono::high_resolution_clock::time_point _creation; ///< The time when this sink is created.
		size_t _time_width = 8; ///< The width of displayed time.

		/// Returns the color that correspond to the given \ref log_level.
		inline static size_t _entry_color(log_level level) {
			switch (level) {
			case log_level::debug:
				return 34;
			case log_level::info:
				return 32;
			case log_level::warning:
				return 33;
			case log_level::error:
				return 31;
			}
			return 30;
		}

		/// Changes the foreground color.
		void _color_fg(size_t code) {
			std::cout << "\033[" << code << "m";
		}
		/// Changes the background color.
		void _color_bg(size_t code) {
			std::cout << "\033[" << code + 10 << "m\033[K";
		}
		/// Resets terminal colors.
		void _color_reset() {
			std::cout << "\033[0m\033[K";
		}

		/// Prints the left border.
		void _print_left(log_level lvl, size_t fg, size_t bg, str_view_t text = " ") {
			_color_bg(bg);
			_color_fg(fg);
			std::cout << std::setw(_time_width) << text;
		}
		/// Prints the message with a fixed width.
		void _print_w(str_view_t msg, size_t fg, size_t bg, size_t lfg, size_t lbg, log_level lvl, size_t w) {
			_color_bg(bg);
			_color_fg(fg);
			while (msg.length() > 0 && msg.back() == '\n') {
				msg.remove_suffix(1);
			}
			size_t cl = 0;
			for (auto it = msg.begin(); it != msg.end(); ++it) {
				if (*it == '\n' || cl == w) {
					std::cout << "\n";
					_print_left(lvl, lfg, lbg);
					_color_bg(bg);
					_color_fg(fg);
					cl = 0;
				}
				if (*it != '\n') {
					std::cout << *it;
					++cl;
				}
			}
			std::cout << "\n";
		}

		/// Returns the width of the console window. This function is platform-specific.
		size_t _get_console_width() const;
	};
}
