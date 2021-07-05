// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/misc.h"

/// \file
/// Implementation of miscellaneous regex-related classes and definitions.

#include "codepad/core/unicode/database.h"

namespace codepad::regex {
	namespace tables {
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
					codepoint_range(0x2029) // paragraph separator
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
