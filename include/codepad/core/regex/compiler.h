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
		/// A state.
		struct state {
			std::vector<std::pair<transition_key, state*>> transitions; ///< Transitions to new states.
		};

		std::deque<state> states; ///< States.
		state *start_state = nullptr; ///< The starting state.
		state *end_state = nullptr; ///< The ending state.
	};

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] void compile(const ast::nodes::subexpression &expr) {
			result.start_state = &result.states.emplace_back();
			result.end_state = &result.states.emplace_back();
			_compile(*result.start_state, *result.end_state, expr);
		}

		state_machine result; ///< The result.
	protected:
		/// Does nothing for error nodes.
		[[nodiscard]] void _compile(state_machine::state&, state_machine::state&, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		[[nodiscard]] void _compile(state_machine::state&, state_machine::state&, const ast::nodes::feature&) {
		}
		/// Compiles the given literal node.
		[[nodiscard]] void _compile(
			state_machine::state &start, state_machine::state &end, const ast::nodes::literal &node
		) {
			auto &transition = start.transitions.emplace_back();
			transition.first = node.contents;
			transition.second = &end;
		}
		/// Compiles the given character class.
		[[nodiscard]] void _compile(
			state_machine::state &start, state_machine::state &end, const ast::nodes::character_class &char_class
		) {
			auto &transition = start.transitions.emplace_back();
			transition.first = char_class.get_effective_ranges();
			transition.second = &end;
		}
		/// Compiles the given subexpression, starting from the given state.
		[[nodiscard]] void _compile(
			state_machine::state &start, state_machine::state &end, const ast::nodes::subexpression &expr
		) {
			if (expr.nodes.empty()) {
				start.transitions.emplace_back().second = &end;
				return;
			}
			auto it = expr.nodes.begin();
			state_machine::state *current = &start;
			auto compile_expr = [this](state_machine::state &cur, state_machine::state &next, const ast::node &n) {
				std::visit(
					[this, &cur, &next](const auto &v) {
						_compile(cur, next, v);
					},
					n.value
				);
			};
			for (; it + 1 != expr.nodes.end(); ++it) {
				auto &next = result.states.emplace_back();
				compile_expr(*current, next, *it);
				current = &next;
			}
			compile_expr(*current, end, *it);
		}
		/// Compiles the given alternative expression.
		[[nodiscard]] void _compile(
			state_machine::state &start, state_machine::state &end, const ast::nodes::alternative &expr
		) {
			for (const auto &alt : expr.alternatives) {
				_compile(start, end, alt);
			}
		}
		/// Compiles the given repetition.
		[[nodiscard]] void _compile(
			state_machine::state &start, state_machine::state &end, const ast::nodes::repetition &rep
		) {
			state_machine::state *cur = &start;
			for (std::size_t i = 1; i < rep.min; ++i) {
				auto &next = result.states.emplace_back();
				_compile(*cur, next, rep.expression);
				cur = &next;
			}
			if (rep.min == rep.max) {
				_compile(*cur, end, rep.expression);
			} else if (rep.max == ast::nodes::repetition::no_limit) {
				auto &next = result.states.emplace_back();
				_compile(*cur, next, rep.expression);
				next.transitions.emplace_back().second = cur;
				cur->transitions.emplace_back().second = &end;
			} else {
				if (rep.min > 0) {
					auto &next = result.states.emplace_back();
					_compile(*cur, next, rep.expression);
					cur = &next;
				}
				for (std::size_t i = rep.min + 1; i < rep.max; ++i) {
					auto &next = result.states.emplace_back();
					_compile(*cur, next, rep.expression);
					cur->transitions.emplace_back().second = &end;
					cur = &next;
				}
				_compile(*cur, end, rep.expression);
				cur->transitions.emplace_back().second = &end;
			}
		}
	};
}
