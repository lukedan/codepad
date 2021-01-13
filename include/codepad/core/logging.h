// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Logging related classes.

#include <sstream>
#include <vector>
#include <chrono>
#include <functional>
#include <mutex>

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

	/// Receives and processes log messages.
	class log_sink {
	public:
		/// Default virtual destructor.
		virtual ~log_sink() = default;

		/// Called when a message is sent.
		virtual void on_message(
			const std::chrono::duration<double>&, const code_position&, log_level, std::u8string_view
		) = 0;
	};

	/// Struct used to format and produce log. This struct is thread-safe.
	struct APIGEN_EXPORT_RECURSIVE logger {
	public:
		using clock_t = std::chrono::high_resolution_clock; ///< The clock used to calculate time.
		/// Dummy struct that signals the \ref log_entry that a stacktrace should be added at this location.
		struct stacktrace_t {
		};
		/// Temporary struct used to record a single log entry, with additional formatting.
		struct log_entry {
			friend logger;
		public:
			/// Move constructor.
			log_entry(log_entry &&src) noexcept :
				_contents(std::move(src._contents)), _pos(std::move(src._pos)),
				_parent(src._parent), _level(src._level) {
				src._parent = nullptr;
			}
			/// No copy construction.
			log_entry(const log_entry&) = delete;
			/// Move assignment.
			log_entry &operator=(log_entry &&src) noexcept {
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

#ifdef CP_LOG_STACKTRACE
			/// Appends stacktrace information to this log entry. This function is platform-specific.
			void append_stacktrace();
#else
			void append_stacktrace() {
				*this << "\n-- [stacktrace disabled] --\n";
			}
#endif

			/// Appends a stacktrace to this entry.
			log_entry &operator<<(stacktrace_t) {
				append_stacktrace();
				return *this;
			}
			/// Appends the given contents to \ref _contents.
			template <typename T> log_entry &operator<<(T &&contents) {
				// make std::stringstream work with char8_t
				if constexpr (
					std::is_same_v<std::decay_t<T>, std::u8string> ||
					std::is_same_v<std::decay_t<T>, std::u8string_view>
				) {
					_contents << std::string_view(reinterpret_cast<const char*>(contents.data()), contents.size());
				} else if constexpr (
					std::is_same_v<std::decay_t<T>, const char8_t*> ||
					std::is_same_v<std::decay_t<T>, char8_t*>
				) {
					_contents << reinterpret_cast<const char*>(contents);
				} else {
					_contents << std::forward<T>(contents);
				}
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
					auto dur = clock_t::now() - _parent->get_creation_time();
					{
						std::lock_guard<std::mutex> mtx(_parent->_mutex);
						for (auto &&sink : _parent->sinks) {
							sink->on_message(
								dur, _pos, _level,
								std::u8string_view(reinterpret_cast<const char8_t*>(message.c_str()), message.size())
							);
						}
					}
					_parent = nullptr;
				}
			}

			/// Stores the contents of this entry. Since the standard library only provides \p basic_stringstream
			/// implementations for \p char and \p wchar_t, we're stuck with \p std::stringstream here.
			std::stringstream _contents;
			code_position _pos; ///< The location where this entry is created.
			logger *_parent = nullptr; ///< The \ref logger that created this entry.
			log_level _level = log_level::error; ///< The log level of this entry.
		};
		friend log_entry;

		constexpr static stacktrace_t stacktrace{}; ///< The static \ref stacktrace_t object.

		/// Creates a logger with no sinks.
		logger() : logger(std::vector<std::unique_ptr<log_sink>>()) {
		}
		/// Initializes the list of sinks.
		explicit logger(std::vector<std::unique_ptr<log_sink>> sinks) :
			sinks(std::move(sinks)), _creation(clock_t::now()) {
		}

		/// Creates a new \ref log_entry with the specified \ref log_level.
		template <log_level Level> log_entry log(code_position cp) {
			return log_entry(*this, std::move(cp), Level);
		}
		/// Non-template version of \ref log().
		log_entry log(log_level lvl, code_position cp) {
			return log_entry(*this, std::move(cp), lvl);
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

		/// Returns the time of this logger's creation.
		clock_t::time_point get_creation_time() const {
			return _creation;
		}

		/// Gets the current global \ref logger.
		inline static logger &get() {
			return *_get_ptr();
		}
		/// Sets the current \ref logger.
		inline static void set_current(std::unique_ptr<logger> c) {
			_get_ptr() = std::move(c);
		}

		std::vector<std::unique_ptr<log_sink>> sinks; ///< Sinks that accept log entries.
	protected:
		/// Returns a reference to the pointer to the global logger object. Here the object is managed automatically
		/// since static objects may still log when cleaning up after \p main() returns.
		static std::unique_ptr<logger> &_get_ptr();

		clock_t::time_point _creation; ///< The time of this logger's creation.
		std::mutex _mutex; ///< Mutex used for synchronization.
	};
}
