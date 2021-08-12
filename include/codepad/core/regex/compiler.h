// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Compiler for regular expressions.

#include <deque>
#include <variant>
#include <vector>
#include <map>
#include <span>
#include <compare>

#include "parser.h"
#include "parser.inl"

namespace codepad::regex {
	namespace compiled {
		struct state;
		struct state_ref;

		/// Records the corresponding numbered capture indices of all named captures.
		struct named_capture_registry {
			/// Numbered capture indices for all named captures. Use \ref start_indices to obtain the range of
			/// indices that correspond to a specific name.
			std::vector<std::size_t> indices;
			/// Starting indices in \ref indices. This array has one additional element at the end (which is the
			/// length of \ref indices) for convenience.
			std::vector<std::size_t> start_indices;

			/// Returns the range in \ref indices that correspond to the given name.
			[[nodiscard]] std::span<const std::size_t> get_indices_for_name(std::size_t name_index) const {
				return {
					indices.begin() + start_indices[name_index],
					indices.begin() + start_indices[name_index + 1]
				};
			}
		};

		/// A state machine corresponding to a regular expression.
		class state_machine {
		public:
			std::vector<state> states; ///< States.
			named_capture_registry named_captures; ///< Mapping from named captures to regular indexed captures.
			std::size_t start_state = 0; ///< The starting state.
			std::size_t end_state = 0; ///< The ending state.

			/// Creates a new state in this \ref state_machine and returns its index.
			state_ref create_state();

			/// Dumps this state macine into a DOT file.
			void dump(std::ostream&, bool valid_only = true) const;
		};

		namespace transitions {
			/// A character class.
			struct character_class {
				codepoint_range_list ranges; ///< Ranges.
				bool is_negate = false; ///< Whether the codepoint should not match any character in this class.

				/// Tests whether the given codepoint is matched by this character class.
				[[nodiscard]] bool matches(codepoint cp, bool case_insensitive) const {
					bool result = ranges.contains(cp);
					if (case_insensitive && !result) {
						result = ranges.contains(unicode::case_folding::get_cached().fold_simple(cp));
						if (!result) {
							for (auto folded : unicode::case_folding::get_cached().inverse_fold_simple(cp)) {
								if (ranges.contains(folded)) {
									result = true;
									break;
								}
							}
						}
					}
					return is_negate ? !result : result;
				}
			};
			/// An assertion used in a transition.
			struct assertion {
				ast::nodes::assertion::type assertion_type; ///< The type of this assertion.
				codepoint_range_list character_class; ///< The character class potentially using by this assertion.
				state_machine expression; ///< The expression that is expected to match or not match.
			};
			/// Starts a capture.
			struct capture_begin {
				/// Indicates that a capture does not have a name.
				constexpr static std::size_t unnamed_capture = std::numeric_limits<std::size_t>::max();

				std::size_t index = 0; ///< Index of the capture.
				std::size_t name_index = unnamed_capture; ///< Name index of this capture.
			};
			/// Ends a capture.
			struct capture_end {
			};
			/// A numbered backreference.
			struct numbered_backreference {
				std::size_t index = 0; ///< Index of the capture.
			};
			/// A named backreference.
			struct named_backreference {
				std::size_t index = 0; ///< Index of the capture.
			};
			/// Resets the starting position of this match.
			struct reset_match_start {
			};
			/// Pushes the start of an atomic range.
			struct push_atomic {
			};
			/// Pops all states on stack pushed since the start of the last atomic range.
			struct pop_atomic {
			};

			/// Transitions that are conditions.
			namespace conditions {
				/// Checks if the given numbered group has been matched.
				struct numbered_capture {
					std::size_t index = 0; ///< The index of the capture.
				};
				/// Checks if the given named group has been matched.
				struct named_capture {
					std::size_t name_index = 0; ///< The index of the name.
				};
			}
		}
		/// Stores the data of a transition.
		struct transition {
			/// A key used to determine if a transition is viable.
			using key = std::variant<
				codepoint_string,
				transitions::character_class,
				transitions::assertion,
				transitions::capture_begin,
				transitions::capture_end,
				transitions::numbered_backreference,
				transitions::named_backreference,
				transitions::reset_match_start,
				transitions::push_atomic,
				transitions::pop_atomic,
				transitions::conditions::numbered_capture,
				transitions::conditions::named_capture
			>;

			key condition; ///< Condition of this transition.
			std::size_t new_state_index = 0; ///< Index of the state to transition to.
			bool case_insensitive = false; ///< Whether the condition is case-insensitive.
		};
		/// A state.
		struct state {
			std::vector<transition> transitions; ///< Transitions to new states.
			bool no_backtracking = false; ///< Whether the matcher should not backtrack to this state.
		};

		struct transition_ref;

		/// Safe reference to a \ref state in a \ref state_machine.
		struct state_ref {
			/// Initializes all fields of the struct.
			state_ref(state_machine &sm, std::size_t i) : graph(&sm), index(i) {
			}

			/// Returns the referenced state.
			[[nodiscard]] state &get() const {
				return graph->states[index];
			}
			/// \overload
			[[nodiscard]] state *operator->() const {
				return &get();
			}

