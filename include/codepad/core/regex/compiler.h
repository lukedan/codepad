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
		using transition_key = std::variant<codepoint_string, std::vector<std::pair<codepoint, codepoint>>>;
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
		[[nodiscard]] state_machine compile(const ast::nodes::subexpression &expr) const {
			state_machine result;
			result.start_state = result.create_state();
			result.end_state = result.create_state();
			_compile(result, result.start_state, result.end_state, expr);
			return result;
		}
	protected:
		/// Does nothing for error nodes.
		void _compile(state_machine&, std::size_t, std::size_t, const ast::nodes::error&) const {
		}
		/// Does nothing for feature nodes.
		void _compile(state_machine&, std::size_t, std::size_t, const ast::nodes::feature&) const {
		}
		/// Compiles the given literal node.
		void _compile(
			state_machine &result, std::size_t start, std::size_t end, const ast::nodes::literal &node
		) const {
			auto &transition = result.states[start].transitions.emplace_back();
			transition.condition = node.contents;
			transition.new_state_index = end;
		}
		/// Compiles the given character class.
		void _compile(
			state_machine &result, std::size_t start, std::size_t end, const ast::nodes::character_class &char_class
		) const {
			auto &transition = result.states[start].transitions.emplace_back();
			transition.condition = char_class.get_effective_ranges();
			transition.new_state_index = end;
		}
		/// Compiles the given subexpression, starting from the given state.
		void _compile(
			state_machine &result, std::size_t start, std::size_t end, const ast::nodes::subexpression &expr
		) const {
			if (expr.nodes.empty()) {
				result.states[start].transitions.emplace_back().new_state_index = end;
				return;
			}
			auto compile_expr = [this, &result](std::size_t cur, std::size_t next, const ast::node &n) {
				std::visit(
					[this, &result, cur, next](const auto &v) {
						_compile(result, cur, next, v);
					},
					n.value
				);
			};
			std::size_t current = start;
			auto it = expr.nodes.begin();
			for (; it + 1 != expr.nodes.end(); ++it) {
				std::size_t next = result.create_state();
				compile_expr(current, next, *it);
				current = next;
			}
			compile_expr(current, end, *it);
		}
		/// Compiles the given alternative expression.
		void _compile(
			state_machine &result, std::size_t start, std::size_t end, const ast::nodes::alternative &expr
		) const {
			for (const auto &alt : expr.alternatives) {
				_compile(result, start, end, alt);
			}
		}
		/// Compiles the given repetition.
		void _compile(
			state_machine &result, std::size_t start, std::size_t end, const ast::nodes::repetition &rep
		) const {
			std::size_t cur = start;
			for (std::size_t i = 1; i < rep.min; ++i) {
				std::size_t next = result.create_state();
				_compile(result, cur, next, rep.expression);
				cur = next;
			}
			if (rep.min == rep.max) {
				_compile(result, cur, end, rep.expression);
			} else if (rep.max == ast::nodes::repetition::no_limit) {
				if (rep.min > 0) {
					std::size_t next = result.create_state();
					_compile(result, cur, next, rep.expression);
					cur = next;
				}
				std::size_t next = result.create_state();
				_compile(result, cur, next, rep.expression);
				result.states[next].transitions.emplace_back().new_state_index = cur;
				result.states[cur].transitions.emplace_back().new_state_index = end;
			} else {
				if (rep.min > 0) {
					std::size_t next = result.create_state();
					_compile(result, cur, next, rep.expression);
					cur = next;
				}
				for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
					std::size_t next = result.create_state();
					_compile(result, cur, next, rep.expression);
					result.states[cur].transitions.emplace_back().new_state_index = end;
					cur = next;
				}
				_compile(result, cur, end, rep.expression);
				result.states[cur].transitions.emplace_back().new_state_index = end;
			}
		}
	};
}
