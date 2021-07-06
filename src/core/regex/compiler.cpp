// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/compiler.h"

/// \file
/// Regex tests using the PCRE2 test data.

namespace codepad::regex {
	namespace compiled {
		std::size_t state_machine::create_state(bool is_atomic) {
			std::size_t res = states.size();
			states.emplace_back().is_atomic = is_atomic;
			return res;
		}
	}


	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::literal &node) {
		auto &transition = _result.states[start].transitions.emplace_back();
		transition.new_state_index = end;
		transition.case_insensitive = node.case_insensitive;
		if (node.case_insensitive) {
			auto &cond_str = transition.condition.emplace<codepoint_string>();
			for (codepoint cp : node.contents) {
				cond_str.push_back(unicode::case_folding::get_cached().fold_simple(cp));
			}
		} else {
			transition.condition = node.contents;
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::subexpression &expr) {
		if (expr.nodes.empty()) {
			_result.states[start].transitions.emplace_back().new_state_index = end;
			return;
		}

		auto compile_expr = [this](std::size_t cur, std::size_t next, const ast::node &n) {
			std::visit(
				[this, cur, next](const auto &v) {
					_compile(cur, next, v);
				},
				n.value
			);
		};

		if (expr.subexpr_type == ast::nodes::subexpression::type::atomic) {
			++_atomic_counter;
			// we need to make sure all states in this assertion is atomic
			// so we create new start & finish nodes, and dummy transitions
			auto new_start = _create_state();
			auto new_end = _create_state();
			_result.states[start].transitions.emplace_back().new_state_index = new_start;
			_result.states[new_end].transitions.emplace_back().new_state_index = end;
			start = new_start;
			end = new_end;
		}
		std::size_t current = start;
		auto it = expr.nodes.begin();
		for (; it + 1 != expr.nodes.end(); ++it) {
			std::size_t next = _create_state();
			compile_expr(current, next, *it);
			current = next;
		}
		compile_expr(current, end, *it);
		if (expr.subexpr_type == ast::nodes::subexpression::type::atomic) {
			--_atomic_counter;
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::repetition &rep) {
		if (rep.min > 1000000 || (rep.max != ast::nodes::repetition::no_limit && rep.max > 1000000)) {
			// TODO better algorithm?
			return;
		}
		if (rep.max == 0) { // special case: don't match if the expression is matched zero times
			std::size_t bad_state = _create_state();
			_compile(start, bad_state, rep.expression);
			_result.states[start].transitions.emplace_back().new_state_index = end;
			return;
		}
		std::size_t cur = start;
		for (std::size_t i = 1; i < rep.min; ++i) {
			std::size_t next = _create_state();
			_compile(cur, next, rep.expression);
			cur = next;
		}
		if (rep.min == rep.max) {
			_compile(cur, end, rep.expression);
		} else if (rep.max == ast::nodes::repetition::no_limit) {
			if (rep.min > 0) {
				std::size_t next = _create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			} else {
				// create a new state with epsilon transition,
				// so that we don't have an edge back to the starting node
				std::size_t next = _create_state();
				_result.states[cur].transitions.emplace_back().new_state_index = next;
				cur = next;
			}
			std::size_t next = _create_state();
			if (rep.lazy) {
				_result.states[cur].transitions.emplace_back().new_state_index = end;
				_compile(cur, next, rep.expression);
			} else {
				_compile(cur, next, rep.expression);
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			}
			_result.states[next].transitions.emplace_back().new_state_index = cur;
		} else {
			if (rep.min > 0) {
				std::size_t next = _create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			}
			for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
				std::size_t next = _create_state();
				if (rep.lazy) {
					_result.states[cur].transitions.emplace_back().new_state_index = end;
					_compile(cur, next, rep.expression);
				} else {
					_compile(cur, next, rep.expression);
					_result.states[cur].transitions.emplace_back().new_state_index = end;
				}
				cur = next;
			}
			if (rep.lazy) {
				_result.states[cur].transitions.emplace_back().new_state_index = end;
				_compile(cur, end, rep.expression);
			} else {
				_compile(cur, end, rep.expression);
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			}
		}
	}

	void compiler::_compile(std::size_t start, std::size_t end, const ast::nodes::assertion &rep) {
		auto &transition = _result.states[start].transitions.emplace_back();
		transition.new_state_index = end;
		auto &assertion = transition.condition.emplace<compiled::assertion>();
		assertion.assertion_type = rep.assertion_type;
		if (rep.assertion_type >= ast::nodes::assertion::type::complex_first) {
			compiler cmp;
			assertion.expression = cmp.compile(rep.expression);
		} else if (rep.assertion_type >= ast::nodes::assertion::type::character_class_first) {
			assertion.expression.states.emplace_back().transitions.emplace_back().condition =
				std::get<ast::nodes::character_class>(rep.expression.nodes[0].value).get_effective_ranges();
		}
	}
}
