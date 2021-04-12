// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Fuzz test for caret manipulation.

#include <codepad/core/fuzz_test.h>
#include <codepad/editors/caret_set.h>

namespace cp = codepad;

/// The caret set class with an \p int associated with each caret.
class caret_set : public cp::editors::caret_set_base<int, caret_set> {
};

enum class test_op : unsigned char {
	insert, ///< Inserting a caret.
	erase, ///< Erasing a caret.

	point_query, ///< Point query.

	max_value ///< Max value used for random number generation.
};

/// Range of test operation indices.
std::pair<unsigned, unsigned> op_range{ 0, static_cast<unsigned>(test_op::max_value) - 1 };
std::pair<std::size_t, std::size_t>
	caret_begin_range{ 0, 5000 }, ///< The range of the starting position of a caret.
	caret_length_range{ 0, 200 }; ///< The range of the length of a caret.
/// Possible range of caret data.
std::pair<int, int> data_range{ std::numeric_limits<int>::min(), std::numeric_limits<int>::max() };

/// Class for the test.
class caret_test : public cp::fuzz_test {
public:
	/// Returns the name of this test.
	std::u8string_view get_name() const override {
		return u8"caret_test";
	}

	/// Logs the current number of carets.
	void log_status(cp::logger::log_entry &entry) const override {
		entry << "Num carets: " << _reference.size();
	}

	/// Runs one test.
	void iterate() override {
		auto op = static_cast<test_op>(random_int(op_range));
		bool is_modification = false;
		switch (op) {
		case test_op::insert:
			{
				cp::ui::caret_selection caret;
				if (random_double() < 0.1) { // 10% chance: no selection
					caret.selection_begin = random_int(caret_begin_range);
				} else {
					caret.selection_begin = random_int(caret_begin_range);
					caret.selection_length = random_int(caret_length_range);
					double x = random_double();
					if (x < 0.1) { // 10% chance: caret at beginning
						caret.caret_offset = 0;
					} else if (x < 0.2) { // 10% chance: caret at end
						caret.caret_offset = caret.selection_length;
					} else {
						caret.caret_offset = random_int<std::size_t>(0, caret.selection_length);
					}
				}
				int data = random_int(data_range);

				_carets.add(caret, data);

				// insert into reference
				std::size_t new_beg = caret.selection_begin, new_end = caret.get_selection_end();
				std::optional<std::vector<std::pair<cp::ui::caret_selection, int>>::const_iterator> insert_before;
				for (auto it = _reference.begin(); it != _reference.end(); ) {
					if (it->first.get_selection_end() < caret.selection_begin || (
						it->first.get_selection_end() == caret.selection_begin &&
						it->first.selection_length > 0 && caret.selection_length > 0
					)) {
						++it;
						continue;
					}
					if (it->first.selection_begin > caret.get_selection_end() || (
						it->first.selection_begin == caret.get_selection_end() &&
						it->first.selection_length > 0 && caret.selection_length > 0
					)) {
						insert_before = it; // insert right here
						break;
					}
					new_beg = std::min(new_beg, it->first.selection_begin);
					new_end = std::max(new_end, it->first.get_selection_end());
					it = _reference.erase(it);
					insert_before = _reference.end();
				}
				caret.caret_offset = caret.get_caret_position() - new_beg;
				caret.selection_begin = new_beg;
				caret.selection_length = new_end - new_beg;
				_reference.insert(insert_before.value_or(_reference.end()), std::make_pair(caret, data));

				is_modification = true;
				break;
			}
		case test_op::erase:
			{
				if (_reference.empty()) {
					break;
				}

				std::size_t point = random_int<std::size_t>(0, _reference.back().first.get_selection_end());
				auto it = _carets.find_first_ending_at_or_after(point);
				auto refit = _reference.begin();
				for (; refit != _reference.end(); ++refit) {
					if (refit->first.get_selection_end() >= point) {
						break;
					}
				}

				// don't erase if the point does not intersect with the selection - so that we always have a good
				// number of carets in play
				if (it.get_iterator() == _carets.carets.end() || it.get_selection_begin() > point) {
					break;
				}

				_carets.remove(it.get_iterator());
				_reference.erase(refit);

				is_modification = true;
				break;
			}

		case test_op::point_query:
			{
				std::size_t point = random_int(caret_begin_range);
				auto it = _carets.find_first_ending_at_or_after(point);
				auto refit = _reference.begin();
				for (; refit != _reference.end(); ++refit) {
					if (refit->first.get_selection_end() >= point) {
						break;
					}
				}

				if (it.get_iterator() == _carets.carets.end()) {
					cp::assert_true_logical(refit == _reference.end(), "invalid point query result");
				} else {
					cp::assert_true_logical(refit->first == it.get_caret_selection(), "invalid point query result");
					cp::assert_true_logical(refit->second == it.get_iterator()->data, "invalid point query result");
				}

				break;
			}
		default:
			cp::assert_true_logical(false, "invalid operation");
		}

		if (is_modification) {
			auto it = _carets.begin();
			for (auto refit = _reference.begin(); it.get_iterator() != _carets.carets.end(); it.move_next(), ++refit) {
				cp::assert_true_logical(refit != _reference.end(), "missing carets in caret_set");
				cp::assert_true_logical(it.get_caret_selection() == refit->first, "incorrect caret position");
				cp::assert_true_logical(it.get_iterator()->data == refit->second, "incorrect caret data");
			}
			cp::assert_true_logical(it.get_iterator() == _carets.carets.end(), "too many carets in caret_set");
		}
	}
protected:
	caret_set _carets; ///< The set of carets.
	std::vector<std::pair<cp::ui::caret_selection, int>> _reference; ///< Reference data.
};

/// Entry point of the test.
int main(int argc, char **argv) {
	return cp::fuzz_test::main(argc, argv, std::make_unique<caret_test>());
}
