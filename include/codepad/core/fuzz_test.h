// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Fuzz test utilities. This file is not actually used by the main project; it's only used by various fuzz tests.

#include <csignal>
#include <atomic>
#include <chrono>
#include <memory>
#include <random>

#include "codepad/core/logging.h"
#include "codepad/core/logger_sinks.h"

namespace codepad {
	/// An abstract fuzz test.
	class fuzz_test {
	public:
		using random_engine = std::default_random_engine; ///< Random engine.

		/// Initializes the random engine with a dfixed default seed.
		fuzz_test() : fuzz_test(123456) {
		}
		/// Initializes the random engine with the specified seed.
		explicit fuzz_test(random_engine::result_type seed) : rng(seed) {
		}
		/// Default virtual destructor.
		virtual ~fuzz_test() = default;

		/// Returns the name of this test.
		[[nodiscard]] virtual std::u8string_view get_name() const = 0;

		/// Initializes the test. This is called after everything's set up and before \ref iterate().
		virtual void initialize() {
		}
		/// Runs one iteration of the fuzz test. This will be executed repeatedly until \p SIGINT is received.
		virtual void iterate() = 0;
		/// Logs the current status of this test.
		virtual void log_status(logger::log_entry &entry) const {
			entry << "[No details]";
		}

		/// The main function of a fuzz test. This function sets up the fuzz test by registering for \p SIGINT,
		/// setting up the logger, calling \ref initialize(), etc.
		[[nodiscard]] inline static int main(int argc, char **argv, std::unique_ptr<fuzz_test> test) {
			// initialization
			static std::atomic_bool _keep_running = true;
			std::signal(SIGINT, [](int) {
				_keep_running = false;
			});

			auto global_log = std::make_unique<logger>();
			global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
			logger::set_current(std::move(global_log));

			codepad::initialize(argc, argv);

			logger::get().log_info(CP_HERE) << "Initializing fuzz test: " << test->get_name();
			test->initialize();

			auto last_log = std::chrono::high_resolution_clock::now();
			auto start_time = last_log;

			for (std::size_t i = 1; _keep_running; ++i) {
				test->iterate();

				auto now = std::chrono::high_resolution_clock::now();
				if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log).count() >= 1) {
					auto secs = std::chrono::duration<double>(now - start_time).count();
					auto entry = logger::get().log_info(CP_HERE);
					entry <<
						"Fuzz test: " << test->get_name() << "\n" <<
						"Elapsed time: " << std::chrono::duration<double>(now - start_time) << "\n" <<
						"Total iterations: " << i << "\n"
						"Iterations/second: " << i / secs << "\n";
					test->log_status(entry);
					last_log = now;
				}
			}

			logger::get().log_info(CP_HERE) << "Exiting fuzz test normally: " << test->get_name();

			return 0;
		}

		random_engine rng; ///< The random number generator.
	};
}
