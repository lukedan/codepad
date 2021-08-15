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
					const auto &cap = caps_copy.back();
					debug_log <<
						u8"\t\t" << caps_copy.size() <<
						u8": #" << cap.index <<
						u8",  begin: " << cap.begin.codepoint_position() << u8"\n";
					caps_copy.pop_back();
				}

				_log(current_state, u8"\t");

				debug_log << u8"\tState stack:\n";
				auto states_copy = _state_stack;
				while (!states_copy.empty()) {
					_log(states_copy.back(), u8"\t\t");
					states_copy.pop_back();
				}
			}
			if (!_subroutine_stack.empty() && current_state.automata_state == _subroutine_stack.back().target) {
				// check subroutines before checking if we're finished, so that we correctly handle recursion
				current_state.automata_state = _subroutine_stack.back().return_state;
				current_state.transition = 0;
				if (_subroutine_stack.back().state_stack_size < _state_stack.size()) {
					// this subroutine started before the last backtracking state; we need to save it so that when
					// backtracking we can restart it
					_state_stack.back().finished_subroutines.emplace_back(std::move(_subroutine_stack.back()));
				}
				_subroutine_stack.pop_back();
				_log(u8"\tSubroutine finished");
				// TODO revert capture groups
				continue;
			}
			if (current_state.automata_state == expr.end_state) {
				break; // we've reached the end state
			}
			Stream checkpoint_stream = current_state.stream;
			const compiled::transition *transition = nullptr;
			if (!current_state.get_automata_state(expr).transitions.empty()) {
				transition = &current_state.get_transition(expr);
				std::visit(
					[&, this](auto &&cond) {
						if (!_check_transition(current_state.stream, *transition, cond)) {
							transition = nullptr;
						}
					},
					transition->condition
				);
			}

			++current_state.transition;
			if (transition) {
				_log(u8"\tTransition OK\n");
				if (current_state.transition < current_state.get_automata_state(expr).transitions.size()) {
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
					transition->condition
				);
				current_state.automata_state = transition->new_state_index;
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

				// pop atomic groups that started after the state
				while (!_atomic_stack_sizes.empty() && _state_stack.size() < _atomic_stack_sizes.back()) {
					_atomic_stack_sizes.pop_back();
				}
				// pop subroutine stackframes that started after the state
				while (
					!_subroutine_stack.empty() && _state_stack.size() < _subroutine_stack.back().state_stack_size
				) {
					_subroutine_stack.pop_back();
				}
				// pop checkpoints that started after the state
				while (
					!_checkpoint_stack.empty() && _state_stack.size() < _checkpoint_stack.back().state_stack_size
				) {
					_checkpoint_stack.pop_back();
				}

				// restore _ongoing_captures to the state we're backtracking to
				std::size_t count =
					current_state.initial_ongoing_captures - current_state.partial_finished_captures.size();
				while (_ongoing_captures.size() > count) {
					_ongoing_captures.pop_back();
				}
				// restore _result.captures
				while (!current_state.finished_captures.empty()) {
					auto &cap = current_state.finished_captures.back();
					_result.captures[cap.index] = std::move(cap.capture_data);
					current_state.finished_captures.pop_back();
				}
				while (!current_state.partial_finished_captures.empty()) {
					const auto &cap = current_state.partial_finished_captures.back();
					_ongoing_captures.emplace_back(std::move(cap.begin), cap.capture.index);
					_result.captures[_ongoing_captures.back().index] = std::move(cap.capture.capture_data);
					current_state.partial_finished_captures.pop_back();
				}
				// restore _result.overriden_match_begin
				_result.overriden_match_begin = current_state.initial_match_begin;
				// restore subroutines
				while (!current_state.finished_subroutines.empty()) {
					_subroutine_stack.emplace_back(std::move(current_state.finished_subroutines.back()));
					current_state.finished_subroutines.pop_back();
				}
				// restore checkpoints
				while (!current_state.restored_checkpoints.empty()) {
					_checkpoint_stack.emplace_back(std::move(current_state.restored_checkpoints.back()));
					current_state.restored_checkpoints.pop_back();
				}
			}
		}
		stream = std::move(current_state.stream);

		_state_stack = std::deque<_state>();
		_expr = nullptr;

		if (current_state.automata_state == expr.end_state) {
			assert_true_logical(_ongoing_captures.empty(), "there are still ongoing captures");
			assert_true_logical(_atomic_stack_sizes.empty(), "there are still ongoing atomic groups");
			assert_true_logical(_subroutine_stack.empty(), "there are still ongoing subroutines");
			assert_true_logical(_checkpoint_stack.empty(), "there are still checkpoints");
			_result.captures.front().length =
				stream.codepoint_position() - _result.captures.front().begin.codepoint_position();
			while (!_result.captures.empty() && !_result.captures.back().is_valid()) {
				_result.captures.pop_back();
			}
			return std::move(_result);
		}
		_ongoing_captures = std::deque<_capture_info>();
		_atomic_stack_sizes = std::deque<std::size_t>();
		_subroutine_stack = std::deque<_subroutine_stackframe>();
		_checkpoint_stack = std::deque<_checkpointed_stream>();
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
		auto res = std::move(_ongoing_captures.back());
		_ongoing_captures.pop_back();
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

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_execute_transition(
		const Stream&, const compiled::transitions::pop_atomic&
	) {
		std::size_t target_stack_size = _atomic_stack_sizes.back();
		if (target_stack_size > 0 && target_stack_size < _state_stack.size()) {
			// we need to fold the completed captures information back to the previous state
			auto &new_top_state = _state_stack[target_stack_size - 1];
			// this stores the number of captures that started after new top state but before the current state
			std::size_t captures_started_before_current = 0;
			for (std::size_t i = target_stack_size; i < _state_stack.size(); ++i) {
				const auto &prev_state = _state_stack[i - 1];
				const auto &cur_state = _state_stack[i];

				// save information of all subroutines that needs restarting
				for (auto &subroutine : cur_state.finished_subroutines) {
					if (subroutine.state_stack_size < target_stack_size) {
						new_top_state.finished_subroutines.emplace_back(std::move(subroutine));
					}
					// otherwise, this subroutine started after the new top state and we don't care
				}

				// save information of all restored checkpoints that may need restoring
				for (auto &checkpoint : cur_state.restored_checkpoints) {
					if (checkpoint.state_stack_size < target_stack_size) {
						new_top_state.restored_checkpoints.emplace_back(std::move(checkpoint));
					}
					// otherwise, this checkpoint was restored after the new top state and we don't care
				}

				// update the variable with the number of captures that started after the previous state and
				// before the current state
				captures_started_before_current +=
					cur_state.initial_ongoing_captures -
					(prev_state.initial_ongoing_captures - prev_state.partial_finished_captures.size());

				// this is the number of states that started after the new top state, and finished after this
				// state was pushed, but before the next state was pushed
				std::size_t fully_finished_partial_captures = std::min(
					captures_started_before_current, cur_state.partial_finished_captures.size()
				);

				// captures that started after new_top_state but before cur_state, and finished after cur_state
				// but before the next state
				for (std::size_t j = 0; j < fully_finished_partial_captures; ++j) {
					new_top_state.finished_captures.emplace_back(
						std::move(cur_state.partial_finished_captures[j].capture)
					);
				}

				// captures that started before new_top_state, and finished after cur_state but before the next
				// state
				for (
					std::size_t j = fully_finished_partial_captures;
					j < cur_state.partial_finished_captures.size();
					++j
				) {
					new_top_state.partial_finished_captures.emplace_back(
						std::move(cur_state.partial_finished_captures[j])
					);
				}

				// captures that started & finished after cur_state but before the next state
				for (auto &s : cur_state.finished_captures) {
					new_top_state.finished_captures.emplace_back(std::move(s));
				}

				// update captures_started_before_current
				captures_started_before_current -= fully_finished_partial_captures;
			}
		}
		_state_stack.erase(_state_stack.begin() + target_stack_size, _state_stack.end());
		_atomic_stack_sizes.pop_back();
	}

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_execute_transition(
		Stream stream, const compiled::transitions::push_stream_checkpoint&
	) {
		auto &checkpoint = _checkpoint_stack.emplace_back();
		checkpoint.position = std::move(stream);
		checkpoint.state_stack_size = _state_stack.size();
	}

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_execute_transition(
		Stream &stream, const compiled::transitions::restore_stream_checkpoint&
	) {
		auto checkpoint = std::move(_checkpoint_stack.back());
		_checkpoint_stack.pop_back();
		stream = checkpoint.position;
		if (checkpoint.state_stack_size < _state_stack.size()) {
			// this was checkpointed before the current state was pushed; we need to save it for backtracking
			_state_stack.back().restored_checkpoints.emplace_back(std::move(checkpoint));
		}
	}

	template <typename Stream, typename LogFunc> void matcher<Stream, LogFunc>::_execute_transition(
		const Stream&, const compiled::transitions::jump &jmp
	) {
		auto &sf = _subroutine_stack.emplace_back();
		sf.target = jmp.target;
		sf.return_state = jmp.return_state;
		sf.subroutine_index = jmp.subroutine_index;
		sf.state_stack_size = _state_stack.size();
	}
}
