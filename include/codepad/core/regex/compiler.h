// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Compiler for regular expressions.

#include <deque>
#include <variant>

#include "parser.h"

namespace codepad::regex {
	/// A state machine corresponding to a regular expression.
	class state_machine {
	public:
		/// A key used to determine if a transition is viable.
		using transition_key = std::variant<codepoint_string, codepoint_range_list>;
		/// Stores the data of a transition.
		struct transition {
			transition_key condition; ///< Condition of this transition.
			std::size_t new_state_index; ///< Index of the state to transition to.
		};
		/// A state.
		struct state {
			std::vector<transition> transitions; ///< Transitions to new states.
		};

		std::vector<state> states; ///< States.
		std::size_t start_state = 0; ///< The starting state.
		std::size_t end_state = 0; ///< The ending state.

		/// Creates a new state in this \ref state_machine and returns its index.
		std::size_t create_state() {
			std::size_t res = states.size();
			states.emplace_back();
			return res;
		}
	};

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] state_machine compile(const ast::nodes::subexpression &expr) {
			_result = state_machine();
			_result.start_state = _result.create_state();
			_result.end_state = _result.create_state();
			_compile(_result.start_state, _result.end_state, expr);
			return std::move(_result);
		}
	protected:
		state_machine _result; ///< The result.

		/// Does nothing for error nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::feature&) {
		}
		/// Compiles the given literal node.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::literal &node) {
			auto &transition = _result.states[start].transitions.emplace_back();
			transition.condition = node.contents;
			transition.new_state_index = end;
		}
		/// Compiles the given backreference.
		void _compile(std::size_t, std::size_t, const ast::nodes::backreference&) {
			// TODO
		}
		/// Compiles the given character class.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::character_class &char_class) {
			auto &transition = _result.states[start].transitions.emplace_back();
			transition.condition = char_class.get_effective_ranges();
			transition.new_state_index = end;
		}
		/// Compiles the given subexpression, starting from the given state.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::subexpression &expr) {
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
			std::size_t current = start;
			auto it = expr.nodes.begin();
			for (; it + 1 != expr.nodes.end(); ++it) {
				std::size_t next = _result.create_state();
				compile_expr(current, next, *it);
				current = next;
			}
			compile_expr(current, end, *it);
		}
		/// Compiles the given alternative expression.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::alternative &expr) {
			for (const auto &alt : expr.alternatives) {
				_compile(start, end, alt);
			}
		}
		/// Compiles the given repetition.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::repetition &rep) {
			if (rep.min > 1000000 || (rep.max != ast::nodes::repetition::no_limit && rep.max > 1000000)) {
				// TODO better algorithm?
				return;
			}
			if (rep.max == 0) { // special case: don't match if the expression is matchedS
				std::size_t bad_state = _result.create_state();
				_compile(start, bad_state, rep.expression);
				_result.states[start].transitions.emplace_back().new_state_index = end;
				return;
			}
			std::size_t cur = start;
			for (std::size_t i = 1; i < rep.min; ++i) {
				std::size_t next = _result.create_state();
				_compile(cur, next, rep.expression);
				cur = next;
			}
			if (rep.min == rep.max) {
				_compile(cur, end, rep.expression);
			} else if (rep.max == ast::nodes::repetition::no_limit) {
				if (rep.min > 0) {
					std::size_t next = _result.create_state();
					_compile(cur, next, rep.expression);
					cur = next;
				}
				std::size_t next = _result.create_state();
				_compile(cur, next, rep.expression);
				_result.states[next].transitions.emplace_back().new_state_index = cur;
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			} else {
				if (rep.min > 0) {
					std::size_t next = _result.create_state();
					_compile(cur, next, rep.expression);
					cur = next;
				}
				for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
					std::size_t next = _result.create_state();
					_compile(cur, next, rep.expression);
					_result.states[cur].transitions.emplace_back().new_state_index = end;
					cur = next;
				}
				_compile(cur, end, rep.expression);
				_result.states[cur].transitions.emplace_back().new_state_index = end;
			}
		}
	};
}
