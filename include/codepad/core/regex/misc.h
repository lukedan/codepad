// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous regex-related classes and definitions.

#include "codepad/core/unicode/common.h"
#include "codepad/core/text.h"
#include "codepad/core/encodings.h"

namespace codepad::regex {
	/// Input stream type for a pair of iterators.
	template <typename Encoding, typename Iter> struct basic_input_stream {
	public:
		/// Default constructor.
		basic_input_stream() = default;
		/// Initializes \ref _string.
		basic_input_stream(Iter beg, Iter end) : _beg(beg), _cur(beg), _next(beg), _end(end) {
			if (_next != _end) {
				if (!Encoding::next_codepoint(_next, _end, _cp)) {
					_cp = unicode::replacement_character;
				}
			}
		}

		/// Returns whether _position is at the end of the string.
		[[nodiscard]] bool empty() const {
			return _cur == _end;
		}
		/// Returns the current character and increments \ref _position.
		codepoint take() {
			assert_true_logical(!empty(), "taking from an empty stream");
			codepoint res = _cp;
			_cur = _next;
			if (_next != _end) {
				if (!Encoding::next_codepoint(_next, _end, _cp)) {
					_cp = unicode::replacement_character;
				}
			}
			++_pos;
			return res;
		}
		/// Returns the current character.
		[[nodiscard]] codepoint peek() const {
			assert_true_logical(!empty(), "peeking an empty stream");
			return _cp;
		}

		/// Returns whether it's possible to move to the previous codepoint.
		[[nodiscard]] bool prev_empty() const {
			return _cur == _beg;
		}
		/// Returns the previous character and decrements \ref _position;
		codepoint prev() {
			assert_true_logical(!prev_empty(), "cannot move stream backwards");
			_next = _cur;
			--_pos;
			auto [it, cp] = _prev(_cur);
			_cur = it;
			_cp = cp;
			return _cp;
		}
		/// Returns the previous character without moving this stream.
		[[nodiscard]] codepoint peek_prev() const {
			assert_true_logical(!prev_empty(), "peeking an empty stream backwards");
			return _prev(_cur).second;
		}

		/// Returns the current codepoint position.
		[[nodiscard]] std::size_t codepoint_position() const {
			return _pos;
		}
		/// Returns the current byte position.
		template <typename Ret = decltype(Iter() - Iter())> [[nodiscard]] Ret byte_position() const {
			return _cur - _beg;
		}
	protected:
		std::size_t _pos = 0; ///< Position of the stream in codepoints.
		codepoint _cp = 0; /// The current codepoint.
		Iter _beg{}; ///< Points at the beginning of the stream.
		Iter _cur{}; ///< Points to the first byte of \ref _cp.
		Iter _next{}; ///< Points past the last byte of \ref _cp.
		Iter _end{}; ///< Points past the end of the stream.

		/// Returns the codepoint before the given iterator and its starting position.
		[[nodiscard]] std::pair<Iter, codepoint> _prev(Iter it) const {
			if (it == _end && !encodings::is_position_aligned<Encoding>(it - _beg)) {
				encodings::align_iterator<Encoding>(it, _beg);
				return { it, unicode::replacement_character };
			}
			codepoint cp;
			if (!Encoding::previous_codepoint(it, _beg, cp)) {
				cp = unicode::replacement_character;
			}
			return { it, cp };
		}
	};
	/// Creates a new input stream from the given pair of iterators.
	template <
		typename Encoding, typename Iter
	> basic_input_stream<Encoding, std::decay_t<Iter>> make_basic_input_stream(Iter beg, Iter end) {
		return basic_input_stream<Encoding, std::decay_t<Iter>>(std::move(beg), std::move(end));
	}


	/// Consumes a line ending from the given stream.
	///
	/// \return Type of the consumed line ending.
	template <typename Stream> line_ending consume_line_ending(Stream &s) {
		if (s.empty()) {
			return line_ending::none;
		}
		switch (s.peek()) {
		case U'\r':
			s.take();
			if (s.empty() || s.peek() != U'\n') {
				return line_ending::r;
			}
			s.take();
			return line_ending::rn;
		case U'\n':
			s.take();
			return line_ending::n;
		}
		return line_ending::none;
	}
	/// Similar to \ref consume_line_ending(), but backwards.
	template <typename Stream> line_ending consume_line_ending_backwards(Stream &s) {
		if (s.prev_empty()) {
			return line_ending::none;
		}
		switch (s.peek_prev()) {
		case U'\n':
			s.prev();
			if (s.prev_empty() || s.peek_prev() != U'\r') {
				return line_ending::n;
			}
			s.prev();
			return line_ending::rn;
		case U'\r':
			s.prev();
			return line_ending::r;
		}
		return line_ending::none;
	}
	/// Tests if the stream is currently at the boundary between a CR and a LF.
	template <typename Stream> [[nodiscard]] bool is_within_crlf(Stream &s) {
		if (s.empty() || s.prev_empty()) {
			return false;
		}
		return s.peek() == U'\n' && s.peek_prev() == U'\r';
	}

	/// Regular expresion options. All options are disabled by default.
	struct options {
		bool case_insensitive = false; ///< Case insensitive matching.
		bool multiline = false; ///< ^ and $ matches new lines.
		bool no_auto_capture = false; ///< Only named captures are available.
		bool dot_all = false; ///< . Matches any character.
		bool extended = false; ///< Ignore new lines and # comments.
		bool extended_more = false; ///< Similar to \ref extended, but also ignores spaces in character classes.
		bool global = false; ///< Whether or not to enable global matching.
	};

	/// Codepoint ranges used by the parser.
	namespace tables {
		/// Returns the list of whitespaces that are ignored in extended mode.
		[[nodiscard]] const codepoint_range_list &extended_mode_whitespaces();
		/// Returns the list of horizontal whitespaces.
		[[nodiscard]] const codepoint_range_list &horizontal_whitespaces();
		/// Returns the list of vertical whitespaces.
		[[nodiscard]] const codepoint_range_list &vertical_whitespaces();
		/// Returns the list of `word' characters.
		[[nodiscard]] const codepoint_range_list &word_characters();
		/// The list of unicode new line characters.
		[[nodiscard]] const codepoint_range_list &newline_characters();
		/// The list of POSIX white spaces.
		[[nodiscard]] const codepoint_range_list &posix_spaces();
	}
}
