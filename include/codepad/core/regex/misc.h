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
		/// Not reversed.
		[[nodiscard]] bool is_reversed() const {
			return false;
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

	/// A wrapper around another stream that reverses it.
	template <typename Stream> struct basic_reverse_stream {
	public:
		using original_stream_t = Stream; ///< Original stream type.

		/// Default constructor.
		basic_reverse_stream() = default;
		/// Initializes the underlying stream.
		explicit basic_reverse_stream(Stream s) : _s(std::move(s)) {
		}

		/// Returns \ref _empty.
		[[nodiscard]] bool empty() const {
			return _s.prev_empty();
		}
		/// Takes a codepoint from this reversed stream.
		codepoint take() {
			return _s.prev();
		}
		/// Previews the next codepoint in this reversed stream.
		[[nodiscard]] codepoint peek() const {
			return _s.peek_prev();
		}

		/// Returns \ref _next_empty.
		[[nodiscard]] bool prev_empty() const {
			return _s.empty();
		}
		/// Returns the previous character and decrements \ref _position;
		codepoint prev() {
			return _s.take();
		}
		/// Peeks the previous codepoint.
		[[nodiscard]] codepoint peek_prev() const {
			return _s.peek();
		}

		/// This stream is reversed if the underlying stream is not, and vice versa.
		[[nodiscard]] bool is_reversed() const {
			return !_s.is_reversed();
		}

		/// Returns the original unreversed stream.
		[[nodiscard]] const Stream &get_original_stream() const {
			return _s;
		}
	protected:
		Stream _s; ///< The underlying stream.
	};

	/// Helper class used to obtain the type of a reversed stream. By default \ref basic_reverse_stream is used.
	template <typename Stream> struct reversed_stream_type {
		using type = basic_reverse_stream<Stream>; ///< Reversed stream type using \ref basic_reverse_stream.
	};
	/// Shorthand for \ref reversed_stream_type::type.
	template <typename Stream> using reversed_stream_type_t = typename reversed_stream_type<Stream>::type;
	/// For \ref basic_reverse_stream types, the reversed stream type is the original stream type.
	template <typename Stream> struct reversed_stream_type<basic_reverse_stream<Stream>> {
		using type = typename basic_reverse_stream<Stream>::original_stream_t; ///< Original stream type.
	};

	/// Creates a reversed stream from the given stream.
	template <
		typename Stream
	> [[nodiscard]] basic_reverse_stream<std::decay_t<Stream>> make_reverse_stream(Stream &&s) {
		return basic_reverse_stream<std::decay_t<Stream>>(std::forward<Stream>(s));
	}
	/// Overload for reversed streams - returns the original stream.
	template <typename Stream> [[nodiscard]] Stream make_reverse_stream(const basic_reverse_stream<Stream> &stream) {
		return stream.get_original_stream();
	}


	/// Consumes a line ending from the given stream.
	///
	/// \return Type of the consumed line ending.
	template <typename Stream> line_ending consume_line_ending(Stream &s) {
		if (s.empty()) {
			return line_ending::none;
		}
		if (s.is_reversed()) {
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
		} else {
			switch (s.peek()) {
			case U'\n':
				s.take();
				if (s.empty() || s.peek() != U'\r') {
					return line_ending::n;
				}
				s.take();
				return line_ending::rn;
			case U'\r':
				s.take();
				return line_ending::r;
			}
		}
		return line_ending::none;
	}
	/// Tests if the stream is currently at the boundary between a CR and a LF.
	template <typename Stream> [[nodiscard]] bool is_within_crlf(Stream &s) {
		if (s.empty() || s.prev_empty()) {
			return false;
		}
		if (s.is_reversed()) {
			return s.peek() == U'\r' && s.peek_prev() == U'\n';
		} else {
			return s.peek() == U'\n' && s.peek_prev() == U'\r';
		}
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
