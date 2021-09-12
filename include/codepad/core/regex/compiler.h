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
	namespace half_compiled {
		class state_machine;
	}


	/// Data type presets for various kinds of patterns.
	namespace data_types {
		/// Uses \p std::size_t for all index types.
		class unoptimized {
		public:
			using state_index = std::size_t; ///< Index type for states.
			using transition_index = std::size_t; ///< Index type for transitions.
		};

		/// Uses \p std::uint16_t for all index types.
		class small_expression {
		public:
			using state_index = std::uint16_t; ///< Index type for states.
			using transition_index = std::uint16_t; ///< Index type for transitions.
		};
	}


	/// Compiled components of a state machine.
	template <typename DataTypes> class compiled {
	public:
		class state_machine;

		using state_index = typename DataTypes::state_index; ///< State index type.
		using transition_index = typename DataTypes::transition_index; ///< Transition index type.

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

			/// Converts this struct into one that uses a different set of data types.
			template <
				typename OtherDataTypes
			> [[nodiscard]] typename compiled<OtherDataTypes>::named_capture_registry into() const {
				typename compiled<OtherDataTypes>::named_capture_registry result;
				result.indices = indices;
				result.start_indices = start_indices;
				result.reverse_mapping = reverse_mapping;
				return result;
			}
		};

		/// Reference to a state in an automata. This must be used in conjunction with a
		/// \ref half_compiled::state_machine or a \ref compiled::state_machine.
		struct state_ref {
			template <typename> friend class compiled;
			friend half_compiled::state_machine;
			friend state_machine;
		public:
			/// Invalid state index.
			constexpr static state_index invalid_state = std::numeric_limits<state_index>::max();

			/// Default constructor.
			state_ref() = default;

			/// Initializes this struct from a state reference that uses another type.
			template <
				typename OtherDataType
			> [[nodiscard]] typename compiled<OtherDataType>::state_ref into() const {
				return typename compiled<OtherDataType>::state_ref(static_cast<typename OtherDataType::state_index>(
					_index == invalid_state ? compiled<OtherDataType>::state_ref::invalid_state : _index
				));
			}

			/// Returns the index of this state.
			[[nodiscard]] state_index get_index() const {
				return _index;
			}

			/// Checks if this reference points to a valid state.
			[[nodiscard]] bool is_empty() const {
				return _index == invalid_state;
			}
			/// \overload
			[[nodiscard]] explicit operator bool() const {
				return !is_empty();
			}
			/// Equality.
			[[nodiscard]] friend bool operator==(state_ref, state_ref) = default;
		protected:
			state_index _index = invalid_state; ///< Index of the state.

			/// Initializes \ref _index.
			explicit state_ref(state_index i) : _index(i) {
			}
		};


		/// All transition types.
		class transitions {
		public:
			/// A literal.
			struct literal {
				codepoint_string contents; ///< Contents of this literal.
				bool case_insensitive = false; ///< Whether the literal is matched in a case-insensitive manner.

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::literal into() && {
					typename compiled<OtherDataTypes>::transitions::literal result;
					result.contents = std::move(contents);
					result.case_insensitive = case_insensitive;
					return result;
				}
			};
			/// A character class.
			struct character_class {
				codepoint_range_list ranges; ///< Ranges.
				bool is_negate = false; ///< Whether the codepoint should not match any character in this class.
				bool case_insensitive = false; ///< Whether the condition is case-insensitive.

				/// Tests whether the given codepoint is matched by this character class.
				[[nodiscard]] bool matches(codepoint cp) const {
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

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::character_class into() && {
					typename compiled<OtherDataTypes>::transitions::character_class result;
					result.ranges = std::move(ranges);
					result.is_negate = is_negate;
					result.case_insensitive = case_insensitive;
					return result;
				}
			};
			/// Simple assertion.
			struct simple_assertion {
				/// The type of this assertion.
				ast_nodes::simple_assertion::type assertion_type = ast_nodes::simple_assertion::type::always_false;

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::simple_assertion into() && {
					typename compiled<OtherDataTypes>::transitions::simple_assertion result;
					result.assertion_type = assertion_type;
					return result;
				}
			};
			/// An assertion that checks if we are at a character class boundary.
			struct character_class_assertion {
				character_class char_class; ///< The character class.
				bool boundary = false; ///< Whether we're expecting a boundary.

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::character_class_assertion into() && {
					typename compiled<OtherDataTypes>::transitions::character_class_assertion result;
					result.char_class = std::move(char_class).into<OtherDataTypes>();
					result.boundary = boundary;
					return result;
				}
			};
			/// Starts a capture.
			struct capture_begin {
				std::size_t index = 0; ///< Index of the capture.

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::capture_begin into() && {
					typename compiled<OtherDataTypes>::transitions::capture_begin result;
					result.index = index;
					return result;
				}
			};
			/// Ends a capture.
			struct capture_end {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::capture_end into() && {
					return typename compiled<OtherDataTypes>::transitions::capture_end();
				}
			};
			/// A numbered backreference.
			struct numbered_backreference {
				std::size_t index = 0; ///< Index of the capture.
				bool case_insensitive = false; ///< Whether the condition is case-insensitive.

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::numbered_backreference into() && {
					typename compiled<OtherDataTypes>::transitions::numbered_backreference result;
					result.index = index;
					result.case_insensitive = case_insensitive;
					return result;
				}
			};
			/// A named backreference.
			struct named_backreference {
				std::size_t index = 0; ///< Index of the capture.
				bool case_insensitive = false; ///< Whether the condition is case-insensitive.

				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::named_backreference into() && {
					typename compiled<OtherDataTypes>::transitions::named_backreference result;
					result.index = index;
					result.case_insensitive = case_insensitive;
					return result;
				}
			};
			/// Pushes a subroutine stack frame indicating that once \ref target is reached, jumps to
			/// \ref return_state. This should only be used to implement subroutines and recursions.
			struct jump {
				std::size_t subroutine_index = 0; ///< Index of the subroutine.
				/// The target state. Once this state is reached, matching jumps back to \ref final.
				state_ref target;
				/// The state to jump to once \ref target is reached.
				state_ref return_state;

				/// Converts to another data type.
				template <typename OtherDataTypes> typename compiled<OtherDataTypes>::transitions::jump into() && {
					typename compiled<OtherDataTypes>::transitions::jump result;
					result.subroutine_index = subroutine_index;
					result.target = target.into<OtherDataTypes>();
					result.return_state = return_state.into<OtherDataTypes>();
					return result;
				}
			};
			/// Resets the starting position of this match.
			struct reset_match_start {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::reset_match_start into() && {
					return typename compiled<OtherDataTypes>::transitions::reset_match_start();
				}
			};
			/// Pushes the start of an atomic range.
			struct push_atomic {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::push_atomic into() && {
					return typename compiled<OtherDataTypes>::transitions::push_atomic();
				}
			};
			/// Pops all states on stack pushed since the start of the last atomic range.
			struct pop_atomic {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::pop_atomic into() && {
					return typename compiled<OtherDataTypes>::transitions::pop_atomic();
				}
			};
			/// Pushes the current input stream as a checkpoint, which can be restored later using
			/// \ref restore_stream_checkpoint.
			struct push_stream_checkpoint {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::push_stream_checkpoint into() && {
					return typename compiled<OtherDataTypes>::transitions::push_stream_checkpoint();
				}
			};
			/// Restores the checkpointed stream, previously pushed using \ref push_stream_checkpoint.
			struct restore_stream_checkpoint {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::restore_stream_checkpoint into() && {
					return typename compiled<OtherDataTypes>::transitions::restore_stream_checkpoint();
				}
			};
			/// Paired with \ref infinite_loop, this transition pushes the current stream position onto a stack so
			/// that we can later check if we're stuck in an infinite loop where the body doesn't consume any
			/// characters.
			struct push_position {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::push_position into() && {
					return typename compiled<OtherDataTypes>::transitions::push_position();
				}
			};
			/// This transition can only happen if any characters have been consumed since the last
			/// \ref push_position. Regardless of whether this transition happens, it will pop the previously pushed
			/// stream position.
			struct check_infinite_loop {
				/// Converts to another data type.
				template <
					typename OtherDataTypes
				> typename compiled<OtherDataTypes>::transitions::check_infinite_loop into() && {
					return typename compiled<OtherDataTypes>::transitions::check_infinite_loop();
				}
			};
			/// Rewinds the stream back the specified number of codepoints.
			struct rewind {
				std::size_t num_codepoints = 0; ///< The number of codepoints to rewind.

				/// Converts to another data type.
				template <typename OtherDataTypes> typename compiled<OtherDataTypes>::transitions::rewind into() && {
					typename compiled<OtherDataTypes>::transitions::rewind result;
					result.num_codepoints = num_codepoints;
					return result;
				}
			};

			/// Transitions that are conditions.
			class conditions {
			public:
				/// Checks if we're currently in a recursion.
				struct numbered_recursion {
					/// Indicates that any ongoing recursion satisfies this condition.
					constexpr static std::size_t any_index = std::numeric_limits<std::size_t>::max();

					/// Index of the group to check for, or \ref any_index to indicate any recursion.
					std::size_t index = any_index;

					/// Converts to another data type.
					template <
						typename OtherDataTypes
					> typename compiled<OtherDataTypes>::transitions::conditions::numbered_recursion into() && {
						typename compiled<OtherDataTypes>::transitions::conditions::numbered_recursion result;
						result.index =
							index == any_index ?
							compiled<OtherDataTypes>::transitions::conditions::numbered_recursion::any_index :
							index;
						return result;
					}
				};
				/// Checks if we're currently in the specified named subroutine call.
				struct named_recursion {
					std::size_t name_index = 0; ///< Index of the name in \ref state_machine::named_captures.

					/// Converts to another data type.
					template <
						typename OtherDataTypes
					> typename compiled<OtherDataTypes>::transitions::conditions::named_recursion into() && {
						typename compiled<OtherDataTypes>::transitions::conditions::named_recursion result;
						result.name_index = name_index;
						return result;
					}
				};
				/// Checks if the given numbered group has been matched.
				struct numbered_capture {
					std::size_t index = 0; ///< The index of the capture.

					/// Converts to another data type.
					template <
						typename OtherDataTypes
					> typename compiled<OtherDataTypes>::transitions::conditions::numbered_capture into() && {
						typename compiled<OtherDataTypes>::transitions::conditions::numbered_capture result;
						result.index = index;
						return result;
					}
				};
				/// Checks if the given named group has been matched.
				struct named_capture {
					std::size_t name_index = 0; ///< Index of the name in \ref state_machine::named_captures.

					/// Converts to another data type.
					template <
						typename OtherDataTypes
					> typename compiled<OtherDataTypes>::transitions::conditions::named_capture into() && {
						typename compiled<OtherDataTypes>::transitions::conditions::named_capture result;
						result.name_index = name_index;
						return result;
					}
				};
			};

			/// Backtracking control verbs.
			class verbs {
			public:
				/// Fails and causes backtracking immediately.
				struct fail {
					/// Converts to another data type.
					template <typename OtherDataTypes> typename compiled<OtherDataTypes>::transitions::verbs::fail into() && {
						return typename compiled<OtherDataTypes>::transitions::verbs::fail();
					}
				};
			};
		};

		/// Stores the data of a transition.
		struct transition {
			/// A key used to determine if a transition is viable.
			using key = std::variant<
				typename transitions::literal,
				typename transitions::character_class,
				typename transitions::simple_assertion,
				typename transitions::character_class_assertion,
				typename transitions::capture_begin,
				typename transitions::capture_end,
				typename transitions::numbered_backreference,
				typename transitions::named_backreference,
				typename transitions::jump,
				typename transitions::reset_match_start,
				typename transitions::push_atomic,
				typename transitions::pop_atomic,
				typename transitions::push_stream_checkpoint,
				typename transitions::restore_stream_checkpoint,
				typename transitions::push_position,
				typename transitions::check_infinite_loop,
				typename transitions::rewind,
				typename transitions::conditions::numbered_recursion,
				typename transitions::conditions::named_recursion,
				typename transitions::conditions::numbered_capture,
				typename transitions::conditions::named_capture,
				typename transitions::verbs::fail
			>;

			key condition; ///< Condition of this transition.
			state_ref new_state; ///< The state to transition to.
		};

		/// A state in a \ref state_machine.
		struct state {
			transition_index first_transition = 0; ///< Index of the first transition associated with this state.
			/// Index past the last trnasition assocaited with this state.
			transition_index past_last_transition = 0;
		};
		/// State machine corresponding to a regular expression.
		class state_machine {
			friend half_compiled::state_machine;
		public:
			/// Returns the start state.
			[[nodiscard]] state_ref get_start_state() const {
				return _start_state;
			}
			/// Returns the end state.
			[[nodiscard]] state_ref get_end_state() const {
				return _end_state;
			}
			/// Returns the transitions associated with the given \ref state_ref.
			[[nodiscard]] std::span<const transition> get_transitions(state_ref r) const {
				const auto &s = _states[r._index];
				return std::span<const transition>(
					_transitions.begin() + s.first_transition, _transitions.begin() + s.past_last_transition
				);
			}
			/// Returns the \ref named_capture_registry.
			[[nodiscard]] const named_capture_registry &get_named_captures() const {
				return _named_captures;
			}
		protected:
			std::vector<state> _states; ///< States.
			std::vector<transition> _transitions; ///< Transitions.
			/// Mapping from named captures to regular indexed captures.
			named_capture_registry _named_captures;
			state_ref _start_state; ///< The starting state.
			state_ref _end_state; ///< The ending state.
		};
	};
	using compiled_unoptimized = compiled<data_types::unoptimized>; ///< Unoptimized compiled components.

	namespace half_compiled {
		class state_machine;

		/// A half-compiled state.
		struct state {
			std::vector<compiled_unoptimized::transition> transitions; ///< Transitions to new states.
		};

		/// Reference to a transition.
		struct transition_ref {
			friend state_machine;
		public:
			/// Default constructor.
			transition_ref() = default;
			/// Initializes this reference.
			transition_ref(compiled_unoptimized::state_ref &s, std::size_t i) : _state(s), _index(i) {
			}

			/// Tests if this transition reference is valid.
			[[nodiscard]] explicit operator bool() const {
				return !_state.is_empty();
			}
		protected:
			compiled_unoptimized::state_ref _state; ///< Reference to the state.
			std::size_t _index = 0; ///< Index of the transition.
		};

		/// A half-compiled state machine corresponding to a regular expression.
		class state_machine {
		public:
			std::deque<state> states; ///< States.
			/// Mapping from named captures to regular indexed captures.
			compiled_unoptimized::named_capture_registry named_captures;
			compiled_unoptimized::state_ref start_state; ///< The starting state.
			compiled_unoptimized::state_ref end_state; ///< The ending state.

			/// Creates a new state in this \ref state_machine and returns its index.
			[[nodiscard]] compiled_unoptimized::state_ref create_state();
			/// Creates a new transition for the given state.
			std::pair<transition_ref, compiled_unoptimized::transition&> create_transition_from_to(
				compiled_unoptimized::state_ref, compiled_unoptimized::state_ref
			);
			/// Returns the transition corresponding to the reference.
			[[nodiscard]] compiled_unoptimized::transition &get_transition(transition_ref r) {
				return states[r._state._index].transitions[r._index];
			}

			/// Condenses this state machine into a representation that's better for matching.
			template <
				typename TargetDataTypes
			> [[nodiscard]] typename compiled<TargetDataTypes>::state_machine finalize() && {
				using compiled_types = compiled<TargetDataTypes>;

				typename compiled_types::state_machine result;
				result._named_captures = named_captures.into<TargetDataTypes>();
				result._start_state = start_state.into<TargetDataTypes>();
				result._end_state = end_state.into<TargetDataTypes>();
				result._states.reserve(states.size());
				for (auto &st : states) {
					auto &res_st = result._states.emplace_back();
					res_st.first_transition = static_cast<typename TargetDataTypes::transition_index>(
						result._transitions.size()
					);
					for (auto &trans : st.transitions) {
						auto &res_trans = result._transitions.emplace_back();
						res_trans.new_state = trans.new_state.into<TargetDataTypes>();
						std::visit(
							[&](auto &&cond) {
								res_trans.condition = std::move(cond).into<TargetDataTypes>();
							},
							std::move(trans.condition)
						);
					}
					res_st.past_last_transition = static_cast<typename TargetDataTypes::transition_index>(
						result._transitions.size()
					);
				}
				return result;
			}

			/// Dumps this state macine into a DOT file.
			void dump(std::ostream&, bool valid_only = true) const;
		};
	}

	/// Regex compiler.
	class compiler {
	public:
		/// Compiles the given AST.
		[[nodiscard]] half_compiled::state_machine compile(const ast&, const ast::analysis&);
	protected:
		/// Information about a named capture.
		struct _named_capture_info {
			/// Default constructor.
			_named_capture_info() = default;
			/// Initializes all fields of this struct.
			_named_capture_info(codepoint_string n, std::size_t i) : name(std::move(n)), index(i) {
			}

			codepoint_string name; ///< The name of this capture.
			std::size_t index = 0; ///< Corresponding capture index.

			/// Default comparisons.
			friend std::strong_ordering operator<=>(
				const _named_capture_info&, const _named_capture_info&
			) = default;
		};
		/// States of a capture group.
		struct _capture_info {
			compiled_unoptimized::state_ref start; ///< The starting state.
			compiled_unoptimized::state_ref end; ///< The end state.

			/// Returns whether this slot is currently empty.
			[[nodiscard]] bool is_empty() const {
				return !start;
			}
		};
		/// Information about a subroutine transition.
		struct _subroutine_transition {
			/// Initializes all fields of this struct.
			_subroutine_transition(half_compiled::transition_ref t, std::size_t i) : transition(t), index(i) {
			}

			half_compiled::transition_ref transition; ///< The transition.
			std::size_t index = 0; ///< The group index.
		};

		half_compiled::state_machine _result; ///< The result.
		std::vector<_named_capture_info> _named_captures; ///< Information about all named captures.
		std::vector<codepoint_string> _capture_names; ///< All unique capture names, sorted.
		std::vector<_capture_info> _captures; ///< First occurences of all capture groups.
		std::vector<_subroutine_transition> _subroutines; ///< All subroutine transitions.
		compiled_unoptimized::state_ref _fail_state; ///< Fail state. This is created on-demand.
		const ast *_ast = nullptr; ///< The \ref ast that's being compiled.
		const ast::analysis *_analysis = nullptr; ///< Analysis of \ref _ast.

		/// Collects capture names from a node.
		void _collect_capture_names(const ast::node &node) {
			if (node.is<ast_nodes::subexpression>()) {
				const auto &expr = std::get<ast_nodes::subexpression>(node.value);
				if (!expr.capture_name.empty()) {
					_named_captures.emplace_back(expr.capture_name, expr.capture_index);
				}
			}
		}

		/// Returns the fail state, creating it if necessary.
		[[nodiscard]] compiled_unoptimized::state_ref _get_fail_state() {
			if (!_fail_state) {
				_fail_state = _result.create_state();
			}
			return _fail_state;
		}

		/// Finds the index of the given capture name.
		[[nodiscard]] std::optional<std::size_t> _find_capture_name(codepoint_string_view sv) const {
			auto it = std::lower_bound(_capture_names.begin(), _capture_names.end(), sv);
			if (it == _capture_names.end() || sv != *it) {
				return std::nullopt;
			}
			return it - _capture_names.begin();
		}

		/// Compiles the given \ref ast_nodes::node_ref.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end, ast_nodes::node_ref n
		) {
			std::visit(
				[&, this](const auto &node) {
					_compile(start, end, node);
				},
				_ast->get_node(n).value
			);
		}

		/// Does nothing for error nodes.
		void _compile(compiled_unoptimized::state_ref, compiled_unoptimized::state_ref, const ast_nodes::error&) {
		}
		/// Does nothing for feature nodes.
		void _compile(compiled_unoptimized::state_ref, compiled_unoptimized::state_ref, const ast_nodes::feature&) {
		}
		/// Compiles a \ref ast_nodes::match_start_override.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::match_start_override&
		) {
			auto [transition_ref, transition] = _result.create_transition_from_to(start, end);
			transition.condition.emplace<compiled_unoptimized::transitions::reset_match_start>();
		}
		/// Compiles the given literal node.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end, const ast_nodes::literal&
		);
		/// Compiles the given numbered backreference.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::numbered_backreference&
		);
		/// Compiles the given named backreference.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::named_backreference&
		);
		/// Compiles the given numbered subroutine.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::numbered_subroutine&
		);
		/// Compiles the given named subroutine.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::named_subroutine&
		);
		/// Compiles the given character class.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::character_class &char_class
		) {
			auto [transition_ref, transition] = _result.create_transition_from_to(start, end);
			auto &cls = transition.condition.emplace<compiled_unoptimized::transitions::character_class>();
			cls.ranges = char_class.ranges;
			cls.is_negate = char_class.is_negate;
			cls.case_insensitive = char_class.case_insensitive;
		}
		/// Compiles the given subexpression, starting from the given state.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::subexpression&
		);
		/// Compiles the given alternative expression.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::alternative &expr
		) {
			for (const auto &alt : expr.alternatives) {
				_compile(start, end, alt);
			}
		}
		/// Compiles the given repetition.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::repetition&
		);
		/// Compiles the given simple assertion.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::simple_assertion &ass
		) {
			auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
			auto &out_ass = trans.condition.emplace<compiled_unoptimized::transitions::simple_assertion>();
			out_ass.assertion_type = ass.assertion_type;
		}
		/// Compiles the given character class assertion.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::character_class_assertion &ass
		) {
			auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
			auto &cond = trans.condition.emplace<compiled_unoptimized::transitions::character_class_assertion>();
			cond.char_class.ranges = ass.char_class.ranges;
			cond.char_class.is_negate = ass.char_class.is_negate;
			cond.boundary = ass.boundary;
		}
		/// Compiles the given complex assertion.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::complex_assertion&
		);
		/// Compiles the given conditional subexpression.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::conditional_expression&
		);
		/// Compiles the given verb.
		void _compile(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref, const ast_nodes::verbs::fail&
		) {
			_result.create_transition_from_to(start, _get_fail_state());
		}

		/// Does nothing - <tt>DEFINE</tt>'s are handled separately.
		void _compile_condition(
			compiled_unoptimized::state_ref, compiled_unoptimized::state_ref,
			const ast_nodes::conditional_expression::define&
		) {
		}
		/// Compiles the given condition that checks for a numbered capture.
		void _compile_condition(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::conditional_expression::numbered_capture_available &cond
		) {
			auto [trans_ref, trans] = _result.create_transition_from_to(start, end);
			trans.condition.emplace<
				compiled_unoptimized::transitions::conditions::numbered_capture
			>().index = cond.index;
		}
		/// Compiles the given condition that checks for a named capature.
		void _compile_condition(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::conditional_expression::named_capture_available&
		);
		/// Compiles the given assertion.
		void _compile_condition(
			compiled_unoptimized::state_ref start, compiled_unoptimized::state_ref end,
			const ast_nodes::conditional_expression::complex_assertion&
		);
	};
}
