// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/fragment_generation.h"

/// \file
/// Implementation of certain fragment generation functionalities.

#include "codepad/editors/code/contents_region.h"

namespace codepad::editors::code {
	/// Initializes this struct using the given \ref contents_region.
	fragment_assembler::fragment_assembler(const contents_region &rgn) : fragment_assembler(
		rgn.get_manager().get_renderer(), *rgn.get_font_families()[0], rgn.get_font_size(),
		rgn.get_line_height(), rgn.get_baseline(), rgn.get_tab_width()
	) {
	}
}
