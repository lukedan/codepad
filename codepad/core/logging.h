// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Logging related classes.

#include <sstream>
#include <functional>

#include "misc.h"
#include "encodings.h"

namespace codepad {
	/// Enumeration used to specify the level of logging and the type of individual log entries.
	enum class log_level : unsigned char {
		error, ///< Notification of internal program errors.
		warning, ///< Notification of exceptions that are not enough to crash the program.
		info, ///< Detailed logging that contains helpful information about the state of the program.
		debug ///< Debugging information.
	};

	/// Struct used to format and produce log.
	struct logger {
	public:
		using clock_t = std::chrono::high_resolution_clock; ///< The clock used to calculate time.
		/// A callback that handles a log message.
		using sink_t = std::function<void(const code_position&, log_level, str_view_t)>;
		/// Dummy struct that signals the \ref log_entry that a stacktrace should be added at this location.
		struct stacktrace_t {
		};
		/// Temporary struct used to record a single log entry, with additional formatting.
		struct log_entry {
			friend logger;
		public:
			/// Move constructor.
			log_entry(log_entry &&src) :
				_contents(std::move(src._contents)), _pos(std::move(src._pos)),
				_parent(src._parent), _level(src._level) {
				src._parent = nullptr;
			}
			/// No copy construction.
			log_entry(const log_entry&) = delete;
			/// Move assignment.
			log_entry &operator=(log_entry &&src) {
				_flush();
				_contents = std::move(src._contents);
				_pos = std::move(src._pos);
				_parent = src._parent;
				_level = src._level;
				src._parent = nullptr;
				return *this;
			}
			/// No copy assignment.
			log_entry &operator=(const log_entry&) = delete;
			/// Calls \ref _flush() to submit this log entry.
			~log_entry() {
				_flush();
			}

			/// Appends stacktrace information to this log entry. This function is platform-specific.
			void append_stacktrace();

			/// Appends a stacktrace to this entry.
			log_entry &operator<<(stacktrace_t) {
				append_stacktrace();
				return *this;
			}
			/// Appends the given contents to \ref _contents.
			template <typename T> log_entry &operator<<(T &&contents) {
				_contents << std::forward<T>(contents);
				return *this;
			}
		protected:
			/// Initializes \ref _parent.
			log_entry(logger &p, code_position pos, log_level lvl) : _pos(std::move(pos)), _parent(&p), _level(lvl) {
			}

			/// Submits this entry and resets \ref _parent, if this entry is valid.
			void _flush() {
				if (_parent) {
					auto message = _contents.str();
					for (auto &&sink : _parent->sinks) {
						sink(_pos, _level, message);
					}
					_parent = nullptr;
				}
			}

			std::stringstream _contents; ///< Stores the contents of this entry.
			code_position _pos; ///< The location where this entry is created.
			logger *_parent = nullptr; ///< The \ref logger that created this entry.
			log_level _level = log_level::error; ///< The log level of this entry.
		};

		constexpr static stacktrace_t stacktrace{}; ///< The static \ref stacktrace_t object.

		/// Default constructor.
		logger() = default;
		/// Constructs the \ref logger with the given list of sinks.
		explicit logger(std::vector<sink_t> snks) : sinks(std::move(snks)) {
		}

		/// Creates a new \ref log_entry with the specified \ref log_level.
		template <log_level Level> log_entry log(code_position cp) {
			return log_entry(*this, std::move(cp), Level);
		}
		/// Invokes \ref log() with \ref log_level::error.
		log_entry log_error(code_position cp) {
			return log<log_level::error>(cp);
		}
		/// Invokes \ref log() with \ref log_level::warning.
		log_entry log_warning(code_position cp) {
			return log<log_level::warning>(cp);
		}
		/// Invokes \ref log() with \ref log_level::info.
		log_entry log_info(code_position cp) {
			return log<log_level::info>(cp);
		}
		/// Invokes \ref log() with \ref log_level::debug.
		log_entry log_debug(code_position cp) {
			return log<log_level::debug>(cp);
		}

		/// Gets the global \ref logger object.
		static logger &get();

		std::vector<sink_t> sinks; ///< Sinks that accept log entries.
	};
}
