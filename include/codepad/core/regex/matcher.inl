// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once
#include "matcher.h"

/// \file
/// Implementation of the matcher.

namespace codepad::regex {
	template <
		typename Stream, typename LogFunc
	> std::optional<match_result<Stream>> matcher<Stream, LogFunc>::try_match(
		Stream &stream, const compiled::state_machine &expr, std::size_t max_iters
	) {
		_result = result();
		_result.captures.emplace_back().begin = stream;
		_expr = &expr;

		_state current_state(std::move(stream), expr.start_state, 0, 0, std::nullopt);
		for (std::size_t i = 0; i < max_iters; ++i) {
			if constexpr (_uses_log) {
				debug_log << u8"\nIteration " << i << u8"\n";
					
				debug_log << u8"\tCaptures:\n";
				for (std::size_t j = 0; j < _result.captures.size(); ++j) {
					debug_log <<
						u8"\t\tPosition: " << _result.captures[j].begin.codepoint_position() <<
						u8",  length: " << _result.captures[j].length << u8"\n";
				}

				debug_log << u8"\tOngoing captures:\n";
				auto caps_copy = _ongoing_captures;
				while (!caps_copy.empty()) {
					const auto &cap = caps_copy.top();
					debug_log <<
						u8"\t\t" << caps_copy.size() <<
						u8": #" << cap.index <<
						u8",  begin: " << cap.begin.codepoint_position() << u8"\n";
					caps_copy.pop();
				}

				_log(current_state, u8"\t");

				debug_log << u8"\tState stack:\n";
				auto states_copy = _state_stack;
				while (!states_copy.empty()) {
					_log(states_copy.back(), u8"\t\t");
					states_copy.pop_back();
				}
			}
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
				_log(u8"\tTransition OK\n");
				if (
					current_state.transition < current_state.get_automata_state(expr).transitions.size() &&
					!current_state.get_automata_state(expr).no_backtracking
				) {
					_log(u8"\t\tPushing state\n");
					_state_stack.emplace_back(
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
					_log(u8"\tTransition fail: next transition\n");
					current_state.stream = std::move(checkpoint_stream);
					continue; // try the next transition
				}
				// otherwise backtrack
				_log(u8"\tTransition fail: backtracking\n");
				if (_state_stack.empty()) {
					current_state.stream = std::move(checkpoint_stream);
					break;
				}
				// update state
				current_state = std::move(_state_stack.back());
				_state_stack.pop_back();
				// pop atomic groups
				while (!_atomic_stack_sizes.empty() && _state_stack.size() < _atomic_stack_sizes.top()) {
					_atomic_stack_sizes.pop();
				}
				// restore _ongoing_captures to the state we're backtracking to
				std::size_t count =
					current_state.initial_ongoing_captures - current_state.partial_finished_captures.size();
				while (_ongoing_captures.size() > count) {
					_ongoing_captures.pop();
				}
				// restore _result.captures
				while (!current_state.finished_captures.empty()) {
					auto &cap = current_state.finished_captures.back();
					_result.captures[cap.index] = std::move(cap.capture_data);
					current_state.finished_captures.pop_back();
				}
				while (!current_state.partial_finished_captures.empty()) {
					const auto &cap = current_state.partial_finished_captures.back();
					_ongoing_captures.emplace(std::move(cap.begin), cap.capture.index);
					_result.captures[_ongoing_captures.top().index] = std::move(cap.capture.capture_data);
					current_state.partial_finished_captures.pop_back();
				}
				// restore _result.overriden_match_begin
				_result.overriden_match_begin = current_state.initial_match_begin;
			}
		}
		stream = std::move(current_state.stream);

		_state_stack = std::deque<_state>();
		_expr = nullptr;

		if (current_state.automata_state == expr.end_state) {
			assert_true_logical(_ongoing_captures.empty(), "there are still ongoing captures");
			assert_true_logical(_atomic_stack_sizes.empty(), "there are still ongoing atomic groups");
			_result.captures.front().length =
				stream.codepoint_position() - _result.captures.front().begin.codepoint_position();
			return std::move(_result);
		}
		_ongoing_captures = std::stack<_capture_info>();
		_atomic_stack_sizes = std::stack<std::size_t>();
		return std::nullopt;
	}

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_log(
		[[maybe_unused]] const _state &s, [[maybe_unused]] std::u8string_view indent
	) {
		if constexpr (_uses_log) {
			debug_log <<
				indent << u8"Stream position: " << s.stream.codepoint_position() <<
				u8";  state: " << s.automata_state <<
				", transition: " << s.transition << "\n";

			debug_log << indent << u8"\tInitial ongoing captures: " << s.initial_ongoing_captures << "\n";

			debug_log << indent << u8"\tPartial ongoing captures:\n";
			auto partial_captures_copy = s.partial_finished_captures;
			while (!partial_captures_copy.empty()) {
				const auto &cap = partial_captures_copy.back();
				debug_log <<
					indent << u8"\t\t" << partial_captures_copy.size() <<
					": #" << cap.capture.index <<
					", from: " << cap.begin.codepoint_position() <<
					";  old from : " << cap.capture.capture_data.begin.codepoint_position() <<
					", old length : " << cap.capture.capture_data.length << "\n";
				partial_captures_copy.pop_back();
			}

			debug_log << indent << u8"\tFinished ongoing captures:\n";
			auto captures_copy = s.finished_captures;
			while (!captures_copy.empty()) {
				const auto &cap = captures_copy.back();
				debug_log <<
					indent << u8"\t\t" << captures_copy.size() <<
					": #" << cap.index <<
					", from: " << cap.capture_data.begin.codepoint_position() <<
					", length: " << cap.capture_data.length << "\n";
				captures_copy.pop_back();
			}
		}
	}

	template <typename Stream, typename LogFunc> bool matcher<Stream, LogFunc>::_check_backreference_transition(
		Stream &stream, std::size_t index, bool case_insensitive
	) const {
		auto &cap = _result.captures[index];
		Stream new_stream = stream;
		Stream target_stream = cap.begin;
		for (std::size_t i = 0; i < cap.length; ++i) {
			if (new_stream.empty()) {
				return false;
			}
			codepoint got = new_stream.take();
			codepoint expected = target_stream.take();
			if (case_insensitive) {
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

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_execute_transition(
		const Stream &stream, const compiled::transitions::capture_end&
	) {
		auto res = std::move(_ongoing_captures.top());
		_ongoing_captures.pop();
		if (_result.captures.size() <= res.index) {
			_result.captures.resize(res.index + 1);
		}

		if (!_state_stack.empty()) {
			// if necessary, record that this capture has finished
			auto &st = _state_stack.back();
			if (
				_ongoing_captures.size() + st.partial_finished_captures.size() <
				st.initial_ongoing_captures
			) {
				// when backtracking to the state, it's necessary to restore _ongoing_captures to this state
				st.partial_finished_captures.emplace_back(
					_finished_capture_info(_result.captures[res.index], res.index), res.begin
				);
			} else {
				// when backtracking, it's necessary to completely reset this capture to the previous state
				st.finished_captures.emplace_back(_result.captures[res.index], res.index);
			}
		}

		std::size_t index = res.index;
		_result.captures[index].length = stream.codepoint_position() - res.begin.codepoint_position();
		_result.captures[index].begin = std::move(res.begin);
	}
}
