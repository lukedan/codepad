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
		Stream &stream, const typename compiled_types::state_machine &expr,
		bool reject_empty_match, std::size_t max_iters
	) {
		_result = result();
		_result.captures.emplace_back().begin = stream;
		_expr = &expr;
		std::size_t reject_starting_position =
			reject_empty_match ? stream.codepoint_position() : std::numeric_limits<std::size_t>::max();

		_state current_state(std::move(stream), expr.get_start_state(), 0, *this, std::nullopt);
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

				_log(current_state, &current_state, u8"\t");

				debug_log << u8"\tState stack:\n";
				for (std::size_t i = 0; i < _state_stack.size(); ++i) {
					_log(_state_stack[i], i + 1 < _state_stack.size() ? &_state_stack[i + 1] : nullptr, u8"\t\t");
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
					_state_finished_subroutines.emplace(std::move(stackframe));
				}
				// pop stack frame
				_subroutine_stack.pop_back();
				continue;
			}
			if (current_state.automata_state == expr.get_end_state()) {
				// check if the match is empty and should be rejected
				if (current_state.stream.codepoint_position() != reject_starting_position) {
					break; // we've reached the end state
				}
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
						*this, _result.overriden_match_begin
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

				// throw away captures that started after this state
				std::size_t count =
					current_state.initial_ongoing_captures -
					_state_partial_finished_captures.count_since(current_state.partial_finished_captures);
				while (_ongoing_captures.size() > count) {
					_ongoing_captures.pop_back();
				}
				// restore finished captures that started after the state to their previously captured values
				while (!_state_finished_captures.is_at_bookmark(current_state.finished_captures)) {
					auto &cap = _state_finished_captures.top();
					_result.captures[cap.capture.get_index()] = std::move(cap.capture_data);
					_state_finished_captures.pop();
				}
				// restart finished captures that started before the state
				while (!_state_partial_finished_captures.is_at_bookmark(current_state.partial_finished_captures)) {
					const auto &cap = _state_partial_finished_captures.top();
					_ongoing_captures.emplace_back(std::move(cap.begin), cap.capture.capture);
					_result.captures[_ongoing_captures.back().capture.get_index()] =
						std::move(cap.capture.capture_data);
					_state_partial_finished_captures.pop();
				}
				// restore _result.overriden_match_begin
				_result.overriden_match_begin = current_state.initial_match_begin;
				// restore subroutines
				while (!_state_finished_subroutines.is_at_bookmark(current_state.finished_subroutines)) {
					_subroutine_stack.emplace_back(std::move(_state_finished_subroutines.top()));
					_state_finished_subroutines.pop();
				}
				// restore checkpoints
				while (!_state_restored_checkpoints.is_at_bookmark(current_state.restored_checkpoints)) {
					_checkpoint_stack.emplace_back(std::move(_state_restored_checkpoints.top()));
					_state_restored_checkpoints.pop();
				}
				// restore stream positions
				while (!_state_finished_stream_positions.is_at_bookmark(current_state.finished_stream_positions)) {
					_stream_position_stack.emplace_back(std::move(_state_finished_stream_positions.top()));
					_state_finished_stream_positions.pop();
				}
			}
		}
		stream = std::move(current_state.stream);

		_state_stack = std::deque<_state>();
		_state_partial_finished_captures = spliced_stack<_partial_finished_capture_info>();
		_state_finished_captures = spliced_stack<_finished_capture_info>();
		_state_finished_subroutines = spliced_stack<_subroutine_stackframe>();
		_state_restored_checkpoints = spliced_stack<_checkpointed_stream>();
		_state_finished_stream_positions = spliced_stack<_stream_position>();

		_expr = nullptr;

		if (iter < max_iters && current_state.automata_state == expr.get_end_state()) {
			assert_true_logical(_ongoing_captures.empty(), "there are still ongoing captures");
			assert_true_logical(_atomic_stack_sizes.empty(), "there are still ongoing atomic groups");
			assert_true_logical(_subroutine_stack.empty(), "there are still ongoing subroutines");
			assert_true_logical(_checkpoint_stack.empty(), "there are still checkpoints");
			assert_true_logical(_stream_position_stack.empty(), "there are still stream positions");

			// don't report a match if it should be rejected
			if (stream.codepoint_position() != reject_starting_position) {
				_result.captures.front().length =
					stream.codepoint_position() - _result.captures.front().begin.codepoint_position();
				while (!_result.captures.empty() && !_result.captures.back().is_valid()) {
					_result.captures.pop_back();
				}
				return std::move(_result);
			}
		}

		_ongoing_captures = std::deque<_capture_info>();
		_atomic_stack_sizes = std::deque<std::size_t>();
		_subroutine_stack = std::deque<_subroutine_stackframe>();
		_checkpoint_stack = std::deque<_checkpointed_stream>();
		_stream_position_stack = std::deque<_stream_position>();

		return std::nullopt;
	}

	template <typename Stream, typename DataTypes, typename LogFunc> void matcher<Stream, DataTypes, LogFunc>::_log(
		const _state &s, const _state *next, std::u8string_view indent
	) {
		if constexpr (_uses_log) {
			debug_log <<
				indent << u8"Stream position: " << s.stream.codepoint_position() <<
				u8";  state: " << s.automata_state.get_index() <<
				", transition: " << s.transition << "\n";

			debug_log << indent << u8"\tInitial ongoing captures: " << s.initial_ongoing_captures << "\n";

			if (next != &s) {
				debug_log << indent << u8"\tPartial ongoing captures:\n";
				auto partial_cap_beg =
					_state_partial_finished_captures.get_iterator_for(s.partial_finished_captures);
				auto partial_cap_end =
					next ?
					_state_partial_finished_captures.get_iterator_for(next->partial_finished_captures) :
					_state_partial_finished_captures.end();
				std::size_t i = 0;
				for (auto it = partial_cap_beg; it != partial_cap_end; ++it, ++i) {
					debug_log <<
						indent << u8"\t\t" << i <<
						": #" << it->capture.capture.get_index() <<
						", from: " << it->begin.codepoint_position() <<
						";  old from : " << it->capture.capture_data.begin.codepoint_position() <<
						", old length : " << it->capture.capture_data.length << "\n";
				}

				debug_log << indent << u8"\tFinished ongoing captures:\n";
				auto cap_beg = _state_finished_captures.get_iterator_for(s.finished_captures);
				auto cap_end =
					next ?
					_state_finished_captures.get_iterator_for(next->finished_captures) :
					_state_finished_captures.end();
				i = 0;
				for (auto it = cap_beg; it != cap_end; ++it, ++i) {
					debug_log <<
						indent << u8"\t\t" << i <<
						": #" << it->capture.get_index() <<
						", from: " << it->capture_data.begin.codepoint_position() <<
						", length: " << it->capture_data.length << "\n";
				}
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
				_ongoing_captures.size() +
					_state_partial_finished_captures.count_since(st.partial_finished_captures) <
				st.initial_ongoing_captures
			) {
				// when backtracking to the state, it's necessary to restore _ongoing_captures to this state
				_state_partial_finished_captures.emplace(
					_finished_capture_info(_result.captures[index], res.capture), res.begin
				);
			} else {
				// when backtracking, it's necessary to completely reset this capture to the previous state
				_state_finished_captures.emplace(_result.captures[index], res.capture);
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
			auto &first_erased_state = _state_stack[target_stack_size];
			// filter out saved information that we don't care about anymore (since they don't span across saved
			// states)
			_state_finished_subroutines.filter(
				_state_finished_subroutines.get_iterator_for(first_erased_state.finished_subroutines),
				_state_finished_subroutines.end(),
				[&](const _subroutine_stackframe &frame) {
					return frame.state_stack_size < target_stack_size;
				}
			);
			_state_restored_checkpoints.filter(
				_state_restored_checkpoints.get_iterator_for(first_erased_state.restored_checkpoints),
				_state_restored_checkpoints.end(),
				[&](const _checkpointed_stream &checkpoint) {
					return checkpoint.state_stack_size < target_stack_size;
				}
			);
			_state_finished_stream_positions.filter(
				_state_finished_stream_positions.get_iterator_for(first_erased_state.finished_stream_positions),
				_state_finished_stream_positions.end(),
				[&](const _stream_position &pos) {
					return pos.state_stack_size < target_stack_size;
				}
			);

			// it's more complicated for captures: we may need to convert partial finished captures into full
			// finished captures, since the new top state happens earlier

			// first, compute the number of captures to convert for each state
			// ** here we avoid an allocation by storing the number in initial_ongoing_captures - the states are
			// going to be erased anyway **
			std::size_t total_number_to_convert = 0;
			{

				// the number of captures that started after new top state but before the current state
				std::size_t captures_started_before_current = 0;
				// initial_ongoing_captures of the previous state
				std::size_t prev_initial_ongoing_captures =
					_state_stack[target_stack_size - 1].initial_ongoing_captures;
				for (std::size_t i = target_stack_size; i < _state_stack.size(); ++i) {
					const auto &prev_state = _state_stack[i - 1];
					auto &cur_state = _state_stack[i];

					// update the variable with the number of captures that started after the previous state and
					// before the current state
					captures_started_before_current +=
						cur_state.initial_ongoing_captures -
						(prev_initial_ongoing_captures - (
							_state_partial_finished_captures.count_since(prev_state.partial_finished_captures) -
							_state_partial_finished_captures.count_since(cur_state.partial_finished_captures)
						));

					// this is the number of states that started after the new top state, and finished after this
					// state was pushed, but before the next state was pushed; in other words, the number of states
					// that need to be converted
					std::size_t fully_finished_partial_captures = std::min(
						captures_started_before_current,
						_state_partial_finished_captures.count_since(cur_state.partial_finished_captures) - (
							i + 1 < _state_stack.size() ?
							_state_partial_finished_captures.count_since(
								_state_stack[i + 1].partial_finished_captures
							) :
							0
						)
					);

					// this number of captures didn't finish before this state
					captures_started_before_current -= fully_finished_partial_captures;

					prev_initial_ongoing_captures = cur_state.initial_ongoing_captures;
					// for the next phase we need to work from back to front, so it's more convenient to not include
					// the count between this state and the next state
					cur_state.initial_ongoing_captures = total_number_to_convert;
					// this sum is the total offset for all captures of this state
					total_number_to_convert += fully_finished_partial_captures;
				}
			}
			// then, rearrange the entries in _state_finished_captures to leave enough space for the converted
			// entries, and take elemnts from partial finished captures to fill the gaps
			{
				std::size_t old_size = _state_finished_captures.size();
				_state_finished_captures.resize(old_size + total_number_to_convert);
				// do it back to front
				auto old_it = _state_finished_captures.begin() + old_size;
				auto it = _state_finished_captures.end();
				std::size_t prev_offset = total_number_to_convert;
				std::size_t prev_count = total_number_to_convert; // account for the resize
				for (std::size_t i = _state_stack.size(); i > target_stack_size; ) {
					--i;
					auto &cur_state = _state_stack[i];

					std::size_t total_offset = cur_state.initial_ongoing_captures;
					std::size_t this_offset = prev_offset - total_offset;
					std::size_t total_count = _state_finished_captures.count_since(cur_state.finished_captures);
					std::size_t this_count = total_count - prev_count;
					
					// copy fully finished captures
					for (std::size_t j = 0; j < this_count; ++j) {
						--old_it;
						--it;
						*it = std::move(*old_it);
					}

					// old partially finished range to convert to fully finished
					auto cvt_it =
						_state_partial_finished_captures.get_iterator_for(cur_state.partial_finished_captures) +
						this_offset;
					for (std::size_t j = 0; j < this_offset; ++j) {
						--cvt_it;
						--it;
						*it = std::move(cvt_it->capture);
					}

					// update to include the count between this state and the next state, because we're workingn from
					// front to back next
					cur_state.initial_ongoing_captures = prev_offset;

					prev_offset = total_offset;
					prev_count = total_count;
				}
			}
			// finally, remove all converted partially finished ranges
			{
				std::size_t prev_offset = 0;
				auto to_it = _state_partial_finished_captures.get_iterator_for(
					_state_stack[target_stack_size].partial_finished_captures
				);
				auto from_it = to_it;
				for (std::size_t i = target_stack_size; i < _state_stack.size(); ++i) {
					const auto &state = _state_stack[i];
					std::size_t total_offset = state.initial_ongoing_captures;
					std::size_t this_offset = total_offset - prev_offset;
					std::size_t this_count =
						_state_partial_finished_captures.count_since(state.partial_finished_captures) - this_offset;
					if (i + 1 < _state_stack.size()) {
						this_count -= _state_partial_finished_captures.count_since(
							_state_stack[i + 1].partial_finished_captures
						);
					}

					from_it += this_offset;
					for (std::size_t j = 0; j < this_count; ++j, ++from_it, ++to_it) {
						*to_it = std::move(*from_it);
					}

					prev_offset = total_offset;
				}
				_state_partial_finished_captures.erase(to_it, _state_partial_finished_captures.end());
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
			_state_restored_checkpoints.emplace(std::move(checkpoint));
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
