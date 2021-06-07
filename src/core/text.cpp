// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/text.h"

/// \file
/// Implementation of string utilities.

namespace codepad {
	std::size_t get_line_ending_length(line_ending le) {
		switch (le) {
		case line_ending::none:
			return 0;
		case line_ending::n:
			[[fallthrough]];
		case line_ending::r:
			return 1;
		case line_ending::rn:
			return 2;
		}
		return 0;
	}

	std::u32string_view line_ending_to_string(line_ending le) {
		switch (le) {
		case line_ending::r:
			return U"\r";
		case line_ending::n:
			return U"\n";
		case line_ending::rn:
			return U"\r\n";
		default:
			return U"";
		}
	}
}
