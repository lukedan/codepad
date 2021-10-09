// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once
#include "matcher.h"

/// \file
/// Implementation of the matcher.

namespace codepad::regex {
	template <
		typename Stream, typename DataTypes, typename LogFunc
	> std::optional<match_result<Stream>> matcher<Stream, DataTypes, LogFunc>::try_match(
		Stream &stream, const typename compiled_types::state_machine &expr, std::size_t max_iters
	) {
		_result = result();
		_result.captures.emplace_back().begin = stream;
		_expr = &expr;

		_state current_state(std::move(stream), expr.get_start_state(), 0, 0, std::nullopt);
		std::size_t iter = 0;
		for (; iter < max_iters; ++iter) {
			if constexpr (_uses_log) {
				debug_log << u8"\nIteration " << iter << u8"\n";
					
				debug_log << u8"\tCaptures:\n";
				for (std::size_t j = 0; j < _result.captures.size(); ++j) {
					debug_log <<
						u8"\t\tPosition: " << _result.captures[j].begin.codepoint_position() <<
						u8",  length: " << _result.captures[j].length << u8"\n";
				}

				debug_log << u8"\tOngoing captures:\n";
				for (auto it = _ongoing_captures.crbegin(); it != _ongoing_captures.crend(); ++it) {
					debug_log <<
						u8"\t\t" << (_ongoing_captures.size() - (it - _ongoing_captures.crbegin())) <<
						u8": #" << it->capture.get_index() <<
						u8",  begin: " << it->begin.codepoint_position() << u8"\n";
				}

				debug_log << u8"\tOngoing subroutines:\n";
				for (auto it = _subroutine_stack.crbegin(); it != _subroutine_stack.crend(); ++it) {
					debug_log <<
						u8"\t\t" << (_subroutine_stack.size() - (it - _subroutine_stack.crbegin())) <<
						u8": #" << it->subroutine_capture.get_index() << u8"\n";
					for (auto it2 = it->finished_captures.crbegin(); it2 != it->finished_captures.crend(); ++it2) {
						debug_log <<
							u8"\t\t\t" << (it->finished_captures.size() - (it2 - it->finished_captures.crbegin())) <<
							u8": #" << it2->capture.get_index() <<
							u8",  begin: " << it2->capture_data.begin.codepoint_position() <<
							u8",  length: " << it2->capture_data.length << u8"\n";
					}
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
				_log(u8"\tSubroutine finished\n");
				auto &stackframe = _subroutine_stack.back();
				current_state.automata_state = stackframe.return_state;
				current_state.transition = 0;
				// revert captures during the subroutine
				for (
					auto it = stackframe.finished_captures.rbegin();
					it != stackframe.finished_captures.rend();
					++it
				) {
					_result.captures[it->capture.get_index()] = it->capture_data;
					if constexpr (_uses_log) {
						debug_log <<
							u8"\t\tRestoring capture #" << it->capture.get_index() <<
							u8"  begin:" << _result.captures[it->capture.get_index()].begin.codepoint_position() <<
							u8"  length: " << _result.captures[it->capture.get_index()].length << u8"\n";
					}
				}
				if (stackframe.state_stack_size < _state_stack.size()) {
					// this subroutine started before the last backtracking state; we need to save it so that when
					// backtracking we can restart it
					_state_stack.back().finished_subroutines.emplace_back(std::move(stackframe));
				}
				// pop stack frame
				_subroutine_stack.pop_back();
				continue;
			}
			if (current_state.automata_state == expr.get_end_state()) {
				break; // we've reached the end state
			}
			Stream checkpoint_stream = current_state.stream;
			const compiled_types::transition *transition = nullptr;
			if (!current_state.get_transitions(expr).empty()) {
				transition = &current_state.get_current_transition(expr);
				std::visit(
					[&, this](auto &&cond) {
						if (!_check_transition(current_state.stream, cond)) {
							transition = nullptr;
						}
					},
					transition->condition
				);
			}

			++current_state.transition;
			if (transition) {
				_log(u8"\tTransition OK\n");
				if (current_state.transition < current_state.get_transitions(expr).size()) {
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
				current_state.automata_state = transition->new_state;
				current_state.transition = 0;
				// continue to the next iteration
			} else {
				if (current_state.transition < current_state.get_transitions(expr).size()) {
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
				// pop stream positions saved after this state
				while (
					!_stream_position_stack.empty() &&
					_state_stack.size() < _stream_position_stack.back().state_stack_size
				) {
					_stream_position_stack.pop_back();
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
					_result.captures[cap.capture.get_index()] = std::move(cap.capture_data);
					current_state.finished_captures.pop_back();
				}
				while (!current_state.partial_finished_captures.empty()) {
					const auto &cap = current_state.partial_finished_captures.back();
					_ongoing_captures.emplace_back(std::move(cap.begin), cap.capture.capture);
					_result.captures[_ongoing_captures.back().capture.get_index()] =
						std::move(cap.capture.capture_data);
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
				// restore stream positions
				while (!current_state.finished_stream_positions.empty()) {
					_stream_position_stack.emplace_back(std::move(current_state.finished_stream_positions.back()));
					current_state.finished_stream_positions.pop_back();
				}
			}
		}
		stream = std::move(current_state.stream);

		_state_stack = std::deque<_state>();
		_expr = nullptr;

		if (iter < max_iters && current_state.automata_state == expr.get_end_state()) {
			assert_true_logical(_ongoing_captures.empty(), "there are still ongoing captures");
			assert_true_logical(_atomic_stack_sizes.empty(), "there are still ongoing atomic groups");
			assert_true_logical(_subroutine_stack.empty(), "there are still ongoing subroutines");
			assert_true_logical(_checkpoint_stack.empty(), "there are still checkpoints");
			assert_true_logical(_stream_position_stack.empty(), "there are still stream positions");
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
		_stream_position_stack = std::deque<_stream_position>();
		return std::nullopt;
	}

	template <typename Stream, typename DataTypes, typename LogFunc> void matcher<Stream, DataTypes, LogFunc>::_log(
		[[maybe_unused]] const _state &s, [[maybe_unused]] std::u8string_view indent
	) {
		if constexpr (_uses_log) {
			debug_log <<
				indent << u8"Stream position: " << s.stream.codepoint_position() <<
				u8";  state: " << s.automata_state.get_index() <<
				", transition: " << s.transition << "\n";

			debug_log << indent << u8"\tInitial ongoing captures: " << s.initial_ongoing_captures << "\n";

			debug_log << indent << u8"\tPartial ongoing captures:\n";
			auto partial_captures_copy = s.partial_finished_captures;
			while (!partial_captures_copy.empty()) {
				const auto &cap = partial_captures_copy.back();
				debug_log <<
					indent << u8"\t\t" << partial_captures_copy.size() <<
					": #" << cap.capture.capture.get_index() <<
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
					": #" << cap.capture.get_index() <<
					", from: " << cap.capture_data.begin.codepoint_position() <<
					", length: " << cap.capture_data.length << "\n";
				captures_copy.pop_back();
			}
		}
	}

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> bool matcher<Stream, DataTypes, LogFunc>::_check_backreference_transition(
		Stream &stream, typename compiled_types::capture_ref capture, bool case_insensitive
	) const {
		auto &cap = _result.captures[capture.get_index()];
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

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> void matcher<Stream, DataTypes, LogFunc>::_execute_transition(
		const Stream &stream, const typename compiled_types::transitions::capture_end&
	) {
		auto res = std::move(_ongoing_captures.back());
		auto index = res.capture.get_index();
		_ongoing_captures.pop_back();
		if (_result.captures.size() <= index) {
			_result.captures.resize(index + 1);
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
					_finished_capture_info(_result.captures[index], res.capture), res.begin
				);
			} else {
				// when backtracking, it's necessary to completely reset this capture to the previous state
				st.finished_captures.emplace_back(_result.captures[index], res.capture);
			}
		}
		if (!_subroutine_stack.empty()) {
			// a capture cannot span across subroutine calls
			_subroutine_stack.back().finished_captures.emplace_back(_result.captures[index], res.capture);
		}

		_result.captures[index].length = stream.codepoint_position() - res.begin.codepoint_position();
		_result.captures[index].begin = std::move(res.begin);
	}

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> void matcher<Stream, DataTypes, LogFunc>::_execute_transition(
		const Stream&, const typename compiled_types::transitions::pop_atomic&
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

				// save information of all saved stream positions
				for (auto &pos : cur_state.finished_stream_positions) {
					if (pos.state_stack_size < target_stack_size) {
						new_top_state.finished_stream_positions.emplace_back(std::move(pos));
					}
					// otherwise, this position was saved after the new top state and we don't care
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

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> void matcher<Stream, DataTypes, LogFunc>::_execute_transition(
		Stream stream, const typename compiled_types::transitions::push_stream_checkpoint&
	) {
		auto &checkpoint = _checkpoint_stack.emplace_back();
		checkpoint.position = std::move(stream);
		checkpoint.state_stack_size = _state_stack.size();
	}

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> void matcher<Stream, DataTypes, LogFunc>::_execute_transition(
		Stream &stream, const typename compiled_types::transitions::restore_stream_checkpoint&
	) {
		auto checkpoint = std::move(_checkpoint_stack.back());
		_checkpoint_stack.pop_back();
		stream = checkpoint.position;
		if (checkpoint.state_stack_size < _state_stack.size()) {
			// this was checkpointed before the current state was pushed; we need to save it for backtracking
			_state_stack.back().restored_checkpoints.emplace_back(std::move(checkpoint));
		}
	}

	template <
		typename Stream, typename DataTypes, typename LogFunc
	> void matcher<Stream, DataTypes, LogFunc>::_execute_transition(
		const Stream&, const typename compiled_types::transitions::jump &jmp
	) {
		auto &sf = _subroutine_stack.emplace_back();
		sf.target = jmp.target;
		sf.return_state = jmp.return_state;
		sf.subroutine_capture = jmp.subroutine_capture;
		sf.state_stack_size = _state_stack.size();
	}
}
