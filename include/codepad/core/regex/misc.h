// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Miscellaneous regex-related classes and definitions.

#include "codepad/core/unicode/common.h"
#include "codepad/core/text.h"

namespace codepad::regex {
	/// Input stream type for basic strings.
	template <typename Encoding> struct basic_string_input_stream {
	public:
		/// Default constructor.
		basic_string_input_stream() = default;
		/// Initializes \ref _string.
		basic_string_input_stream(const std::byte *beg, const std::byte *end) : _cur(beg), _end(end) {
			if (_cur != _end) {
				if (!Encoding::next_codepoint(_cur, _end, _cp)) {
					_cp = unicode::replacement_character;
				}
				--_cur;
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
			if (++_cur != _end) {
				if (!Encoding::next_codepoint(_cur, _end, _cp)) {
					_cp = unicode::replacement_character;
				}
				--_cur;
			}
			++_pos;
			return res;
		}
		/// Returns the current character.
		[[nodiscard]] codepoint peek() {
			assert_true_logical(!empty(), "peeking an empty stream");
			return _cp;
		}

		/// Returns the current position of this stream.
		[[nodiscard]] std::size_t position() const {
			return _pos;
		}
	protected:
		std::size_t _pos = 0; ///< Position of the stream in codepoints.
		codepoint _cp = 0; /// The current codepoint.
		const std::byte *_cur = nullptr; ///< Last (not past!) byte of \ref _cp.
		const std::byte *_end = nullptr; ///< Pointer past the final byte.
	};

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

	/// Regular expresion options. All options are disabled by default.
	struct options {
		bool case_insensitive = false; ///< Case insensitive matching.
		bool multiline = false; ///< ^ and $ matches new lines.
		bool no_auto_capture = false; ///< Only named captures are available.
		bool dot_all = false; ///< . Matches any character.
		bool extended = false; ///< Ignore new lines and # comments.
		bool extended_more = false; ///< Similar to \ref extended, but also ignores spaces in character classes.
	};

	/// Codepoint ranges used by the parser.
	namespace tables {
		/// Returns the list of whitespaces that are ignored in extended mode.
		[[nodiscard]] const codepoint_range_list &extended_mode_whitespaces();
	}
}
