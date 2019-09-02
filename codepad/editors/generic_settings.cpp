// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#include "editor.h"

/// \file
/// Generic settings that apply to all types of editors.

using namespace std;

namespace codepad::editors {
	settings::retriever_parser<double> &editor::get_font_size_setting() {
		static settings::retriever_parser<double> _setting = settings::get().create_retriever_parser<double>(
			{u8"code_editor", u8"font_size"}, settings::basic_parsers::basic_type_with_default<double>(12.0)
			);
		return _setting;
	}

	settings::retriever_parser<vector<str_t>> &editor::get_interaction_modes_setting() {
		static settings::retriever_parser<vector<str_t>> _setting =
			settings::get().create_retriever_parser<vector<str_t>>(
				{u8"code_editor", u8"interaction_modes"},
				settings::basic_parsers::basic_type_with_default(vector<str_t>(), json::array_parser<str_t>())
				);
		return _setting;
	}
}
