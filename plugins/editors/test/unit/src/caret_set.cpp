// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Tests for \ref codepad::editors::caret_set_base.

#include <catch.hpp>

#include <codepad/editors/binary/caret_set.h>

using caret_set = codepad::editors::binary::caret_set;
using caret_data = codepad::editors::binary::caret_data;
using caret_selection = codepad::ui::caret_selection;
using caret_list = std::vector<caret_selection>;

namespace Catch {
	template <> struct StringMaker<caret_selection> {
		static std::string convert(caret_selection val) {
			return
				"[" + std::to_string(val.selection_begin) +
				", " + std::to_string(val.get_caret_position()) +
				", " + std::to_string(val.get_selection_end()) + "]";
		}
	};
}

/// Flattens a \ref caret_set into a \ref caret_list.
[[nodiscard]] caret_list flatten_caret_set(const caret_set &set) {
	caret_list result;
	for (auto it = set.begin(); it.get_iterator() != set.carets.end(); it.move_next()) {
		result.emplace_back(it.get_caret_selection());
	}
	return result;
}

TEST_CASE("Manipulating a caret_set", "[caret_set]") {
	caret_set set;
	REQUIRE(flatten_caret_set(set) == caret_list({ }));

	// test that the carets are added correctly
	set.add(caret_selection(30, 20, 10), caret_data());
	REQUIRE(flatten_caret_set(set) == caret_list({ caret_selection(30, 20, 10) }));

	// inserting before all carets, no merging
	set.add(caret_selection(5, 20, 5), caret_data());
	REQUIRE(flatten_caret_set(set) == caret_list({
		caret_selection(5, 20, 5), caret_selection(30, 20, 10)
	}));

	// inserting after all carets, no merging
	set.add(caret_selection(60, 10, 5), caret_data());
	REQUIRE(flatten_caret_set(set) == caret_list({
		caret_selection(5, 20, 5), caret_selection(30, 20, 10), caret_selection(60, 10, 5)
	}));

	// inserting between two carets, no merging
	set.add(caret_selection(52, 5, 5), caret_data());
	REQUIRE(flatten_caret_set(set) == caret_list({
		caret_selection(5, 20, 5), caret_selection(30, 20, 10), caret_selection(52, 5, 5), caret_selection(60, 10, 5)
	}));

	SECTION("Resetting a caret_set") {
		set.reset();
		REQUIRE(flatten_caret_set(set) == caret_list({ caret_selection(0, 0, 0) }));
	}

	SECTION("Inserting carets with merging") {
		// merging before
		set.add(caret_selection(20, 7, 5), caret_data());
		REQUIRE(flatten_caret_set(set) == caret_list({
			caret_selection(5, 22, 20), caret_selection(30, 20, 10), caret_selection(52, 5, 5), caret_selection(60, 10, 5)
		}));

		// merging before, touching
		set.add(caret_selection(20, 10, 5), caret_data());
		REQUIRE(flatten_caret_set(set) == caret_list({
			caret_selection(5, 25, 20), caret_selection(30, 20, 10), caret_selection(52, 5, 5), caret_selection(60, 10, 5)
		}));

		// covering another caret
		set.add(caret_selection(51, 8, 3), caret_data());
		REQUIRE(flatten_caret_set(set) == caret_list({
			caret_selection(5, 25, 20), caret_selection(30, 20, 10), caret_selection(51, 8, 3), caret_selection(60, 10, 5)
		}));

		// covering another caret, and merging at front & back
		set.add(caret_selection(40, 25, 5), caret_data());
		REQUIRE(flatten_caret_set(set) == caret_list({
			caret_selection(5, 25, 20), caret_selection(30, 40, 15)
		}));
	}

	SECTION("Removing carets") {
		set.remove(++set.carets.begin());
		REQUIRE(flatten_caret_set(set) == caret_list({
			caret_selection(5, 20, 5), caret_selection(52, 5, 5), caret_selection(60, 10, 5)
		}));
	}
}
