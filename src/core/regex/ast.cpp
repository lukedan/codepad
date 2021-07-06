// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/core/regex/ast.h"

/// \file
/// Implementation of certain AST functionalities.

namespace codepad::regex::ast::nodes {
	codepoint_range_list character_class::get_effective_ranges() const {
		if (is_negate) {
			codepoint last = 0;
			codepoint_range_list result;
			auto iter = ranges.ranges.begin();
			if (!ranges.ranges.empty() && ranges.ranges.front().first == 0) {
				last = ranges.ranges.front().last + 1;
				++iter;
			}
			for (; iter != ranges.ranges.end(); ++iter) {
				result.ranges.emplace_back(last, iter->first - 1);
				last = iter->last + 1;
			}
			if (last <= unicode::codepoint_max) {
				result.ranges.emplace_back(last, unicode::codepoint_max);
			}
			return result;
		}
		return ranges;
	}
}
