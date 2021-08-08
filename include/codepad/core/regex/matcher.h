// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Used to search regex state machines.

#include <optional>
#include <format>

#include "compiler.h"

namespace codepad::regex {
	/// Does not log internal running data.
	struct no_log {
	};
	/// A match result.
	template <typename Stream> struct match_result {
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
	/// Regular expression matcher.
	template <typename Stream, typename LogFunc = no_log> class matcher {
	public:
		using result = match_result<Stream>; ///< Result type.

		/// Default constructor.
		matcher() = default;
		/// Initializes \ref debug_log.
		template <typename ...Args> explicit matcher(Args &&...f) : debug_log(std::forward<Args>(f)...) {
		}

		/// Tests whether there is a match starting from the current position of the input stream.
		///
		/// \return The state of the state machine after it's been executed from this position.
		[[nodiscard]] std::optional<result> try_match(
			Stream&, const compiled::state_machine&, std::size_t max_iters = 1000000
		);

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
	
		[[no_unique_address]] LogFunc debug_log; ///< Used to log debug information.
	protected:
		constexpr static bool _uses_log = !std::is_same_v<LogFunc, no_log>; ///< Indicates whether logging is enabled.

		/// Information about an ongoing capture.
		struct _capture_info {
			/// Default constructor.
			_capture_info() = default;
			/// Initializes all fields of this struct.
			_capture_info(Stream s, std::size_t id) : begin(std::move(s)), index(id) {
			}

			Stream begin; ///< State of the input stream at the beginning of this capture.
			std::size_t index = 0; ///< The index of this capture.
		};
		/// Information about a finished capture that needs to be reset back to the previous value during
		/// backtracking.
		struct _finished_capture_info {
			/// Default constructor.
			_finished_capture_info() = default;
			/// Initializes all fields of this struct.
			_finished_capture_info(result::capture cap, std::size_t id) :
				capture_data(std::move(cap)), index(id) {
			}

			result::capture capture_data; ///< Capture data.
			std::size_t index = 0; ///< The index of this capture.
		};
		/// Information about a partially finished capture that needs to restart after backtracking.
		struct _partial_finished_capture_info {
			/// Default constructor.
			_partial_finished_capture_info() = default;
			/// Initializes all fields of this struct.
			_partial_finished_capture_info(_finished_capture_info cap, Stream beg) :
				capture(std::move(cap)), begin(std::move(beg)) {
			}

			_finished_capture_info capture; ///< Information about the starting of this capture.
			Stream begin; ///< Beginning position of the ongoing capture when the state was pushed.
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
			std::stack<_finished_capture_info> finished_captures;
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
		/// Size of \ref _state_stack at the beginning of each ongoing atomic group.
		std::stack<std::size_t> _atomic_stack_sizes;
		const compiled::state_machine *_expr = nullptr; ///< State machine for the expression.

		/// Logs the given string.
		void _log([[maybe_unused]] std::u8string_view str) {
			if constexpr (_uses_log) {
				debug_log << str;
			}
		}
		/// Logs information about the given state.
		void _log([[maybe_unused]] const _state&, [[maybe_unused]] std::u8string_view indent);

		/// Finds the index of the first matched group that has the given name.
		[[nodiscard]] std::optional<std::size_t> _find_matched_named_capture(std::size_t name_index) const {
			for (std::size_t id : _expr->named_captures.get_indices_for_name(name_index)) {
				if (id >= _result.captures.size()) {
					// since the captures are ordered, no captures after this one can be matched
					break;
				}
				if (_result.captures[id].is_valid()) {
					return id;
				}
			}
			return std::nullopt; // capture not yet matched
		}

		/// Checks if a backreference that has already been matched matches the string starting from the current
		/// position.
		[[nodiscard]] bool _check_backreference_transition(Stream&, std::size_t index, bool case_insensitive) const;

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
			Stream &stream, const compiled::transition &trans, const compiled::transitions::character_class &cond
		) const {
			if (stream.empty()) {
				return false;
			}
			return cond.matches(stream.take(), trans.case_insensitive);
		}
		/// Checks if the assertion is satisfied.
		[[nodiscard]] bool _check_transition(
			Stream stream, const compiled::transition &trans, const compiled::transitions::assertion &cond
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
					auto &boundary = std::get<compiled::transitions::character_class>(
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
					auto &boundary = std::get<compiled::transitions::character_class>(
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
		/// Checks if a numbered backreference matches.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition &trans,
			const compiled::transitions::numbered_backreference &cond
		) {
			if (cond.index >= _result.captures.size() || !_result.captures[cond.index].is_valid()) {
				return false; // capture not yet matched
			}
			return _check_backreference_transition(stream, cond.index, trans.case_insensitive);
		}
		/// Checks if a named backreference matches.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition &trans,
			const compiled::transitions::named_backreference &cond
		) {
			if (auto id = _find_matched_named_capture(cond.index)) {
				return _check_backreference_transition(stream, id.value(), trans.case_insensitive);
			}
			return false;
		}
		/// The capture group is started later in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			Stream, const compiled::transition&, const compiled::transitions::capture_begin&
		) {
			return true;
		}
		/// The capture group is finished later in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&, const compiled::transitions::capture_end&
		) {
			return true;
		}
		/// Resets the start of this match.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const compiled::transition&, const compiled::transitions::reset_match_start&
		) {
			// do this before a state is potentially pushed for backtracking
			if (!_state_stack.empty()) {
				_state_stack.top().initial_match_begin = _result.overriden_match_begin;
			}
			_result.overriden_match_begin = std::move(stream);
			return true;
		}
		/// Atomic groups are handled in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&, const compiled::transitions::push_atomic&
		) {
			return true;
		}
		/// Atomic groups are handled in \ref _execute_transition().
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&, const compiled::transitions::pop_atomic&
		) {
			return true;
		}
		/// Checks if the given numbered group has been matched.
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&,
			const compiled::transitions::conditions::numbered_capture &cap
		) {
			return _result.captures.size() > cap.index && _result.captures[cap.index].is_valid();
		}
		/// Checks if the given numbered group has been matched.
		[[nodiscard]] bool _check_transition(
			const Stream&, const compiled::transition&,
			const compiled::transitions::conditions::named_capture &cap
		) {
			return _find_matched_named_capture(cap.name_index).has_value();
		}

		/// Adds a capture that results from an assertion.
		void _add_capture(std::size_t index, result::capture cap) {
			if (!cap.is_valid()) {
				return;
			}
			if (_result.captures.size() <= index) {
				_result.captures.resize(index + 1);
			}
			if (!_state_stack.empty()) { // TODO we should add these after the state has been pushed
				_state_stack.top().finished_captures.emplace(cap, index);
			}
			_result.captures[index] = std::move(cap);
		}

		/// By default nothing extra needs to be done for transitions.
		template <typename Trans> void _execute_transition(const Stream&, const Trans&) {
		}
		/// Starts a capture.
		void _execute_transition(Stream stream, const compiled::transitions::capture_begin &beg) {
			_ongoing_captures.emplace(std::move(stream), beg.index);
		}
		/// Ends a capture.
		void _execute_transition(const Stream&, const compiled::transitions::capture_end&);
		/// Pushes an atomic group.
		void _execute_transition(const Stream&, const compiled::transitions::push_atomic&) {
			_atomic_stack_sizes.emplace(_state_stack.size());
		}
		/// Pops all states associated with the current atomic group.
		void _execute_transition(const Stream&, const compiled::transitions::pop_atomic&) {
			while (_state_stack.size() > _atomic_stack_sizes.top()) {
				_state_stack.pop();
			}
			_atomic_stack_sizes.pop();
		}
	};
}
