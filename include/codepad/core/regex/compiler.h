// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Compiler for regular expressions.

#include <deque>
#include <variant>

#include "parser.h"

namespace codepad::regex {
	namespace compiled {
		struct state;

		/// A state machine corresponding to a regular expression.
		class state_machine {
		public:
			std::vector<state> states; ///< States.
			std::size_t start_state = 0; ///< The starting state.
			std::size_t end_state = 0; ///< The ending state.

			/// Creates a new state in this \ref state_machine and returns its index.
			std::size_t create_state();
		};
		/// An assertion used in a transition.
		struct assertion {
			ast::nodes::assertion::type assertion_type; ///< The type of this assertion.
			codepoint_range_list character_class; ///< The character class potentially using by this assertion.
			state_machine expression; ///< The expression that is expected to match or not match.
		};
		/// A key used to determine if a transition is viable.
		using transition_key = std::variant<codepoint_string, codepoint_range_list, assertion>;
		/// Stores the data of a transition.
		struct transition {
			transition_key condition; ///< Condition of this transition.
			std::size_t new_state_index = 0; ///< Index of the state to transition to.
			bool case_insensitive = false; ///< Whether the condition is case-insensitive.
		};
		/// A state.
		struct state {
			std::vector<transition> transitions; ///< Transitions to new states.
		};

		inline std::size_t state_machine::create_state() {
			std::size_t res = states.size();
			states.emplace_back();
			return res;
		}
	}

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] compiled::state_machine compile(const ast::nodes::subexpression &expr) {
			_result = compiled::state_machine();
			_result.start_state = _result.create_state();
			_result.end_state = _result.create_state();
			_compile(_result.start_state, _result.end_state, expr);
			return std::move(_result);
		}
	protected:
		compiled::state_machine _result; ///< The result.

		/// Does nothing for error nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::feature&) {
		}
		/// Compiles the given literal node.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::literal &node) {
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
		/// Compiles the given backreference.
		void _compile(std::size_t, std::size_t, const ast::nodes::backreference&) {
			// TODO
		}
		/// Compiles the given character class.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::character_class &char_class) {
			auto &transition = _result.states[start].transitions.emplace_back();
			transition.condition = char_class.get_effective_ranges();
			transition.new_state_index = end;
			transition.case_insensitive = char_class.case_insensitive;
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
			if (rep.max == 0) { // special case: don't match if the expression is matched zero times
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
					std::size_t next = _result.create_state();
					_compile(cur, next, rep.expression);
					cur = next;
				}
				for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
					std::size_t next = _result.create_state();
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
		/// Compiles the given assertion.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::assertion &rep) {
			auto &transition = _result.states[start].transitions.emplace_back();
			transition.new_state_index = end;
			auto &assertion = transition.condition.emplace<compiled::assertion>();
			assertion.assertion_type = rep.assertion_type;
			if (rep.assertion_type >= ast::nodes::assertion::type::complex_first) {
				assertion.expression.states.emplace_back().transitions.emplace_back().condition =
					std::get<ast::nodes::character_class>(rep.expression.nodes[0].value).get_effective_ranges();
			} else if (rep.assertion_type >= ast::nodes::assertion::type::character_class_first) {
				compiler cmp;
				assertion.expression = cmp.compile(rep.expression);
			}
		}
	};
}
