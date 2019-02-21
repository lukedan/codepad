// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "caret_set.h"

/// \file
/// Implementation of certain methods of \ref caret_set.

#include "interpretation.h"

namespace codepad::editors::code {
	void caret_set::calculate_byte_positions(const interpretation &interp) {
		if (bytepos_valid) {
			return;
		}
		interpretation::character_position_converter cvt(interp);
		for (auto &pair : carets) {
			if (pair.first.first == pair.first.second) {
				pair.second.bytepos_first = pair.second.bytepos_second = cvt.character_to_byte(pair.first.first);
			} else if (pair.first.first < pair.first.second) { // calculate for the position in front first
				pair.second.bytepos_first = cvt.character_to_byte(pair.first.first);
				pair.second.bytepos_second = cvt.character_to_byte(pair.first.second);
			} else {
				pair.second.bytepos_second = cvt.character_to_byte(pair.first.second);
				pair.second.bytepos_first = cvt.character_to_byte(pair.first.first);
			}
		}
		bytepos_valid = true;
	}
}
