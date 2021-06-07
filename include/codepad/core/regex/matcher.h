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
		[[nodiscard]] std::size_t try_match(Stream &stream, const state_machine &expr) {
			_state current_state(std::move(stream), expr.start_state, 0);
			while (current_state.automata_state != expr.end_state) {
				Stream checkpoint_stream = current_state.stream;
				const auto &transition = current_state.get_transition(expr);
				bool transition_ok = true;
				if (std::holds_alternative<codepoint_string>(transition.condition)) {
					const auto &str = std::get<codepoint_string>(transition.condition);
					for (codepoint cp : str) {
						if (current_state.stream.empty() || current_state.stream.take() != cp) {
							transition_ok = false;
							break;
						}
					}
				} else {
					if (current_state.stream.empty()) {
						transition_ok = false;
					} else {
						codepoint cp = current_state.stream.take();
						const auto &ranges =
							std::get<std::vector<std::pair<codepoint, codepoint>>>(transition.condition);
						auto it = std::lower_bound(
							ranges.begin(), ranges.end(), cp,
							[](std::pair<codepoint, codepoint> range, codepoint c) {
								return range.second < c;
							}
						);
						transition_ok = it != ranges.end() && cp >= it->first;
					}
				}

				++current_state.transition;
				if (transition_ok) {
					if (current_state.transition < current_state.get_automata_state(expr).transitions.size()) {
						_state_stack.emplace(
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
					if (_state_stack.empty()) {
						current_state.stream = std::move(checkpoint_stream);
						break;
					}
					current_state = std::move(_state_stack.top());
					_state_stack.pop();
				}
			}
			stream = std::move(current_state.stream);
			return current_state.automata_state;
		}

		/// Tests if the given expression matches the full input.
		[[nodiscard]] bool match(Stream s, const state_machine &expr) {
			std::size_t end_state = try_match(s, expr);
			return end_state == expr.end_state && s.empty();
		}
		/// Finds the starting point of the next match in the input stream. After the function returns, \p s will be
		/// at the end of the input if no match is found, or be at the end position of the match.
		///
		/// \return Starting position of the match.
		[[nodiscard]] std::optional<std::size_t> find_next(Stream &s, const state_machine &expr) {
			while (!s.empty()) {
				Stream temp = s;
				std::size_t state = try_match(temp, expr);
				if (state == expr.end_state) {
					std::size_t start_pos = s.position();
					s = std::move(temp);
					return start_pos;
				}
				s.take();
			}
			return std::nullopt;
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
			[[nodiscard]] const state_machine::state &get_automata_state(const state_machine &sm) const {
				return sm.states[automata_state];
			}
			/// Returns the associated \ref state_machine::transition.
			[[nodiscard]] const state_machine::transition &get_transition(const state_machine &sm) const {
				return sm.states[automata_state].transitions[transition];
			}
		};
		/// The stack of states used for backtracking.
		std::stack<_state> _state_stack;
	};
}
