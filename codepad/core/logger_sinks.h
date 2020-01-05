// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Logging sink that outputs the log to the console.

#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

#include "logging.h"

/// Common sinks used by the logger.
namespace codepad::logger_sinks {
	/// A sink that prints all logging information to the console.
	struct console_sink : public log_sink {
	public:
		/// The color of text and background.
		enum class color : unsigned char {
			black = 30, ///< Black.
			red = 31, ///< Red.
			green = 32, ///< Green.
			yellow = 33, ///< Yellow.
			blue = 34, ///< Blue.
			magenta = 35, ///< Magenta.
			cyan = 36, ///< Cyan.
			white = 37 ///< White.
		};
		/// The color scheme for logging.
		struct color_scheme {
			/// Includes foreground and background colors.
			struct entry {
				/// Default constructor.
				entry() = default;
				/// Initializes all fields of this struct.
				entry(color fg, color bg) : foreground(fg), background(bg) {
				}

				color
					foreground = color::white, ///< Foreground color.
					background = color::black; ///< Background color.
			};

			entry
				time{color::black, color::cyan}, ///< Color for time display.
				code_position{color::black, color::white}, ///< Color for position display.
				message{color::white, color::black}, ///< Color for message display.

				debug_banner{color::white, color::blue}, ///< Color for the banner of debug messages.
				info_banner{color::black, color::green}, ///< Color for the banner of info messages.
				warning_banner{color::black, color::yellow}, ///< Color for the banner of warning messages.
				error_banner{color::white, color::red}; ///< Color for the banner of error messages.
		};

		/// Default constructor.
		console_sink() = default;
		/// Constructs a \ref console_sink with the given settings.
		console_sink(color_scheme scheme, std::size_t time_w) : _colors(scheme), _time_width(time_w) {
		}

		/// Prints the logging message.
		void on_message(
			const std::chrono::duration<double> &time, const code_position &pos, log_level level, str_view_t text
		) override {
			_color(_colors.time);
			std::cout <<
				std::setiosflags(std::ios::fixed) << std::setw(static_cast<int>(_time_width)) <<
				std::setprecision(2) << time.count();

			std::size_t w = std::max(_get_console_width(), _time_width) - _time_width;

			std::stringstream ss;
			ss << pos.function << " @ " << pos.file << ":" << pos.line;
			_print_w(ss.str(), _colors.code_position, _colors.time, w);

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
			_print_left(_entry_color(level), msg);
			_print_w(text, _colors.message, _entry_color(level), w);
		}

		/// Returns the current color scheme.
		color_scheme &output_color_scheme() {
			return _colors;
		}
		/// \overload
		const color_scheme &output_color_scheme() const {
			return _colors;
		}

		/// Returns the width of displayed time.
		std::size_t &time_display_width() {
			return _time_width;
		}
		/// \overload
		std::size_t time_display_width() const {
			return _time_width;
		}
	protected:
		color_scheme _colors; ///< The color scheme.
		std::size_t _time_width = 8; ///< The width of displayed time.

		/// Returns the color that correspond to the given \ref log_level.
		color_scheme::entry _entry_color(log_level level) {
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

		/// Changes the foreground color.
		void _color_fg(color code) {
			std::cout << "\033[" << static_cast<std::size_t>(code) << "m";
		}
		/// Changes the background color.
		void _color_bg(color code) {
			std::cout << "\033[" << static_cast<std::size_t>(code) + 10 << "m\033[K";
		}
		/// Changes output color by calling \ref _color_fg() and \ref _color_bg().
		void _color(color_scheme::entry scheme) {
			_color_fg(scheme.foreground);
			_color_bg(scheme.background);
		}
		/// Resets terminal colors.
		void _color_reset() {
			std::cout << "\033[0m\033[K";
		}

		/// Prints the left border.
		void _print_left(color_scheme::entry scheme, str_view_t text = " ") {
			_color(scheme);
			std::cout << std::setw(static_cast<int>(_time_width)) << text;
		}
		/// Prints the message with a fixed width.
		void _print_w(str_view_t msg, color_scheme::entry scheme, color_scheme::entry banner, std::size_t w) {
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
					std::cout << *it;
					++cl;
				}
			}
			std::cout << "\n";
		}

		/// Returns the width of the console window. This function is platform-specific.
		static std::size_t _get_console_width();
	};

	/// A sink that writes logging information to a file.
	struct file_sink : public log_sink {
	public:
		/// Initializes the file stream.
		explicit file_sink(const std::filesystem::path &path) : _fout(path, std::ios::app) {
		}

		/// Writes the log message to the file.
		void on_message(
			const std::chrono::duration<double> &time, const code_position &pos, log_level level, str_view_t text
		) override {
			_fout <<
				std::setiosflags(std::ios::fixed) << std::setw(static_cast<int>(_time_width)) <<
				std::setprecision(2) << time.count() << "  " <<
				_get_level_label(level) << "  " << pos.file << " : " << pos.line << " @ " << pos.function << "\n";
			_fout << text << "\n";
		}

		/// Returns a reference to the time output width.
		std::size_t &time_output_width() {
			return _time_width;
		}
		/// \overload
		std::size_t time_output_width() const {
			return _time_width;
		}
	protected:
		std::ofstream _fout; ///< The output stream.
		std::size_t _time_width = 12; ///< The width of times.

		/// Returns the text label that corresponds to the given \ref log_level.
		inline static str_view_t _get_level_label(log_level l) {
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
	};
}
