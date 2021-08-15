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
			/// Indicates that a numbered capture does not have a corresponding reverse mapping.
			constexpr static std::size_t no_reverse_mapping = std::numeric_limits<std::size_t>::max();

			/// Numbered capture indices for all named captures. Use \ref start_indices to obtain the range of
			/// indices that correspond to a specific name.
			std::vector<std::size_t> indices;
			/// Starting indices in \ref indices. This array has one additional element at the end (which is the
			/// length of \ref indices) for convenience.
			std::vector<std::size_t> start_indices;
			/// Mapping from numbered capture indices to the corresponding named capture indices.
			std::vector<std::size_t> reverse_mapping;

			/// Returns the range in \ref indices that correspond to the given name.
			[[nodiscard]] std::span<const std::size_t> get_indices_for_name(std::size_t name_index) const {
				return {
					indices.begin() + start_indices[name_index],
					indices.begin() + start_indices[name_index + 1]
				};
			}
			/// Returns the index of the name corresponding to the group at the given index.
			[[nodiscard]] std::size_t get_name_index_for_group(std::size_t index) const {
				if (index < reverse_mapping.size()) {
					return reverse_mapping[index];
				}
				return no_reverse_mapping;
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
				[[nodiscard]] bool matches(codepoint, bool case_insensitive) const;
			};
			using simple_assertion = ast::nodes::simple_assertion; ///< Simple assertion.
			/// An assertion that checks if we are at a character class boundary.
			struct character_class_assertion {
				character_class char_class; ///< The character class.
				bool boundary = false; ///< Whether we're expecting a boundary.
			};
			/// Starts a capture.
			struct capture_begin {
				std::size_t index = 0; ///< Index of the capture.
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
			/// Pushes a subroutine stack frame indicating that once \ref target is reached, jumps to
			/// \ref return_state. This should only be used to implement subroutines and recursions.
			struct jump {
				/// The target state. Once this state is reached, matching jumps back to \ref final.
				std::size_t target = 0;
				/// The state to jump to once \ref target is reached.
				std::size_t return_state = 0;
				std::size_t subroutine_index = 0; ///< Index of the subroutine.
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
			/// Pushes the current input stream as a checkpoint, which can be restored later using
			/// \ref restore_stream_checkpoint.
			struct push_stream_checkpoint {
			};
			/// Restores the checkpointed stream, previously pushed using \ref push_stream_checkpoint.
			struct restore_stream_checkpoint {
			};

			/// Transitions that are conditions.
			namespace conditions {
				/// Checks if we're currently in a recursion.
				struct numbered_recursion {
					/// Indicates that any ongoing recursion satisfies this condition.
					constexpr static std::size_t any_index = std::numeric_limits<std::size_t>::max();

					/// Index of the group to check for, or \ref any_index to indicate any recursion.
					std::size_t index = any_index;
				};
				/// Checks if we're currently in the specified named subroutine call.
				struct named_recursion {
					std::size_t name_index = 0; ///< Index of the name in \ref state_machine::named_captures.
				};
				/// Checks if the given numbered group has been matched.
				struct numbered_capture {
					std::size_t index = 0; ///< The index of the capture.
				};
				/// Checks if the given named group has been matched.
				struct named_capture {
					std::size_t name_index = 0; ///< Index of the name in \ref state_machine::named_captures.
				};
			}
		}
		/// Stores the data of a transition.
		struct transition {
			/// A key used to determine if a transition is viable.
			using key = std::variant<
				codepoint_string,
				transitions::character_class,
				transitions::simple_assertion,
				transitions::character_class_assertion,
				transitions::capture_begin,
				transitions::capture_end,
				transitions::numbered_backreference,
				transitions::named_backreference,
				transitions::jump,
				transitions::reset_match_start,
				transitions::push_atomic,
				transitions::pop_atomic,
				transitions::push_stream_checkpoint,
				transitions::restore_stream_checkpoint,
				transitions::conditions::numbered_recursion,
				transitions::conditions::named_recursion,
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
		};

		struct transition_ref;

		/// Safe reference to a \ref state in a \ref state_machine.
		struct state_ref {
			/// Initializes this object as empty.
			state_ref() = default;
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

			/// Returns whether this object is empty.
			[[nodiscard]] bool is_empty() const {
				return graph == nullptr;
			}

			/// Shorthand for appending a transition to \ref state::transitions.
			transition_ref create_transition();

			state_machine *graph = nullptr; ///< The \ref state_machine that contains the state.
			std::size_t index = 0; ///< The index of the state.
		};
		/// Safe reference to a \ref transition.
		struct transition_ref {
			/// Default constructor.
			transition_ref() = default;
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
		/// States of a capture group.
		struct _capture_info {
			compiled::state_ref start; ///< The starting state.
			compiled::state_ref end; ///< The end state.

			/// Returns whether this slot is currently empty.
			[[nodiscard]] bool is_empty() const {
				return start.is_empty();
			}
		};
		/// Information about a subroutine transition.
		struct _subroutine_transition {
			/// Initializes all fields of this struct.
			_subroutine_transition(compiled::transition_ref t, std::size_t i) : transition(t), index(i) {
			}

			compiled::transition_ref transition; ///< The transition.
			std::size_t index = 0; ///< The group index.
		};

		compiled::state_machine _result; ///< The result.
		std::vector<_named_capture_info> _named_captures; ///< Information about all named captures.
		std::vector<codepoint_string> _capture_names; ///< All unique capture names, sorted.
		std::vector<_capture_info> _captures; ///< First occurences of all capture groups.
		std::vector<_subroutine_transition> _subroutines; ///< All subroutine transitions.

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
		void _collect_capture_names(const ast::nodes::complex_assertion &expr) {
			_collect_capture_names(expr.expression);
		}
		/// Collects names from the conditional expression.
		void _collect_capture_names(const ast::nodes::conditional_expression &expr) {
			_collect_capture_names(expr.if_true);
			if (expr.if_false) {
				_collect_capture_names(expr.if_false);
			}
		}

		/// Finds the index of the given capture name.
		[[nodiscard]] std::optional<std::size_t> _find_capture_name(codepoint_string_view sv) const {
			auto it = std::lower_bound(_capture_names.begin(), _capture_names.end(), sv);
			if (it == _capture_names.end() || sv != *it) {
				return std::nullopt;
			}
			return it - _capture_names.begin();
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
		/// Compiles the given numbered subroutine.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::numbered_subroutine&);
		/// Compiles the given named subroutine.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::named_subroutine&);
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
		/// Compiles the given simple assertion.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::simple_assertion &ass) {
			auto trans = start.create_transition();
			trans->condition.emplace<compiled::transitions::simple_assertion>(ass);
			trans->new_state_index = end.index;
		}
		/// Compiles the given character class assertion.
		void _compile(
			compiled::state_ref start, compiled::state_ref end, const ast::nodes::character_class_assertion &ass
		) {
			auto trans = start.create_transition();
			auto &cond = trans->condition.emplace<compiled::transitions::character_class_assertion>();
			cond.char_class.ranges = ass.char_class.ranges;
			cond.char_class.is_negate = ass.char_class.is_negate;
			cond.boundary = ass.boundary;
			trans->new_state_index = end.index;
		}
		/// Compiles the given complex assertion.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::complex_assertion&);
		/// Compiles the given conditional subexpression.
		void _compile(compiled::state_ref start, compiled::state_ref end, const ast::nodes::conditional_expression&);

		/// Does nothing - <tt>DEFINE</tt>'s are handled separately.
		void _compile_condition(
			compiled::state_ref, compiled::state_ref, const ast::nodes::conditional_expression::define&
		) {
		}
		/// Compiles the given condition that checks for a numbered capture.
		void _compile_condition(
			compiled::state_ref start, compiled::state_ref end,
			const ast::nodes::conditional_expression::numbered_capture_available &cond
		) {
			auto trans = start.create_transition();
			trans->new_state_index = end.index;
			trans->condition.emplace<compiled::transitions::conditions::numbered_capture>().index = cond.index;
		}
		/// Compiles the given condition that checks for a named capature.
		void _compile_condition(
			compiled::state_ref start, compiled::state_ref end,
			const ast::nodes::conditional_expression::named_capture_available &cond
		) {
			auto trans = start.create_transition();
			trans->new_state_index = end.index;
			if (auto id = _find_capture_name(cond.name)) {
				trans->condition.emplace<compiled::transitions::conditions::named_capture>().name_index = id.value();
				return;
			}
			// check if this is a recursion
			if (cond.name.size() > 0 && cond.name[0] == U'R') {
				if (cond.name.size() > 1 && cond.name[1] == U'&') { // named
					if (auto id = _find_capture_name(codepoint_string_view(cond.name).substr(2))) {
						auto &rec = trans->condition.emplace<compiled::transitions::conditions::named_recursion>();
						rec.name_index = id.value();
					} else {
						// TODO error
					}
				} else { // numbered
					auto &rec = trans->condition.emplace<compiled::transitions::conditions::numbered_recursion>();
					if (cond.name.size() > 1) {
						// an index follows - parse it
						for (std::size_t i = 1; i < cond.name.size(); ++i) {
							if (cond.name[i] < U'0' || cond.name[i] > U'9') {
								// TODO error
								return;
							}
							rec.index = rec.index * 10 + (cond.name[i] - U'0');
						}
					}
				}
				return;
			}
			// TODO error
		}
		/// Compiles the given assertion.
		void _compile_condition(
			compiled::state_ref start, compiled::state_ref end, const ast::nodes::complex_assertion&
		);
	};
}
