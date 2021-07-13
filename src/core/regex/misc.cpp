// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/misc.h"

/// \file
/// Implementation of miscellaneous regex-related classes and definitions.

#include "codepad/core/unicode/database.h"

namespace codepad::regex {
	namespace tables {
		const codepoint_range_list &horizontal_whitespaces() {
			static codepoint_range_list _result;

			if (_result.ranges.empty()) {
				_result.ranges = {
					codepoint_range(0x0009), // horizontal tab
					codepoint_range(0x0020), // space
					codepoint_range(0x00A0), // non-break space
					codepoint_range(0x1680), // ogham space mark
					codepoint_range(0x180E), // mongolian vowel separator
					codepoint_range(0x2000), // en quad
					codepoint_range(0x2001), // em quad
					codepoint_range(0x2002), // en space
					codepoint_range(0x2003), // em space
					codepoint_range(0x2004), // three-per-em space
					codepoint_range(0x2005), // four-per-em space
					codepoint_range(0x2006), // six-per-em space
					codepoint_range(0x2007), // figure space
					codepoint_range(0x2008), // punctuation space
					codepoint_range(0x2009), // thin space
					codepoint_range(0x200A), // hair space
					codepoint_range(0x202F), // narrow no-break space
					codepoint_range(0x205F), // medium mathematical space
					codepoint_range(0x3000), // ideographic space
				};
				_result.sort_and_compact();
			}

			return _result;
		}

		const codepoint_range_list &vertical_whitespaces() {
			static codepoint_range_list _result;

			if (_result.ranges.empty()) {
				_result.ranges = {
					codepoint_range(0x000A), // linefeed
					codepoint_range(0x000B), // vertical tab
					codepoint_range(0x000C), // form feed
					codepoint_range(0x000D), // carriage return
					codepoint_range(0x0085), // next line
					codepoint_range(0x2028), // line separator
					codepoint_range(0x2029), // paragraph separator
				};
				_result.sort_and_compact();
			}

			return _result;
		}

		const codepoint_range_list &extended_mode_whitespaces() {
			static codepoint_range_list _result;

			if (_result.ranges.empty()) {
				_result.ranges = {
					codepoint_range(0x0009), // tab
					codepoint_range(0x000A), // linefeed
					codepoint_range(0x000B), // vertical tab
					codepoint_range(0x000C), // formfeed
					codepoint_range(0x000D), // carriage return
					codepoint_range(0x0020), // space
					codepoint_range(0x0085), // next line
					codepoint_range(0x200E), // left-to-right mark
					codepoint_range(0x200F), // right-to-left mark
					codepoint_range(0x2028), // line separator
					codepoint_range(0x2029), // paragraph separator
				};
				_result.sort_and_compact();
			}

			return _result;
		}

		const codepoint_range_list &word_characters() {
			static codepoint_range_list _result;

			if (_result.ranges.empty()) {
				_result = unicode::unicode_data::cache::get_codepoints_in_category(
					unicode::general_category::letter | unicode::general_category::number
				);
				_result.ranges.emplace_back(U'_');
				_result.sort_and_compact();
			}

			return _result;
		}

		const codepoint_range_list &newline_characters() {
			static codepoint_range_list _result;

			if (_result.ranges.empty()) {
				_result.ranges = {
					codepoint_range(0x000A), // line feed
					codepoint_range(0x000B), // vertical tab
					codepoint_range(0x000C), // form feed
					codepoint_range(0x000D), // carriage return
					codepoint_range(0x0085), // next line
					codepoint_range(0x2028), // line separator
					codepoint_range(0x2029), // paragraph separator
				};
				_result.sort_and_compact();
			}

			return _result;
		}
	}
}
