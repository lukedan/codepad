// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Profiling related code.

#include <chrono>
#include <unordered_map>

#include "logging.h"

namespace codepad {
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
		explicit performance_monitor(str_view_t lbl, log_condition cond = log_condition::late_only) :
			_label(lbl), _beg_time(clock_t::now()), _cond(cond) {
		}
		/// Constructs the \ref performance_monitor from the given label and expected running time.
		template <typename Dur> performance_monitor(
			str_view_t lbl, Dur exp, log_condition cond = log_condition::late_only
		) : performance_monitor(lbl, cond) {
			_expected = std::chrono::duration_cast<clock_t::duration>(exp);
		}
		/// Destructor. Logs if the running time exceeds the expected time.
		~performance_monitor() {
			auto dur = clock_t::now() - _beg_time;
			// TODO print duration directly after C++20
			auto sdur = std::chrono::duration_cast<std::chrono::duration<double>>(dur);
			if (dur > _expected && _cond != log_condition::never) {
				logger::get().log_info(CP_HERE) <<
					"operation took longer(" << sdur.count() << "s) than expected(" <<
					_expected.count() << "): " << _label;
			} else if (_cond == log_condition::always) {
				logger::get().log_debug(CP_HERE) << "operation took " << sdur.count() << "s: " << _label;
			}
		}

		/// Logs the time since the creation of this object so far.
		void log_time() {
			auto dur = std::chrono::duration_cast<std::chrono::duration<double>>(clock_t::now() - _beg_time);
			logger::get().log_debug(CP_HERE) << _label << ": operation has been running for " << dur.count() << "s";
		}
	protected:
		str_view_t _label; ///< The label for this performance monitor.
		clock_t::time_point _beg_time; ///< The time when this object is constructed.
		clock_t::duration _expected = clock_t::duration::max(); ///< The expected running time.
		log_condition _cond = log_condition::late_only; ///< The condition under which to log the running time.
	};


	/// Used to count the number of times a position in the code is reached.
	struct call_counter {
	public:
		/// Registers the given number of calls for the specific slot.
		void increment(const code_position &pos, size_t count = 1) {
			auto &&[iter, inserted] = _counters.try_emplace(pos, 0);
			iter->second += count;
			_has_calls = true; // count could be 0, but whatever
		}

		/// Dumps the result of all slots.
		void dump() const {
			if (_has_calls) {
				auto log = logger::get().log_debug(CP_HERE);
				log << "dumping call counters:";
				for (auto &pair : _counters) {
					log << "\n  " << pair.first.function << ": " << pair.second;
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

		/// Returns the global static \ref call_counter object.
		static call_counter &get();
	protected:
		std::unordered_map<code_position, size_t> _counters; ///< The counters.
		bool _has_calls = false; ///< Records if any calls have been registered.
	};
}
