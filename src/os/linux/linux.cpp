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
#	include <cxxabi.h>
namespace codepad {
	/// Attempts to demangle and output the given stacktrace entry.
	void _write_demangled_stacktrace_entry(std::ostream &out, char *str) {
		char *func_begin = nullptr, *func_end = nullptr, *end = str;
		for (; *end; ++end) {
			if (*end == '(') {
				func_begin = end + 1;
			} else if (*end == '+') {
				func_end = end;
			}
		}
		if (func_begin && func_end && func_begin < func_end) { // function name found, can demangle
			int st;
			*func_end = '\0';
			char *result = abi::__cxa_demangle(func_begin, nullptr, nullptr, &st);
			*func_end = '+';
			if (st == 0) {
				out <<
					std::string_view(str, static_cast<std::size_t>(func_begin - str)) <<
					result <<
					std::string_view(func_end, static_cast<std::size_t>(end - func_end));
				std::free(result);
				return;
			}
		}
		out << str;
	}

	void logger::log_entry::append_stacktrace() {
		const int buffer_size = 200;
		void *buffer[buffer_size];
		int count = backtrace(buffer, buffer_size);
		char **symbols = backtrace_symbols(buffer, count);
		assert_true_sys(symbols != nullptr, "not enough memory for backtrace");

		_contents << "\n-- stacktrace --\n";
		for (int i = 0; i < count; ++i) {
			_contents << "  ";
			_write_demangled_stacktrace_entry(_contents, symbols[i]);
			_contents << "\n";
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
