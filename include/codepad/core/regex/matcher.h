// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Used to search regex state machines.

#include <optional>

#include "compiler.h"

namespace codepad::regex {
	/// Regular expression matcher.
	template <typename Stream> class matcher {
	public:
		/// Tests whether there is a match starting from the current position of the input stream.
		///
		/// \return The state of the state machine after it's been executed from this position.
		[[nodiscard]] std::size_t try_match(Stream &stream, const compiled::state_machine &expr, std::size_t max_iters = 1000000) {
			std::stack<_state> state_stack;
			_state current_state(std::move(stream), expr.start_state, 0);
			for (std::size_t i = 0; i < max_iters; ++i) {
				if (current_state.automata_state == expr.end_state) {
					break;
				}
				Stream checkpoint_stream = current_state.stream;
				if (current_state.get_automata_state(expr).transitions.size() <= current_state.transition) {
					break;
				}
				const auto &transition = current_state.get_transition(expr);
				bool transition_ok = std::visit(
					[&, this](auto &&cond) {
						return _check_transition(current_state.stream, transition, cond);
					},
					transition.condition
				);

				++current_state.transition;
				if (transition_ok) {
					if (
						current_state.transition < current_state.get_automata_state(expr).transitions.size() &&
						!current_state.get_automata_state(expr).is_atomic
					) {
						state_stack.emplace(
							std::move(checkpoint_stream), current_state.automata_state, current_state.transition
						);
					}
					current_state.automata_state = transition.new_state_index;
					current_state.transition = 0;
					// continue to the next iteration
				} else {
					if (current_state.transition < current_state.get_automata_state(expr).transitions.size()) {
						current_state.stream = std::move(checkpoint_stream);
						continue; // try the next transition
					}
					// otherwise backtrack
					if (state_stack.empty()) {
						current_state.stream = std::move(checkpoint_stream);
						break;
					}
					current_state = std::move(state_stack.top());
					state_stack.pop();
				}
			}
			stream = std::move(current_state.stream);
			return current_state.automata_state;
		}

		/// Tests if the given expression matches the full input.
		[[nodiscard]] bool match(Stream s, const compiled::state_machine &expr) {
			std::size_t end_state = try_match(s, expr);
			return end_state == expr.end_state && s.empty();
		}
		/// Finds the starting point of the next match in the input stream. After the function returns, \p s will be
		/// at the end of the input if no match is found, or be at the end position of the match. The caller needs to pay
		/// extra attention when handling empty input streams; use \ref find_all() when necessary.
		///
		/// \return Starting position of the match.
		[[nodiscard]] std::optional<std::size_t> find_next(Stream &s, const compiled::state_machine &expr) {
			while (true) {
				Stream temp = s;
				std::size_t state = try_match(temp, expr);
				if (state == expr.end_state) {
					if (!s.empty() && s.position() == temp.position()) {
						temp.take();
					}
					std::size_t start_pos = s.position();
					s = std::move(temp);
					return start_pos;
				}
				if (s.empty()) {
					break;
				}
				s.take();
			}
			return std::nullopt;
		}
		/// Finds all matches in the given stream, calling the given callback for each match. This is written so that
		/// empty input streams can be handled correctly.
		template <typename Callback> void find_all(Stream &s, const compiled::state_machine &expr, Callback &&cb) {
			using _return_type = std::invoke_result_t<std::decay_t<Callback&&>, std::size_t>;

			while (true) {
				if (auto x = find_next(s, expr)) {
					if constexpr (std::is_same_v<_return_type, bool>) {
						if (!cb(x.value())) {
							break;
						}
					} else {
						cb(x.value());
					}
					if (s.empty()) {
						break;
					}
				} else {
					break;
				}
			}
		}
	protected:
		/// The state of the automata at one moment.
		struct _state {
			/// Default constructor.
			_state() = default;
			/// Initializes all fields of this struct.
			_state(Stream str, std::size_t st, std::size_t trans) :
				stream(std::move(str)), automata_state(st), transition(trans) {
			}

			Stream stream; ///< State of the input stream.
			std::size_t automata_state = 0; ///< Current state in the automata.
			std::size_t transition = 0; ///< Index of the current transition in \ref automata_state.

			/// Returns the associated \ref state_machine::state.
			[[nodiscard]] const compiled::state &get_automata_state(const compiled::state_machine &sm) const {
				return sm.states[automata_state];
			}
			/// Returns the associated \ref state_machine::transition.
			[[nodiscard]] const compiled::transition &get_transition(const compiled::state_machine &sm) const {
				return sm.states[automata_state].transitions[transition];
			}
		};

		/// Checks if the contents in the given stream starts with the given string.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition &trans, const codepoint_string &cond
		) const {
			for (codepoint cp : cond) {
				if (stream.empty()) {
					return false;
				}
				codepoint got_codepoint = stream.take();
				if (trans.case_insensitive) {
					got_codepoint = unicode::case_folding::get_cached().fold_simple(got_codepoint);
				}
				if (got_codepoint != cp) {
					return false;
				}
			}
			return true;
		}
		/// Checks if the next character in the stream is in the given codepoint ranges.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition &trans, const codepoint_range_list &cond
		) const {
			if (stream.empty()) {
				return false;
			}
			codepoint cp = stream.take();
			if (cond.contains(cp)) {
				return true;
			}
			if (trans.case_insensitive) {
				if (cond.contains(unicode::case_folding::get_cached().fold_simple(cp))) {
					return true;
				}
			}
			return false;
		}
		/// Checks if the assertion is satisfied.
		[[nodiscard]] bool _check_transition(
			Stream stream, const compiled::transition&, const compiled::assertion &cond
		) {
			switch (cond.assertion_type) {
			case ast::nodes::assertion::type::always_false:
				return false;

			case ast::nodes::assertion::type::line_start:
				{
					if (stream.prev_empty()) {
						return true;
					}
					auto rev_stream = make_reverse_stream(stream);
					if (consume_line_ending(rev_stream) != line_ending::none) {
						return !is_within_crlf(stream);
					}
					return false;
				}

			case ast::nodes::assertion::type::line_end:
				{
					if (stream.empty()) {
						return true;
					}
					Stream checkpoint = stream;
					if (consume_line_ending(stream) != line_ending::none) {
						return !is_within_crlf(checkpoint);
					}
					return false;
				}

			case ast::nodes::assertion::type::subject_start:
				return stream.prev_empty();

			case ast::nodes::assertion::type::subject_end_or_trailing_newline:
				{
					if (stream.empty()) {
						return true;
					}
					Stream checkpoint = stream;
					consume_line_ending(stream);
					if (stream.empty()) {
						return !is_within_crlf(checkpoint);
					}
					return false;
				}

			case ast::nodes::assertion::type::subject_end:
				return stream.empty();
			case ast::nodes::assertion::type::range_start:
				return false; // TODO

			case ast::nodes::assertion::type::character_class_boundary:
				{
					auto &boundary = std::get<codepoint_range_list>(
						cond.expression.states[0].transitions[0].condition
					);
					bool prev_in = stream.prev_empty() ? false : boundary.contains(stream.peek_prev());
					bool next_in = stream.empty() ? false : boundary.contains(stream.peek());
					return prev_in != next_in;
				}

			case ast::nodes::assertion::type::character_class_nonboundary:
				{
					auto &boundary = std::get<codepoint_range_list>(
						cond.expression.states[0].transitions[0].condition
						);
					bool prev_in = stream.prev_empty() ? false : boundary.contains(stream.peek_prev());
					bool next_in = stream.empty() ? false : boundary.contains(stream.peek());
					return prev_in == next_in;
				}

			case ast::nodes::assertion::type::positive_lookahead:
				{
					matcher<Stream> assertion_matcher;
					std::size_t state = assertion_matcher.try_match(stream, cond.expression);
					return state == cond.expression.end_state;
				}
			case ast::nodes::assertion::type::negative_lookahead:
				{
					matcher<Stream> assertion_matcher;
					std::size_t state = assertion_matcher.try_match(stream, cond.expression);
					return state != cond.expression.end_state;
				}
			case ast::nodes::assertion::type::positive_lookbehind:
				return false; // TODO
			case ast::nodes::assertion::type::negative_lookbehind:
				return false; // TODO
			}

			assert_true_logical(false, "invalid assertion type"); // should not happen
			return false;
		}
	};
}
