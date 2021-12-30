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
			white = 37, ///< White.

			bright_black = 90, ///< Bright black.
			bright_red = 91, ///< Bright red.
			bright_green = 92, ///< Bright green.
			bright_yellow = 93, ///< Bright yellow.
			bright_blue = 94, ///< Bright blue.
			bright_magenta = 95, ///< Bright magenta.
			bright_cyan = 96, ///< Bright cyan.
			bright_white = 97 ///< Bright white.
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
				time{ color::black, color::white }, ///< Color for time display.
				code_position{ color::bright_black, color::black }, ///< Color for position display.
				message{ color::white, color::black }, ///< Color for message display.

				debug_banner{ color::bright_white, color::bright_black }, ///< Color for the banner of debug messages.
				info_banner{ color::bright_white, color::bright_blue }, ///< Color for the banner of info messages.
				warning_banner{ color::black, color::bright_yellow }, ///< Color for the banner of warning messages.
				error_banner{ color::black, color::red }; ///< Color for the banner of error messages.
		};

		/// Default constructor.
		console_sink() = default;
		/// Constructs a \ref console_sink with the given settings.
		console_sink(color_scheme scheme, std::size_t time_w) : _colors(scheme), _time_width(time_w) {
		}

		/// Prints the logging message.
		void on_message(
			const std::chrono::duration<double>&, const std::source_location&, log_level, std::u8string_view
		) override;

		/// Returns the current color scheme.
		[[nodiscard]] color_scheme &output_color_scheme() {
			return _colors;
		}
		/// \overload
		[[nodiscard]] const color_scheme &output_color_scheme() const {
			return _colors;
		}

		/// Returns the width of displayed time.
		[[nodiscard]] std::size_t &time_display_width() {
			return _time_width;
		}
		/// \overload
		[[nodiscard]] std::size_t time_display_width() const {
			return _time_width;
		}
	protected:
		color_scheme _colors; ///< The color scheme.
		std::size_t _time_width = 8; ///< The width of displayed time.
		bool _is_printing = false; ///< Whether we're currently in \ref on_message().

		/// Returns the color that correspond to the given \ref log_level.
		[[nodiscard]] color_scheme::entry _entry_color(log_level) const;

		/// Changes the foreground color.
		static void _color_fg(color);
		/// Changes the background color.
		static void _color_bg(color);
		/// Changes output color by calling \ref _color_fg() and \ref _color_bg().
		static void _color(color_scheme::entry);
		/// Resets terminal colors.
		static void _color_reset();

		/// Prints the left border.
		void _print_left(color_scheme::entry, std::u8string_view = u8" ") const;
		/// Prints the message with a fixed width.
		void _print_w(std::u8string_view, color_scheme::entry scheme, color_scheme::entry banner, std::size_t) const;
	};

	/// A sink that writes logging information to a file.
	struct file_sink : public log_sink {
	public:
		/// Initializes the file stream.
		explicit file_sink(const std::filesystem::path &path) : _fout(path, std::ios::app) {
		}

		/// Writes the log message to the file.
		void on_message(
			const std::chrono::duration<double>&, const std::source_location&, log_level, std::u8string_view
		) override;

		/// Returns a reference to the time output width.
		std::size_t &time_output_width() {
			return _time_width;
		}
		/// \overload
		[[nodiscard]] std::size_t time_output_width() const {
			return _time_width;
		}
	protected:
		std::ofstream _fout; ///< The output stream.
		std::size_t _time_width = 12; ///< The width of times.

		/// Returns the text label that corresponds to the given \ref log_level.
		static std::string_view _get_level_label(log_level);
	};
}
