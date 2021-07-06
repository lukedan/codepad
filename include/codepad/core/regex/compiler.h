// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Compiler for regular expressions.

#include <deque>
#include <variant>

#include "parser.h"
#include "parser.inl"

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
			std::size_t create_state(bool is_atomic);

			/// Dumps this state macine into a DOT file.
			template <typename Stream> void dump(Stream&, bool valid_only = true) const;
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
			bool is_atomic = false; ///< Whether or not to disable backtracking for this node.
		};


		template <typename Stream> inline void state_machine::dump(Stream &stream, bool valid_only) const {
			stream << "digraph {\n";
			stream << "n" << start_state << "[color=red];\n";
			stream << "n" << end_state << "[color=blue];\n";
			auto is_printable_char = [](codepoint cp) {
				return cp >= 0x20 && cp <= 0x7E;
			};
			auto print_char = [&](codepoint cp) {
				if (is_printable_char(cp)) {
					stream << static_cast<char>(cp);
				}
				stream << "[" << std::hex << cp << std::dec << "]";
			};
			for (std::size_t i = 0; i < states.size(); ++i) {
				const auto &s = states[i];
				for (std::size_t j = 0; j < s.transitions.size(); ++j) {
					auto &t = s.transitions[j];
					stream << "n" << i << " -> n" << t.new_state_index << " [label=\"" << j << ": ";
					if (std::holds_alternative<codepoint_string>(t.condition)) {
						const auto &str = std::get<codepoint_string>(t.condition);
						for (codepoint cp : str) {
							if (is_printable_char(cp)) {
								stream << static_cast<char>(cp);
							} else {
								stream << "?";
							}
						}
					} else if (std::holds_alternative<codepoint_range_list>(t.condition)) {
						const auto &list = std::get<codepoint_range_list>(t.condition);
						bool first = true;
						for (auto range : list.ranges) {
							if (first) {
								first = false;
							} else {
								stream << ",";
							}
							if (valid_only && !is_printable_char(range.first)) {
								stream << "...";
								break;
							}
							print_char(range.first);
							if (range.last != range.first) {
								stream << "-";
								print_char(range.last);
							}
						}
					} else {
						stream << "<assertion>";
					}
					stream << "\"];\n";
				}
			}
			stream << "}\n";
		}
	}

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] compiled::state_machine compile(const ast::nodes::subexpression &expr) {
			_result = compiled::state_machine();
			_atomic_counter = 0;
			_result.start_state = _result.create_state(false);
			_result.end_state = _result.create_state(false);
			_compile(_result.start_state, _result.end_state, expr);
			assert_true_logical(_atomic_counter == 0, "atomic counter has not been properly reset");
			return std::move(_result);
		}
	protected:
		compiled::state_machine _result; ///< The result.
		std::size_t _atomic_counter = 0; ///< Counts how many nested atomic groups we're currently in.

		/// Creates a new state in \ref _result using \ref compiled::state_machine::create_state(), determining
		/// whether it's atomic using \ref _atomic_counter.
		std::size_t _create_state() {
			return _result.create_state(_atomic_counter > 0);
		}

		/// Does nothing for error nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::feature&) {
		}
		/// Compiles the given literal node.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::literal&);
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
		void _compile(std::size_t start, std::size_t end, const ast::nodes::subexpression&);
		/// Compiles the given alternative expression.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::alternative &expr) {
			for (const auto &alt : expr.alternatives) {
				_compile(start, end, alt);
			}
		}
		/// Compiles the given repetition.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::repetition&);
		/// Compiles the given assertion.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::assertion&);
	};
}
