// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Used to search regex state machines.

#include <optional>
#include <format>
#include <deque>

#include "compiler.h"

namespace codepad::regex {
	/// A spliced stack.
	template <typename T, typename Container = std::deque<T>, typename MarkType = std::size_t> class spliced_stack {
	public:
		static_assert(std::is_unsigned_v<MarkType>, "Marker must be unsigned integral type.");

		/// A bookmark in the stack.
		struct bookmark {
			friend spliced_stack;
		public:
			/// Indicates that this mark is not valid.
			constexpr static MarkType invalid_mark_value = std::numeric_limits<MarkType>::max();

			/// Default constructor.
			bookmark() = default;

			/// Returns whether this bookmark is valid.
			[[nodiscard]] bool is_valid() const {
				return _mark != invalid_mark_value;
			}
			/// Returns whether this bookmark is valid.
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		protected:
			MarkType _mark = invalid_mark_value; ///< Position of the mark.

			/// Initializes \ref _mark.
			explicit bookmark(typename Container::size_type m) : _mark(m) {
			}
		};
		using size_type = typename Container::size_type; ///< Size type.
		using iterator = typename Container::iterator; ///< Iterator.
		using const_iterator = typename Container::const_iterator; ///< Const iterator.

		/// Creates a new object at the top of the stack.
		template <typename ...Args> T &emplace(Args &&...args) {
			return _storage.emplace_back(std::forward<Args>(args)...);
		}
		/// Removes the top element of the stack.
		void pop() {
			_storage.pop_back();
		}

		/// Returns a bookmark for the current stack state.
		[[nodiscard]] bookmark mark() const {
			return bookmark(_storage.size());
		}

		/// Returns the top element.
		[[nodiscard]] T &top() {
			return _storage.back();
		}
		/// \overload
		[[nodiscard]] const T &top() const {
			return _storage.back();
		}

		/// Returns the size of this container.
		[[nodiscard]] size_type size() const {
			return _storage.size();
		}
		/// Returns whether this container is empty.
		[[nodiscard]] bool empty() const {
			return _storage.empty();
		}

		/// Resizes this container.
		void resize(size_type size) {
			_storage.resize(size);
		}
		/// \overload
		void resize(size_type size, const T &val) {
			_storage.resize(size, val);
		}

		/// Erases the given range of elements.
		void erase(const_iterator begin, const_iterator end) {
			_storage.erase(begin, end);
		}

		/// Returns the number of elements pushed since the given \ref bookmark is set.
		[[nodiscard]] size_type count_since(const bookmark &b) const {
			return _storage.size() - b._mark;
		}
		/// Returns whether the top of the stack is at the given \ref bookmark.
		[[nodiscard]] bool is_at_bookmark(const bookmark &b) const {
			return count_since(b) == 0;
		}

		/// Returns the iterator corresponding to the given \ref bookmark.
		[[nodiscard]] iterator get_iterator_for(const bookmark &b) {
			return _storage.begin() + b._mark;
		}
		/// \overload
		[[nodiscard]] const_iterator get_iterator_for(const bookmark &b) const {
			return _storage.begin() + b._mark;
		}

		/// Returns an iterator to the first element.
		[[nodiscard]] iterator begin() {
			return _storage.begin();
		}
		/// \overload
		[[nodiscard]] const_iterator begin() const {
			return _storage.begin();
		}
		/// Returns an iterator past the last element.
		[[nodiscard]] iterator end() {
			return _storage.end();
		}
		/// \overload
		[[nodiscard]] const_iterator end() const {
			return _storage.end();
		}

		/// Removes all elements in the given range for which the given predicate returns \p false.
		template <typename Pred> void filter(iterator begin, iterator end, Pred &&predicate) {
			iterator last = begin;
			for (iterator i = begin; i != end; ++i) {
				if (predicate(*i)) {
					*last = std::move(*i);
					++last;
				}
			}
			_storage.erase(last, end);
		}
	protected:
		Container _storage; ///< The storage.
	};

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
	template <typename Stream, typename DataTypes, typename LogFunc = no_log> class matcher {
	public:
		using state_index = typename DataTypes::state_index; ///< State index type.
		using transition_index = typename DataTypes::transition_index; ///< Transition index type.
		using compiled_types = compiled<DataTypes>; ///< All compiled types.
		using marker_ref = typename compiled_types::marker_ref; ///< Marker reference type.
		using result = match_result<Stream>; ///< Match result.

		/// Default constructor.
		matcher() = default;
		/// Initializes \ref debug_log.
		template <typename ...Args> explicit matcher(Args &&...f) : debug_log(std::forward<Args>(f)...) {
		}

		/// Tests whether there is a match starting from the current position of the input stream.
		///
		/// \return The state of the state machine after it's been executed from this position.
		[[nodiscard]] std::optional<result> try_match(
			Stream&, const typename compiled_types::state_machine&,
			bool reject_empty_match, std::size_t max_iters = 1000000
		);

		/// Finds the starting point of the next match in the input stream. After the function returns, \p s will be
		/// at the end of the input if no match is found, or be at the end position of the match.
		/// \p reject_empty_match is used both to determine if an empty match should be rejected based on the
		/// previous result, and to notify the caller if the next empty match should be rejected.
		///
		/// \return Starting position of the match.
		[[nodiscard]] std::optional<result> find_next(
			Stream &s, const typename compiled_types::state_machine &expr,
			bool &reject_empty_match, std::size_t max_iters = 1000000
		) {
			while (true) {
				Stream temp = s;
				if (auto res = try_match(temp, expr, reject_empty_match, max_iters)) {
					// if this match is empty, reject the next match if it's empty
					reject_empty_match = s.codepoint_position() == temp.codepoint_position();
					s = std::move(temp);
					return std::move(res.value());
				}
				if (s.empty()) {
					break;
				}
				s.take();
				reject_empty_match = false;
			}
			return std::nullopt;
		}
		/// Finds all matches in the given stream, calling the given callback for each match. This is written so that
		/// empty input streams can be handled correctly.
		template <typename Callback> void find_all(
			Stream &s, const typename compiled_types::state_machine &expr, Callback &&cb,
			std::size_t max_iters = 1000000
		) {
			using _return_type = std::invoke_result_t<std::decay_t<Callback&&>, result>;

			bool reject_empty = false;
			while (true) {
				if (auto res = find_next(s, expr, reject_empty, max_iters)) {
					if constexpr (std::is_same_v<_return_type, bool>) {
						if (!cb(std::move(res.value()))) {
							break;
						}
					} else {
						cb(std::move(res.value()));
					}
				} else {
					break;
				}
			}
		}
	
		[[no_unique_address]] LogFunc debug_log; ///< Used to log debug information.
		marker_ref marker; ///< Last marker encountered during the match.
	protected:
		/// Indicates whether logging is enabled.
		constexpr static bool _uses_log = !std::is_same_v<LogFunc, no_log>;

		/// Information about an ongoing capture.
		struct _capture_info {
			/// Default constructor.
			_capture_info() = default;
			/// Initializes all fields of this struct.
			_capture_info(Stream s, typename compiled_types::capture_ref cap) : begin(std::move(s)), capture(cap) {
			}

			Stream begin; ///< State of the input stream at the beginning of this capture.
			typename compiled_types::capture_ref capture; ///< The capture.
		};
		/// Information about a finished capture that needs to be reset back to the previous value during
		/// backtracking.
		struct _finished_capture_info {
			/// Default constructor.
			_finished_capture_info() = default;
			/// Initializes all fields of this struct.
			_finished_capture_info(typename result::capture cap, typename compiled_types::capture_ref cap_ref) :
				capture_data(std::move(cap)), capture(cap_ref) {
			}

			typename result::capture capture_data; ///< Previous capture data overwritten by this match.
			typename compiled_types::capture_ref capture; ///< The capture.
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
		/// Stackframe of a subroutine.
		struct _subroutine_stackframe {
			/// Used to revert captures back to their initial value after a subroutine finishes.
			std::vector<_finished_capture_info> finished_captures;
			typename compiled_types::state_ref target; ///< \sa \ref compiled_types::jump::target
			typename compiled_types::state_ref return_state; ///< \sa \ref compiled_types::jump::return_state
			/// When backtracking to before this state, this subroutine stackframe becomes invalid and should be
			/// popped.
			std::size_t state_stack_size = 0;
			typename compiled_types::capture_ref subroutine_capture; ///< Index of this subroutine.
		};
		/// A checkpointed stream position.
		struct _checkpointed_stream {
			Stream position; ///< The stream.
			std::size_t state_stack_size = 0; ///< State stack size when this stream was checkpointed.
		};
		/// Saved stream position used for detecting infinite loops.
		struct _stream_position {
			std::size_t codepoint_position = 0; ///< Codepoint position of the stream.
			std::size_t state_stack_size = 0; ///< State stack size when this position was pushed.
		};
		/// The state of the automata at one moment.
		struct _state {
			/// Initializes all fields of this struct.
			_state(
				Stream str, typename compiled_types::state_ref st, typename compiled_types::transition_index trans,
				matcher &owner, std::optional<Stream> overriden_start
			) :
				stream(std::move(str)), automata_state(st), transition(trans),
				initial_ongoing_captures(owner._ongoing_captures.size()),
				partial_finished_captures(owner._state_partial_finished_captures.mark()),
				finished_captures(owner._state_finished_captures.mark()),
				finished_subroutines(owner._state_finished_subroutines.mark()),
				restored_checkpoints(owner._state_restored_checkpoints.mark()),
				finished_stream_positions(owner._state_finished_stream_positions.mark()),
				initial_match_begin(std::move(overriden_start)) {
			}

			Stream stream; ///< State of the input stream.
			typename compiled_types::state_ref automata_state; ///< Current state in the automata.
			transition_index transition = 0; ///< Index of the current transition in \ref automata_state.

			/// The number of captures that was ongoing before this state was pushed onto the stack.
			std::size_t initial_ongoing_captures = 0;
			/// Bookmark in \ref _state_partial_finished_captures for the first element for this state.
			typename spliced_stack<_partial_finished_capture_info>::bookmark partial_finished_captures;
			/// Bookmark in \ref _state_finished_captures for the first element for this state.
			typename spliced_stack<_finished_capture_info>::bookmark finished_captures;
			/// Bookmark in \ref _state_finished_subroutines for the first element for this state.
			typename spliced_stack<_subroutine_stackframe>::bookmark finished_subroutines;
			/// Bookmark in \ref _state_restored_checkpoints for the first element for this state.
			typename spliced_stack<_checkpointed_stream>::bookmark restored_checkpoints;
			/// Bookmark in \ref _state_finished_stream_positions for the first element for this state.
			typename spliced_stack<_stream_position>::bookmark finished_stream_positions;
			/// Overriden match starting position before this state was pushed onto the stack.
			std::optional<Stream> initial_match_begin;

			/// Returns all transitions of the state.
			[[nodiscard]] std::span<const typename compiled_types::transition> get_transitions(
				const typename compiled_types::state_machine &sm
			) const {
				return sm.get_transitions(automata_state);
			}
			/// Returns the associated \ref state_machine::transition.
			[[nodiscard]] const typename compiled_types::transition &get_current_transition(
				const typename compiled_types::state_machine &sm
			) const {
				return get_transitions(sm)[transition];
			}
		};

		const typename compiled_types::state_machine *_expr = nullptr; ///< State machine for the expression.
		result _result; ///< Cached match result.

		std::deque<_state> _state_stack; ///< States for backtracking.

		std::deque<_capture_info> _ongoing_captures; ///< Ongoing captures.
		/// Size of \ref _state_stack at the beginning of each ongoing atomic group.
		std::deque<std::size_t> _atomic_stack_sizes;
		std::deque<_subroutine_stackframe> _subroutine_stack; ///< Subroutine stack.
		std::deque<_checkpointed_stream> _checkpoint_stack; ///< Checkpoint stack.
		std::deque<_stream_position> _stream_position_stack; ///< Saved stream positions.

		/// A spliced stack that stores the stack of captures that started before a state was pushed, and ended after
		/// the state was pushed, but before the next state was pushed.
		spliced_stack<_partial_finished_capture_info> _state_partial_finished_captures;
		/// A spliced stack that stores all captures that started and finished after a state was pushed but before
		/// the next state was pushed.
		spliced_stack<_finished_capture_info> _state_finished_captures;
		/// A spliced stack that stores subroutines that started before a state, and finished after the state is
		/// pushed but before the next state was pushed.
		spliced_stack<_subroutine_stackframe> _state_finished_subroutines;
		/// A spliced stack that stores checkpoints that were set before a state was pushed, and finished after the
		/// state was pushed and before the next state was pushed.
		spliced_stack<_checkpointed_stream> _state_restored_checkpoints;
		/// A spliced stack that stores stream positions saved before a state was pushed, and was popped after the
		/// state was pushed but before the next state was pushed.
		spliced_stack<_stream_position> _state_finished_stream_positions;

		/// Logs the given string.
		void _log([[maybe_unused]] std::u8string_view str) {
			if constexpr (_uses_log) {
				debug_log << str;
			}
		}
		/// Logs information about the given state.
		void _log(
			[[maybe_unused]] const _state&,
			[[maybe_unused]] const _state *next,
			[[maybe_unused]] std::u8string_view indent
		);

		/// Finds the index of the first matched group that has the given name.
		[[nodiscard]] std::optional<typename compiled_types::capture_ref> _find_matched_named_capture(
			typename compiled_types::capture_name_ref name
		) const {
			for (auto id : _expr->get_named_captures().get_indices_for_name(name)) {
				if (id.get_index() >= _result.captures.size()) {
					// since the captures are ordered, no captures after this one can be matched
					break;
				}
				if (_result.captures[id.get_index()].is_valid()) {
					return id;
				}
			}
			return std::nullopt; // capture not yet matched
		}

		/// Checks if a backreference that has already been matched matches the string starting from the current
		/// position.
		[[nodiscard]] bool _check_backreference_transition(
			Stream&, typename compiled_types::capture_ref, bool case_insensitive
		) const;

		/// Blanket overload that handles all conditions that always passes.
		template <typename Condition> [[nodiscard]] bool _check_transition(const Stream&, const Condition&) const {
			return true;
		}
		/// Checks if the contents in the given stream starts with the given string.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const typename compiled_types::transitions::literal &cond
		) const {
			for (codepoint cp : cond.contents) {
				if (stream.empty()) {
					return false;
				}
				codepoint got_codepoint = stream.take();
				if (cond.case_insensitive) {
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
			Stream &stream, const typename compiled_types::transitions::character_class &cond
		) const {
			if (stream.empty()) {
				return false;
			}
			return cond.matches(stream.take());
		}
		/// Checks if the assertion is satisfied.
		[[nodiscard]] bool _check_transition(
			Stream stream, const typename compiled_types::transitions::simple_assertion &cond
		) const {
			switch (cond.assertion_type) {
			case ast_nodes::simple_assertion::type::always_false:
				return false;

			case ast_nodes::simple_assertion::type::line_start:
				{
					if (stream.prev_empty()) {
						return true;
					}
					if (stream.empty()) {
						return false; // ignore final blank line
					}
					Stream checkpoint = stream;
					if (consume_line_ending_backwards(stream) != line_ending::none) {
						return !is_within_crlf(checkpoint);
					}
					return false;
				}

			case ast_nodes::simple_assertion::type::line_end:
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

			case ast_nodes::simple_assertion::type::subject_start:
				return stream.prev_empty();

			case ast_nodes::simple_assertion::type::subject_end_or_trailing_newline:
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

			case ast_nodes::simple_assertion::type::subject_end:
				return stream.empty();
			case ast_nodes::simple_assertion::type::range_start:
				return false; // TODO
			}

			assert_true_logical(false, "invalid assertion type"); // should not happen
			return false;
		}
		/// Checks if the assertion is satisfied.
		[[nodiscard]] bool _check_transition(
			Stream stream, const typename compiled_types::transitions::character_class_assertion &cond
		) const {
			bool prev_in =
				stream.prev_empty() ? false : cond.char_class.matches(stream.peek_prev());
			bool next_in =
				stream.empty() ? false : cond.char_class.matches(stream.peek());
			return cond.boundary ? prev_in != next_in : prev_in == next_in;
		}
		/// Checks if a numbered backreference matches.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const typename compiled_types::transitions::numbered_backreference &cond
		) const {
			if (
				cond.capture.get_index() >= _result.captures.size() ||
				!_result.captures[cond.capture.get_index()].is_valid()
			) {
				return false; // capture not yet matched
			}
			return _check_backreference_transition(stream, cond.capture, cond.case_insensitive);
		}
		/// Checks if a named backreference matches.
		[[nodiscard]] bool _check_transition(
			Stream &stream, const typename compiled_types::transitions::named_backreference &cond
		) const {
			if (auto id = _find_matched_named_capture(cond.name)) {
				return _check_backreference_transition(stream, id.value(), cond.case_insensitive);
			}
			return false;
		}
		/// Checks if we're in an infinite loop.
		[[nodiscard]] bool _check_transition(
			const Stream &stream, const typename compiled_types::transitions::check_infinite_loop&
		) {
			auto pos = _stream_position_stack.back();
			_stream_position_stack.pop_back();
			if (pos.state_stack_size < _state_stack.size()) {
				_state_finished_stream_positions.emplace(pos);
			}
			return stream.codepoint_position() != pos.codepoint_position;
		}
		/// Makes sure that there is enough room for the rewind.
		[[nodiscard]] bool _check_transition(
			const Stream &stream, const typename compiled_types::transitions::rewind &trans
		) const {
			return stream.codepoint_position() >= trans.num_codepoints;
		}
		/// Checks if we're currently in a recursion.
		[[nodiscard]] bool _check_transition(
			const Stream&, const typename compiled_types::transitions::conditions::any_recursion&
		) const {
			return !_subroutine_stack.empty();
		}
		/// Checks if we're currently in a numbered recursion.
		[[nodiscard]] bool _check_transition(
			const Stream&, const typename compiled_types::transitions::conditions::numbered_recursion &cond
		) const {
			if (!_subroutine_stack.empty()) {
				return _subroutine_stack.back().subroutine_capture == cond.capture;
			}
			return false;
		}
		/// Checks if we're currently in a named recursion.
		[[nodiscard]] bool _check_transition(
			const Stream&, const typename compiled_types::transitions::conditions::named_recursion &cond
		) const {
			if (!_subroutine_stack.empty()) {
				auto current_name = _expr->get_named_captures().get_name_index_for_group(
					_subroutine_stack.back().subroutine_capture
				);
				return current_name == cond.name;
			}
			return false;
		}
		/// Checks if the given numbered group has been matched.
		[[nodiscard]] bool _check_transition(
			const Stream&, const typename compiled_types::transitions::conditions::numbered_capture &cap
		) const {
			return
				_result.captures.size() > cap.capture.get_index() &&
				_result.captures[cap.capture.get_index()].is_valid();
		}
		/// Checks if the given numbered group has been matched.
		[[nodiscard]] bool _check_transition(
			const Stream&, const typename compiled_types::transitions::conditions::named_capture &cap
		) const {
			return _find_matched_named_capture(cap.name).has_value();
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
				_state_stack.back().finished_captures.emplace_back(cap, index);
			}
			_result.captures[index] = std::move(cap);
		}

		/// By default nothing extra needs to be done for transitions.
		template <typename Trans> void _execute_transition(const Stream&, const Trans&) {
		}
		/// Starts a capture.
		void _execute_transition(Stream stream, const typename compiled_types::transitions::capture_begin &beg) {
			_ongoing_captures.emplace_back(std::move(stream), beg.capture);
		}
		/// Ends a capture.
		void _execute_transition(const Stream&, const typename compiled_types::transitions::capture_end&);
		/// Resets the start of this match.
		void _execute_transition(
			Stream stream, const typename compiled_types::transitions::reset_match_start&
		) {
			if (!_state_stack.empty()) {
				_state_stack.back().initial_match_begin = _result.overriden_match_begin;
			}
			_result.overriden_match_begin = std::move(stream);
		}
		/// Pushes an atomic group.
		void _execute_transition(const Stream&, const typename compiled_types::transitions::push_atomic&) {
			_atomic_stack_sizes.emplace_back(_state_stack.size());
		}
		/// Pops all states associated with the current atomic group.
		void _execute_transition(const Stream&, const typename compiled_types::transitions::pop_atomic&);
		/// Pushes and checkpoints the current stream.
		void _execute_transition(Stream, const typename compiled_types::transitions::push_stream_checkpoint&);
		/// Restores a previously checkpointed stream.
		void _execute_transition(Stream&, const typename compiled_types::transitions::restore_stream_checkpoint&);
		/// Pushes a subroutine stack frame.
		void _execute_transition(const Stream&, const typename compiled_types::transitions::jump&);
		/// Pushes the current stream position
		void _execute_transition(const Stream &stream, const typename compiled_types::transitions::push_position&) {
			auto &pushed = _stream_position_stack.emplace_back();
			pushed.codepoint_position = stream.codepoint_position();
			pushed.state_stack_size = _state_stack.size();
		}
		/// Rewinds the stream.
		void _execute_transition(Stream &stream, const typename compiled_types::transitions::rewind &trans) {
			for (std::size_t i = 0; i < trans.num_codepoints; ++i) {
				stream.prev();
			}
		}
		/// Sets the mark.
		void _execute_transition(const Stream&, const typename compiled_types::transitions::verbs::mark &trans) {
			marker = trans.marker;
		}
	};
}
