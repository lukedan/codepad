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
		/// A match result.
		struct result {
			/// Position information about a capture in a match.
			struct capture {
				/// Used to mark that this capture is not in the input.
				constexpr static std::size_t invalid_capture_length = std::numeric_limits<std::size_t>::max();

				/// Default constructor.
				capture() = default;
				/// Initializes all fields of this structure.
				capture(Stream beg, std::size_t len) : begin(std::move(beg)), length(len) {
				}

				/// The state of the stream at the beginning of this capture.
				Stream begin;
				/// The length of this capture.
				std::size_t length = invalid_capture_length;

				/// Returns whether the capture is valid.
				[[nodiscard]] bool is_valid() const {
					return length != invalid_capture_length;
				}
			};

			/// Captures in the match. The 0-th element will be the whole match, without taking
			/// \ref overriden_match_begin into account.
			std::vector<capture> captures;
			std::optional<Stream> overriden_match_begin; ///< Match beginning position overriden using \p \\K.
		};

		/// Tests whether there is a match starting from the current position of the input stream.
		///
		/// \return The state of the state machine after it's been executed from this position.
		[[nodiscard]] std::optional<result> try_match(
			Stream &stream, const compiled::state_machine &expr, std::size_t max_iters = 1000000
		) {
			_result = result();
			_result.captures.emplace_back().begin = stream;
			_expr = &expr;

			_state current_state(std::move(stream), expr.start_state, 0, 0, std::nullopt);
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
						_state_stack.emplace(
							std::move(checkpoint_stream), current_state.automata_state, current_state.transition,
							_ongoing_captures.size(), _result.overriden_match_begin
						);
					}
					std::visit(
						[&, this](auto &&trans) {
							_execute_transition(current_state.stream, trans);
						},
						transition.condition
					);
					current_state.automata_state = transition.new_state_index;
					current_state.transition = 0;
					// continue to the next iteration
				} else {
					if (current_state.transition < current_state.get_automata_state(expr).transitions.size()) {
						current_state.stream = std::move(checkpoint_stream);
						continue; // try the next transition
					}
					// otherwise backtrack
					if (_state_stack.empty()) {
						current_state.stream = std::move(checkpoint_stream);
						break;
					}
					// update state
					current_state = std::move(_state_stack.top());
					_state_stack.pop();
					// restore _ongoing_captures to the state we're backtracking to
					std::size_t count =
						current_state.initial_ongoing_captures - current_state.partial_finished_captures.size();
					while (_ongoing_captures.size() > count) {
						_ongoing_captures.pop();
					}
					// restore _result.captures
					while (!current_state.full_finished_captures.empty()) {
						auto &cap = current_state.full_finished_captures.top();
						_result.captures[cap.index] = std::move(cap.capture_data);
						current_state.full_finished_captures.pop();
					}
					while (!current_state.partial_finished_captures.empty()) {
						_ongoing_captures.emplace(std::move(current_state.partial_finished_captures.top().capture));
						_result.captures[_ongoing_captures.top().index] = std::move(
							current_state.partial_finished_captures.top().prev_capture_data
						);
						current_state.partial_finished_captures.pop();
					}
					// restore _result.overriden_match_begin
					_result.overriden_match_begin = current_state.initial_match_begin;
				}
			}
			stream = std::move(current_state.stream);

			_state_stack = std::stack<_state>();
			_ongoing_captures = std::stack<_capture_info>();
			_expr = nullptr;

			if (current_state.automata_state == expr.end_state) {
				_result.captures.front().length =
					stream.codepoint_position() - _result.captures.front().begin.codepoint_position();
				return std::move(_result);
			}
			return std::nullopt;
		}

		/// Finds the starting point of the next match in the input stream. After the function returns, \p s will be
		/// at the end of the input if no match is found, or be at the end position of the match. The caller needs to pay
		/// extra attention when handling empty input streams; use \ref find_all() when necessary.
		///
		/// \return Starting position of the match.
		[[nodiscard]] std::optional<result> find_next(Stream &s, const compiled::state_machine &expr) {
			while (true) {
				Stream temp = s;
				if (auto res = try_match(temp, expr)) {
					if (!temp.empty() && s.codepoint_position() == temp.codepoint_position()) {
						temp.take();
					}
					s = std::move(temp);
					return std::move(res.value());
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
			using _return_type = std::invoke_result_t<std::decay_t<Callback&&>, result>;

			while (true) {
				if (auto x = find_next(s, expr)) {
					if constexpr (std::is_same_v<_return_type, bool>) {
						if (!cb(std::move(x.value()))) {
							break;
						}
					} else {
						cb(std::move(x.value()));
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
		/// Information about an ongoing capture.
		struct _capture_info {
			/// Default constructor.
			_capture_info() = default;
			/// Initializes all fields of this struct.
			_capture_info(Stream s, std::size_t id) : stream_begin(std::move(s)), index(id) {
			}

			Stream stream_begin; ///< State of the input stream at the beginning of this capture.
			std::size_t index = 0; ///< The index of this capture.
		};
		/// Information about a finished capture that needs to restart during backtracking.
		struct _partial_finished_capture_info {
			/// Default constructor.
			_partial_finished_capture_info() = default;
			/// Initializes all fields of this struct.
			_partial_finished_capture_info(_capture_info cap, result::capture prev_cap_data, std::size_t len) :
				capture(std::move(cap)), prev_capture_data(std::move(prev_cap_data)), length(len) {
			}

			_capture_info capture; ///< Information about the starting of this capture.
			result::capture prev_capture_data; ///< Capture data before this capture was previously started.
			std::size_t length = 0; ///< The length of this capture.
		};
		/// Information about a finished capture that needs to be reset back to the previous value during
		/// backtracking.
		struct _full_finished_capture_info {
			/// Default constructor.
			_full_finished_capture_info() = default;
			/// Initializes all fields of this struct.
			_full_finished_capture_info(result::capture cap, std::size_t id) :
				capture_data(std::move(cap)), index(id) {
			}

			result::capture capture_data; ///< Capture data.
			std::size_t index = 0; ///< The index of this capture.
		};
		/// The state of the automata at one moment.
		struct _state {
			/// Default constructor.
			_state() = default;
			/// Initializes all fields of this struct.
			_state(
				Stream str, std::size_t st, std::size_t trans,
				std::size_t ongoing_captures, std::optional<Stream> overriden_start
			) :
				stream(std::move(str)), automata_state(st), transition(trans),
				initial_ongoing_captures(ongoing_captures), initial_match_begin(std::move(overriden_start)) {
			}

			Stream stream; ///< State of the input stream.
			std::size_t automata_state = 0; ///< Current state in the automata.
			std::size_t transition = 0; ///< Index of the current transition in \ref automata_state.

			/// The number of captures that was ongoing before this state was pushed onto the stack.
			std::size_t initial_ongoing_captures = 0;
			/// The stack of captures that started before this state was pushed, and ended after this state was
			/// pushed, but before the next state was pushed.
			std::stack<_partial_finished_capture_info> partial_finished_captures;
			/// All captures that started and finished after this state was pushed but before the next state was
			/// pushed.
			std::stack<_full_finished_capture_info> full_finished_captures;
			/// Overriden match starting position before this state was pushed onto the stack.
			std::optional<Stream> initial_match_begin;

			/// Returns the associated \ref state_machine::state.
			[[nodiscard]] const compiled::state &get_automata_state(const compiled::state_machine &sm) const {
				return sm.states[automata_state];
			}
			/// Returns the associated \ref state_machine::transition.
			[[nodiscard]] const compiled::transition &get_transition(const compiled::state_machine &sm) const {
				return sm.states[automata_state].transitions[transition];
			}
		};

		result _result; ///< Cached match result.
		std::stack<_state> _state_stack; ///< States for backtracking.
		std::stack<_capture_info> _ongoing_captures; ///< Ongoing captures.
		const compiled::state_machine *_expr = nullptr; ///< State machine for the expression.

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
			Stream &stream, const compiled::transition &trans, const compiled::character_class &cond
		) const {
			if (stream.empty()) {
				return false;
			}
			return cond.matches(stream.take(), trans.case_insensitive);
		}
		/// Checks if the assertion is satisfied.
		[[nodiscard]] bool _check_transition(
			Stream stream, const compiled::transition &trans, const compiled::assertion &cond
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
					auto &boundary = std::get<compiled::character_class>(
						cond.expression.states[0].transitions[0].condition
					);
					bool prev_in =
						stream.prev_empty() ? false : boundary.matches(stream.peek_prev(), trans.case_insensitive);
					bool next_in =
						stream.empty() ? false : boundary.matches(stream.peek(), trans.case_insensitive);
					return prev_in != next_in;
				}

			case ast::nodes::assertion::type::character_class_nonboundary:
				{
					auto &boundary = std::get<compiled::character_class>(
						cond.expression.states[0].transitions[0].condition
					);
					bool prev_in =
						stream.prev_empty() ? false : boundary.matches(stream.peek_prev(), trans.case_insensitive);
					bool next_in =
						stream.empty() ? false : boundary.matches(stream.peek(), trans.case_insensitive);
					return prev_in == next_in;
				}

			case ast::nodes::assertion::type::positive_lookahead:
				{
					matcher<Stream> assertion_matcher;
					if (auto match = assertion_matcher.try_match(stream, cond.expression)) {
						for (std::size_t i = 1; i < match->captures.size(); ++i) {
							_add_capture(i, std::move(match->captures[i]));
						}
						return true;
					}
					return false;
				}
			case ast::nodes::assertion::type::negative_lookahead:
				{
					matcher<Stream> assertion_matcher;
					if (auto match = assertion_matcher.try_match(stream, cond.expression)) {
						for (std::size_t i = 1; i < match->captures.size(); ++i) {
							_add_capture(i, std::move(match->captures[i]));
						}
						return false;
					}
					return true;
				}
			case ast::nodes::assertion::type::positive_lookbehind:
				return false; // TODO
			case ast::nodes::assertion::type::negative_lookbehind:
				return false; // TODO
			}

			assert_true_logical(false, "invalid assertion type"); // should not happen
			return false;
		}
		/// Checks if a backreference matches.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition &trans, const compiled::backreference &cond
		) {
			std::size_t index = cond.index;
			if (cond.is_named) {
				bool has_valid_index = false;
				for (std::size_t id : _expr->named_captures.get_indices_for_name(cond.index)) {
					if (id >= _result.captures.size()) {
						return false; // capture not yet matched
					}
					if (_result.captures[id].is_valid()) {
						has_valid_index = true;
						index = id;
						break;
					}
				}
				if (!has_valid_index) {
					return false; // capture not yet matched
				}
			} else {
				if (index >= _result.captures.size() || !_result.captures[index].is_valid()) {
					return false; // capture not yet matched
				}
			}

			auto &cap = _result.captures[index];
			Stream new_stream = stream;
			Stream target_stream = cap.begin;
			for (std::size_t i = 0; i < cap.length; ++i) {
				if (new_stream.empty()) {
					return false;
				}
				codepoint got = new_stream.take();
				codepoint expected = target_stream.take();
				if (trans.case_insensitive) {
					got = unicode::case_folding::get_cached().fold_simple(got);
					expected = unicode::case_folding::get_cached().fold_simple(expected);
				}
				if (got != expected) {
					return false;
				}
			}
			stream = std::move(new_stream);

			return true;
		}
		/// The capture group is started later in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			Stream, const compiled::transition&, const compiled::capture_begin&
		) {
			return true;
		}
		/// The capture group is finished later in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&, const compiled::capture_end&
		) {
			return true;
		}
		/// Resets the start of this match.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition&, const compiled::reset_match_start&
		) {
			// do this before a state is potentially pushed for backtracking
			if (!_state_stack.empty()) {
				_state_stack.top().initial_match_begin = _result.overriden_match_begin;
			}
			_result.overriden_match_begin = std::move(stream);
			return true;
		}


		/// Adds a capture that results from an assertion.
		void _add_capture(std::size_t index, result::capture cap) {
			if (!cap.is_valid()) {
				return;
			}
			if (_result.captures.size() <= index) {
				_result.captures.resize(index + 1);
			}
			if (!_state_stack.empty()) {
				_state_stack.top().full_finished_captures.emplace(cap, index);
			}
			_result.captures[index] = std::move(cap);
		}

		/// By default nothing extra needs to be done for transitions.
		template <typename Trans> void _execute_transition(const Stream&, const Trans&) {
		}
		/// Starts a capture.
		void _execute_transition(Stream stream, const compiled::capture_begin &beg) {
			_ongoing_captures.emplace(std::move(stream), beg.index);
		}
		/// Ends a capture.
		void _execute_transition(const Stream &stream, const compiled::capture_end&) {
			auto res = std::move(_ongoing_captures.top());
			_ongoing_captures.pop();
			if (_result.captures.size() <= res.index) {
				_result.captures.resize(res.index + 1);
			}

			if (!_state_stack.empty()) {
				// if necessary, record that this capture has finished
				auto &st = _state_stack.top();
				if (
					_ongoing_captures.size() + st.partial_finished_captures.size() <=
					st.initial_ongoing_captures
				) {
					// when backtracking to the state, it's necessary to restore _ongoing_captures to this state
					st.partial_finished_captures.emplace(
						res, _result.captures[res.index],
						stream.codepoint_position() - res.stream_begin.codepoint_position()
					);
				} else {
					// when backtracking, it's necessary to completely reset this capture to the previous state
					st.full_finished_captures.emplace(_result.captures[res.index], res.index);
				}
			}

			std::size_t index = res.index;
			_result.captures[index].length = stream.codepoint_position() - res.stream_begin.codepoint_position();
			_result.captures[index].begin = std::move(res.stream_begin);
		}
	};
}
