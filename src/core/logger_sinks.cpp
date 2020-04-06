// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "logger_sinks.h"

/// \file
/// Implementation of logger sinks.

namespace codepad::logger_sinks {
	void console_sink::on_message(
		const std::chrono::duration<double> &time, const code_position &pos, log_level level, std::u8string_view text
	) {
		_color(_colors.time);
		std::cout <<
			std::setiosflags(std::ios::fixed) << std::setw(static_cast<int>(_time_width)) <<
			std::setprecision(2) << time.count();

		std::size_t w = std::max(_get_console_width(), _time_width) - _time_width;

		// TODO use std::format instead
		std::stringstream ss;
		ss << pos.function << " @ " << pos.file << ":" << pos.line;
		std::string res = ss.str();
		_print_w(
			std::u8string_view(reinterpret_cast<const char8_t*>(res.data()), res.size()),
			_colors.code_position, _colors.time, w
		);

		std::u8string_view msg = u8" ";
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
		_print_left(_entry_color(level), msg);
		_print_w(text, _colors.message, _entry_color(level), w);
	}

	console_sink::color_scheme::entry console_sink::_entry_color(log_level level) {
		switch (level) {
		case log_level::debug:
			return _colors.debug_banner;
		case log_level::info:
			return _colors.info_banner;
		case log_level::warning:
			return _colors.warning_banner;
		case log_level::error:
			return _colors.error_banner;
		}
		return _colors.message;
	}

	void console_sink::_color_fg(color code) {
		std::cout << "\033[" << static_cast<std::size_t>(code) << "m";
	}

	void console_sink::_color_bg(color code) {
		std::cout << "\033[" << static_cast<std::size_t>(code) + 10 << "m\033[K";
	}

	void console_sink::_color(color_scheme::entry scheme) {
		_color_fg(scheme.foreground);
		_color_bg(scheme.background);
	}

	void console_sink::_color_reset() {
		std::cout << "\033[0m\033[K";
	}

	void console_sink::_print_left(color_scheme::entry scheme, std::u8string_view text) {
		_color(scheme);
		std::cout <<
			std::setw(static_cast<int>(_time_width)) <<
			std::string_view(reinterpret_cast<const char*>(text.data()), text.size());
	}

	void console_sink::_print_w(
		std::u8string_view msg, color_scheme::entry scheme, color_scheme::entry banner, std::size_t w
	) {
		_color(scheme);
		std::size_t cl = 0;
		for (auto it = msg.begin(); it != msg.end(); ++it) {
			if (*it == '\n' || cl == w) {
				std::cout << "\n";
				_print_left(banner);
				_color(scheme);
				cl = 0;
			}
			if (*it != '\n') {
				std::cout << static_cast<char>(*it);
				++cl;
			}
		}
		std::cout << "\n";
	}


	void file_sink::on_message(
		const std::chrono::duration<double> &time, const code_position &pos, log_level level, std::u8string_view text
	) {
		_fout <<
			std::setiosflags(std::ios::fixed) << std::setw(static_cast<int>(_time_width)) <<
			std::setprecision(2) << time.count() << "  " <<
			_get_level_label(level) << "  " << pos.file << " : " << pos.line << " @ " << pos.function << "\n";
		_fout << std::string_view(reinterpret_cast<const char*>(text.data()), text.size()) << "\n";
	}

	std::string_view file_sink::_get_level_label(log_level l) {
		switch (l) {
		case log_level::debug:
			return "D";
		case log_level::info:
			return "I";
		case log_level::warning:
			return "W";
		case log_level::error:
			return "E";
		}
		return "?";
	}
}
