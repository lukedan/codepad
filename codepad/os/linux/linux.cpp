/// \file
/// Implementation of certain linux-specific functions.

#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include <pthread.h>

#include "../../core/logger_sinks.h"
#include "../../ui/scheduler.h"

#ifdef CP_LOG_STACKTRACE
#	include <execinfo.h>
namespace codepad {
	void logger::log_entry::append_stacktrace() {
		const int buffer_size = 200;
		void *buffer[buffer_size];
		int count = backtrace(buffer, buffer_size);
		char **symbols = backtrace_symbols(buffer, count);
		assert_true_sys(symbols != nullptr, "not enough memory for backtrace");

		_contents << "\n-- stacktrace --\n";
		for (int i = 0; i < count; ++i) {
			_contents << "  " << symbols[i] << "\n";
		}
		_contents << "\n-- stacktrace --\n";

		std::free(symbols);
	}
}
#endif


namespace codepad::logger_sinks {
	std::size_t console_sink::_get_console_width() {
		struct winsize result{};
		int res = ioctl(STDOUT_FILENO, TIOCGWINSZ, &result);
		assert_true_sys(res >= 0, "ioctl() failed");
		if (result.ws_col == 0) { // FIXME clion
			return 200;
		}
		return result.ws_col;
	}
}


namespace codepad::ui {
	scheduler::thread_id_t scheduler::_get_thread_id() {
		return pthread_self();
	}
}
