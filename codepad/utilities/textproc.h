#pragma once

#include "textconfig.h"

namespace codepad {
	inline bool is_newline(char_t c) {
		return c == '\n' || c == '\r';
	}
}