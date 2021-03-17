// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "codepad/editors/code/theme.h"

/// \file
/// Implementation of the theme provider registry.

#include "codepad/editors/code/interpretation.h"

namespace codepad::editors::code {
	text_theme_provider_registry::provider_modifier::~provider_modifier() {
		_interp.appearance_changed.invoke_noret(interpretation::appearance_change_type::layout_and_visual);
	}


	void text_theme_provider_registry::remove_provider(token &tok) {
		assert_true_logical(tok._interpretation, "empty theme provider token");
		_providers.erase(tok._it);
		_interpretation.appearance_changed.invoke_noret(interpretation::appearance_change_type::layout_and_visual);
		tok._interpretation = nullptr;
	}
}
