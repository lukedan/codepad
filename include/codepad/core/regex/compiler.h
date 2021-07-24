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
			std::size_t create_state(bool is_atomic);

			/// Dumps this state macine into a DOT file.
			void dump(std::ostream&, bool valid_only = true) const;
		};

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
		/// A backreference.
		struct backreference {
			/// Index of the capture.
			///
			/// \sa is_named
			std::size_t index = 0;
			bool is_named = false; ///< Indicates whether this references a named capture.
		};
		/// Resets the starting position of this match.
		struct reset_match_start {
		};

		/// Stores the data of a transition.
		struct transition {
			/// A key used to determine if a transition is viable.
			using key = std::variant<
				codepoint_string,
				character_class,
				assertion,
				capture_begin,
				capture_end,
				backreference,
				reset_match_start
			>;

			key condition; ///< Condition of this transition.
			std::size_t new_state_index = 0; ///< Index of the state to transition to.
			bool case_insensitive = false; ///< Whether the condition is case-insensitive.
		};
		/// A state.
		struct state {
			std::vector<transition> transitions; ///< Transitions to new states.
			bool is_atomic = false; ///< Whether or not to disable backtracking for this node.
		};
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

			_collect_capture_names(expr);
			std::sort(_named_captures.begin(), _named_captures.end());
			// collect named capture info
			codepoint_string last_name;
			for (auto &cap : _named_captures) {
				if (cap.name != last_name) {
					_result.named_captures.start_indices.emplace_back(_result.named_captures.indices.size());
					_capture_names.emplace_back(std::exchange(last_name, std::move(cap.name)));
				}
				_result.named_captures.indices.emplace_back(cap.index);
			}
			_result.named_captures.start_indices.emplace_back(_result.named_captures.indices.size());
			_capture_names.emplace_back(std::move(last_name));

			_compile(_result.start_state, _result.end_state, expr);

			assert_true_logical(_atomic_counter == 0, "atomic counter has not been properly reset");
			return std::move(_result);
		}
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
		/// All unique capture names, sorted. Due to how they're computed, there's an empty element at the beginning,
		/// which should be ignored.
		std::vector<codepoint_string> _capture_names;
		std::size_t _atomic_counter = 0; ///< Counts how many nested atomic groups we're currently in.

		/// Creates a new state in \ref _result using \ref compiled::state_machine::create_state(), determining
		/// whether it's atomic using \ref _atomic_counter.
		std::size_t _create_state() {
			return _result.create_state(_atomic_counter > 0);
		}

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
		void _compile(std::size_t, std::size_t, const ast::nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(std::size_t, std::size_t, const ast::nodes::feature&) {
		}
		/// Compiles a \ref ast::nodes::match_start_override.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::match_start_override&) {
			auto &transition = _result.states[start].transitions.emplace_back();
			transition.condition.emplace<compiled::reset_match_start>();
			transition.new_state_index = end;
		}
		/// Compiles the given literal node.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::literal&);
		/// Compiles the given backreference.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::backreference&);
		/// Compiles the given character class.
		void _compile(std::size_t start, std::size_t end, const ast::nodes::character_class &char_class) {
			auto &transition = _result.states[start].transitions.emplace_back();
			auto &cls = transition.condition.emplace<compiled::character_class>();
			cls.ranges = char_class.ranges;
			cls.is_negate = char_class.is_negate;
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
