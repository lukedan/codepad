// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

#pragma once

/// \file
/// Reads and dumps the Unicode Character Database.

#include <fstream>
#include <iostream>

#include <codepad/core/unicode/database.h>

namespace cp = codepad;

int main(int argc, char **argv) {
	const char *path = argc > 1 ? argv[1] : "thirdparty/ucd/UnicodeData.txt";
	auto database = cp::unicode::unicode_data::parse(path);
	for (const auto &entry : database.get_codepoints_in_category(cp::unicode::general_category::decimal_number).ranges) {
		std::cout << entry.first << " - " << entry.last << "\n";
	}
	return 0;
}