			/// Shorthand for appending a transition to \ref state::transitions.
			transition_ref create_transition();

			state_machine *graph = nullptr; ///< The \ref state_machine that contains the state.
			std::size_t index = 0; ///< The index of the state.
		};
		/// Safe reference to a \ref transition.
		struct transition_ref {
			/// Initializes all fields of this struct.
			transition_ref(state_ref s, std::size_t i) : state(s), index(i) {
			}

			/// Returns the referenced transition.
			[[nodiscard]] transition &get() const {
				return state->transitions[index];
			}
			/// \overload
			[[nodiscard]] transition *operator->() const {
				return &get();
			}

			state_ref state; ///< Reference to the \ref state that contains this transition.
			std::size_t index = 0; ///< The index of this transition.
		};
	}

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] compiled::state_machine compile(const ast::nodes::subexpression&);
	protected:
		/// Information about a named capture.
		struct _named_capture_info {
			codepoint_string name; ///< The name of this capture.
			std::size_t index = 0; ///< Corresponding capture index.

			/// Default comparisons.
			friend static std::strong_ordering operator<=>(
				const _named_capture_info&, const _named_capture_info&
			) = default;
		};

		compiled::state_machine _result; ///< The result.
		std::vector<_named_capture_info> _named_captures; ///< Information about all named captures.
		std::vector<codepoint_string> _capture_names; ///< All unique capture names, sorted.

		/// Fallback for nodes with no capture names to collect.
		template <typename Node> void _collect_capture_names(const Node&) {
		}
		/// Collects a name if the subexpression is named, and then recursively collects capture names from its
		/// children.
		void _collect_capture_names(const ast::nodes::subexpression &expr) {
			if (!expr.capture_name.empty()) {
				_named_captures.emplace_back(expr.capture_name, expr.capture_index);
			}
			for (const auto &n : expr.nodes) {
				std::visit([this](auto &&val) {
					_collect_capture_names(val);
				}, n.value);
			}
		}
		/// Collects names from all alternatives.
		void _collect_capture_names(const ast::nodes::alternative &expr) {
			for (const auto &alt : expr.alternatives) {
				_collect_capture_names(alt);
			}
		}
		/// Collects names from the repeated sequence.
		void _collect_capture_names(const ast::nodes::repetition &expr) {
			_collect_capture_names(expr.expression);
		}
		/// Collects names from the lookahead/lookbehind if necessary.
		void _collect_capture_names(const ast::nodes::assertion &expr) {
			if (expr.assertion_type >= ast::nodes::assertion::type::complex_first) {
				_collect_capture_names(expr.expression);
			}
		}

		/// Does nothing for error nodes.
		void _compile(compiled::state_ref, compiled::state_ref, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(compiled::state_ref, compiled::state_ref, const ast::nodes::feature&) {
		}
		/// Compiles a \ref ast::nodes::match_start_override.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::match_start_override&) {
			auto transition = start.create_transition();
			transition->condition.emplace<compiled::transitions::reset_match_start>();
			transition->new_state_index = end.index;
		}
		/// Compiles the given literal node.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::literal&);
		/// Compiles the given numbered backreference.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::numbered_backreference&);
		/// Compiles the given named backreference.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::named_backreference&);
		/// Compiles the given character class.
		void _compile(
			compiled::state_ref start, compiled::state_ref end, const ast::nodes::character_class &char_class
		) {
			auto transition = start.create_transition();
			auto &cls = transition->condition.emplace<compiled::transitions::character_class>();
			cls.ranges = char_class.ranges;
			cls.is_negate = char_class.is_negate;
			transition->new_state_index = end.index;
			transition->case_insensitive = char_class.case_insensitive;
		}
		/// Compiles the given subexpression, starting from the given state.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::subexpression&);
		/// Compiles the given alternative expression.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::alternative &expr) {
			for (const auto &alt : expr.alternatives) {
				_compile(start, end, alt);
			}
		}
		/// Compiles the given repetition.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::repetition&);
		/// Compiles the given assertion.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::assertion&);
		/// Compiles the given conditional subexpression.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::conditional_expression&);

		/// Does nothing - <tt>DEFINE</tt>'s are handled separately.
		void _compile_condition(compiled::transition&, const ast::nodes::conditional_expression::define&) {
		}
		/// Compiles the given condition that checks for a numbered capture.
		void _compile_condition(
			compiled::transition &trans, const ast::nodes::conditional_expression::numbered_capture_available &cond
		) {
			trans.condition.emplace<compiled::transitions::conditions::numbered_capture>().index = cond.index;
		}
		/// Compiles the given condition that checks for a named capature.
		void _compile_condition(
			compiled::transition &trans, const ast::nodes::conditional_expression::named_capture_available &cond
		) {
			auto it = std::lower_bound(_capture_names.begin(), _capture_names.end(), cond.name);
			trans.condition.emplace<compiled::transitions::conditions::named_capture>().name_index =
				it - _capture_names.begin();
		}
		/// Compiles the given assertion.
		void _compile_condition(compiled::transition&, const ast::nodes::assertion&);
	};
}
