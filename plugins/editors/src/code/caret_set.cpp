// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/caret_set.h"

/// \file
/// Implementation of certain methods of \ref codepad::editors::code::caret_set.

#include "codepad/editors/code/interpretation.h"

namespace codepad::editors::code {
	void caret_set::calculate_byte_positions(const interpretation &interp) {
		if (bytepos_valid) {
			return;
		}
		interpretation::character_position_converter cvt(interp);
		for (auto &pair : carets) {
			if (pair.first.caret == pair.first.selection) {
				pair.second.bytepos_first = pair.second.bytepos_second = cvt.character_to_byte(pair.first.caret);
			} else if (pair.first.caret < pair.first.selection) { // calculate for the position in front first
				pair.second.bytepos_first = cvt.character_to_byte(pair.first.caret);
				pair.second.bytepos_second = cvt.character_to_byte(pair.first.selection);
			} else {
				pair.second.bytepos_second = cvt.character_to_byte(pair.first.selection);
				pair.second.bytepos_first = cvt.character_to_byte(pair.first.caret);
			}
		}
		bytepos_valid = true;
	}
}
