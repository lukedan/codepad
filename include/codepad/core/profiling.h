// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Profiling related code.

#include <chrono>
#include <unordered_map>

#include "logging.h"
#include "assert.h"

namespace codepad {
	/// Demangles a given type name for more human-readable output.
	std::u8string demangle(const char*);


	/// Struct that monitors the beginning, ending, and duration of its lifespan.
	struct performance_monitor {
	public:
		using clock_t = std::chrono::high_resolution_clock; ///< The clock used for measuring performance.

		/// Determines when and how should the measured running time be logged.
		enum class log_condition : unsigned char {
			always, ///< Always log time.
			late_only, ///< Only when the execution time exceeds the expected time.
			never ///< Never log time.
		};

		/// Constructs the \ref performance_monitor from the given label.
		explicit performance_monitor(std::u8string_view lbl, log_condition cond = log_condition::late_only) :
			_label(lbl), _beg_time(clock_t::now()), _cond(cond) {
		}
		/// Constructs the \ref performance_monitor from the given label and expected running time.
		template <typename Dur> performance_monitor(
			std::u8string_view lbl, Dur exp, log_condition cond = log_condition::late_only
		) : performance_monitor(lbl, cond) {
			_expected = std::chrono::duration_cast<clock_t::duration>(exp);
		}
		/// Destructor. Logs if the running time exceeds the expected time.
		~performance_monitor();

		/// Logs the time since the creation of this object so far.
		void log_time();
	protected:
		std::u8string_view _label; ///< The label for this performance monitor.
		clock_t::time_point _beg_time; ///< The time when this object is constructed.
		clock_t::duration _expected = clock_t::duration::max(); ///< The expected running time.
		log_condition _cond = log_condition::late_only; ///< The condition under which to log the running time.
	};


	/// Used to count the number of times a position in the code is reached.
	struct call_counter {
	public:
		/// Registers the given number of calls for the specific slot.
		void increment(const std::source_location &pos, std::size_t count = 1) {
			auto &&[iter, inserted] = _counters.try_emplace(pos, 0);
			iter->second += count;
			_has_calls = true; // count could be 0, but whatever
		}

		/// Dumps the result of all slots.
		void dump() const {
			if (_has_calls) {
				auto log = logger::get().log_debug();
				log << "dumping call counters:";
				for (auto &pair : _counters) {
					log <<
						"\n  " << reinterpret_cast<const char8_t*>(pair.first.function_name()) << ": " <<
						pair.second;
				}
			}
		}
		/// Resets the value of all slots to zero.
		void reset() {
			for (auto &pair : _counters) {
				pair.second = 0;
			}
			_has_calls = false;
		}
	protected:
		std::unordered_map<
			std::source_location, std::size_t,
			source_location_hash, source_location_equal_to
		> _counters; ///< The counters.
		bool _has_calls = false; ///< Records if any calls have been registered.
	};
}
