#pragma once

#include <string>
#include <queue>

#include "encodings.h"
#include "misc.h"

namespace codepad {
	class performance_monitor {
	public:
		struct operation {
			operation() = default;
			operation(const char8_t *slbl, str_t dlbl, size_t dep) :
				dynamic_label(std::move(dlbl)), stack_depth(dep), static_label(slbl) {
			}

			void register_begin() {
				begin_time = get_uptime().count();
			}
			void register_end() {
				end_time = get_uptime().count();
			}

			str_t dynamic_label;
			double begin_time = 0.0, end_time = 0.0;
			size_t stack_depth = 0;
			const char8_t *static_label = nullptr;
		};

		const std::deque<operation> &get_recorded_operations() const {
			return _ops;
		}

		void update() {
			double mint = get_uptime().count() - _keep;
			auto it = _ops.begin();
			for (; it != _ops.end() && it->end_time < mint; ++it) {
			}
			_ops.erase(_ops.begin(), it);
		}

		operation begin_operation(const char8_t *slbl, str_t dlbl) {
			operation op(slbl, std::move(dlbl), _stk++);
			op.register_begin();
			return op;
		}
		operation &end_operation(operation op) {
			--_stk;
			return end_operation_nostack(std::move(op));
		}
		operation &end_operation_nostack(operation op) {
			op.register_end();
			return _add_op(std::move(op));
		}

		double get_log_duration() const {
			return _keep;
		}
		void set_log_duration(double v) {
			_keep = v;
		}

		static performance_monitor &get();
	protected:
		std::deque<operation> _ops;
		double _keep = 0.2;
		size_t _stk = 0;

		// assumes operations are logged in increasing order of end_time with few exceptions
		operation &_add_op(operation op) {
			auto it = _ops.end();
			while (it != _ops.begin()) {
				--it;
				if (it->end_time <= op.end_time) {
					++it;
					break;
				}
			}
			return *_ops.insert(it, std::move(op));
		}
	};
	struct monitor_performance {
	public:
		monitor_performance() = default;
		explicit monitor_performance(
			const char8_t *slbl, double exp = std::numeric_limits<double>::quiet_NaN()
		) : _op(performance_monitor::get().begin_operation(slbl, str_t{})), _expected(exp) {
		}
		explicit monitor_performance(
			const code_position &cp, double exp = std::numeric_limits<double>::quiet_NaN()
		) : monitor_performance(reinterpret_cast<const char8_t*>(cp.function), exp) {
		}
		monitor_performance(
			const char8_t *slbl, str_t dlbl, double exp = std::numeric_limits<double>::quiet_NaN()
		) : _op(performance_monitor::get().begin_operation(slbl, std::move(dlbl))), _expected(exp) {
		}
		monitor_performance(
			const code_position &cp, str_t dlbl, double exp = std::numeric_limits<double>::quiet_NaN()
		) : monitor_performance(reinterpret_cast<const char8_t*>(cp.function), std::move(dlbl), exp) {
		}
		~monitor_performance() {
			performance_monitor::operation &res = performance_monitor::get().end_operation(std::move(_op));
			if (!std::isnan(_expected)) {
				double secs = std::chrono::duration<double>(res.end_time - res.begin_time).count();
				if (secs > _expected) {
					logger::get().log_warning(
						CP_HERE, "operation taking longer(", secs, "s) than expected(", _expected, "s): ",
						res.static_label, " ", res.dynamic_label
					);
				}
			}
		}

		performance_monitor::operation &get_operation() {
			return _op;
		}
	protected:
		performance_monitor::operation _op;
		double _expected = std::numeric_limits<double>::quiet_NaN();
	};
}
