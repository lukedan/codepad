// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Fuzz-test for the range registry.

#include <csignal>
#include <algorithm>
#include <random>
#include <atomic>

#include <codepad/core/fuzz_test.h>
#include <codepad/editors/overlapping_range_registry.h>

namespace cp = codepad;

using test_t = int; ///< Value type used for testing.

/// Ranges used by the reference.
struct range {
	/// Default constructor.
	range() = default;
	/// Initializes all fields.
	range(std::size_t beg, std::size_t len, test_t val) : begin(beg), length(len), value(val) {
	}

	std::size_t
		begin = 0, ///< The beginning position.
		length = 0; ///< The past-end position.
	test_t value = 0; ///< The value.

	/// Equality.
	friend bool operator==(const range&, const range&) = default;
};

/// Test operations to run.
enum class test_op : unsigned char {
	/// Inserting ranges. The ranges are inserted before existing ranges that start the same position.
	insert_ranges_before = 0,
	/// Inserting ranges. The ranges are inserted after existing ranges that start the same position.
	insert_ranges_after,
	erase_ranges, ///< Erasing ranges.
	on_modification, ///< Handling modifications.

	query_first_ending_after, ///< Querying the first range that ends after the given position.
	query_point, ///< Querying ranges that intersect with a specific point.
	query_range, ///< Querying ranges that intersect with a specific range.

	max_enum ///< Maximum value of this enum; used for random number generation.
};

std::pair<test_t, test_t> value_range{
	std::numeric_limits<test_t>::min(), std::numeric_limits<test_t>::max()
}; ///< Possible range of random values associated with each range.
std::pair<std::size_t, std::size_t>
	insert_count_range{ 500, 2000 }, ///< Possible range of the number of inserted ranges.
	erase_count_range{ 100, 1000 }, ///< Possible range of the number of erased ranges.

	position_range{ 0, 10000 }, ///< Possible range of the positions of inserted ranges.
	length_range{ 0, 3000 }, ///< Possible range of the lengths of inserted ranges.

	point_query_position_range{ 0, 15000 }, ///< Possible range of the position of point queries.
	range_query_position_range{ 0, 15000 }, ///< Possible range of the starting position of range queries.
	range_query_length_range{ 0, 5000 }, ///< Possible range of the length of range queries.
	modification_position_range{ 0, 15000 }, ///< Possible range of modification starting positions.
	modification_length_range{ 0, 5000 }, ///< Possible range of modification lengths.

	op_range{ 0, static_cast<std::size_t>(test_op::max_enum) - 1 }; ///< Possible range of test operations.

constexpr std::size_t max_num_ranges = 100000; ///< The maximum number of ranges before no new ranges can be added.

/// The fuzz test class.
class overlapping_range_test : public cp::fuzz_test {
public:
	/// Returns the name of this test.
	std::u8string_view get_name() const override {
		return u8"overlapping_range_test";
	}

	/// Logs the number of ranges.
	void log_status(cp::logger::log_entry &entry) const override {
		entry << "Num ranges: " << _reference.size();
	}

	/// Runs one iteration of this test.
	void iterate() override {
		bool has_error = false;
		auto error = [&]() {
			has_error = true;
			return cp::logger::get().log_error(CP_HERE);
		};

		bool is_modification = false;
		auto op = static_cast<test_op>(random_int(op_range));
		switch (op) {
		case test_op::insert_ranges_before:
			if (_reference.size() < max_num_ranges) {
				std::size_t count = random_int(insert_count_range);
				for (std::size_t i = 0; i < count; ++i) {
					range insert_rng;
					insert_rng.begin = random_int(position_range);
					insert_rng.length = random_int(length_range);
					insert_rng.value = random_int(value_range);

					_ranges.insert_range_before(insert_rng.begin, insert_rng.length, insert_rng.value);
					bool inserted = false;
					for (auto iter = _reference.begin(); iter != _reference.end(); ++iter) {
						if (iter->begin >= insert_rng.begin) {
							inserted = true;
							_reference.insert(iter, insert_rng);
							break;
						}
					}
					if (!inserted) {
						_reference.emplace_back(insert_rng);
					}
				}
				is_modification = true;
			}
			// otherwise there are too many ranges; don't insert
			break;
		case test_op::insert_ranges_after:
			if (_reference.size() < max_num_ranges) {
				std::size_t count = random_int(insert_count_range);
				for (std::size_t i = 0; i < count; ++i) {
					range insert_rng;
					insert_rng.begin = random_int(position_range);
					insert_rng.length = random_int(length_range);
					insert_rng.value = random_int(value_range);

					_ranges.insert_range_after(insert_rng.begin, insert_rng.length, insert_rng.value);
					bool inserted = false;
					for (auto iter = _reference.begin(); iter != _reference.end(); ++iter) {
						if (iter->begin > insert_rng.begin) {
							inserted = true;
							_reference.insert(iter, insert_rng);
							break;
						}
					}
					if (!inserted) {
						_reference.emplace_back(insert_rng);
					}
				}
				is_modification = true;
			}
			// otherwise there are too many ranges; don't insert
			break;
		case test_op::erase_ranges:
			{
				// generate indices
				std::size_t count = std::min(random_int(erase_count_range), _reference.size());
				std::pair<std::size_t, std::size_t> id_range{ 0, _reference.size() - 1 };
				std::vector<std::size_t> indices;
				indices.reserve(count);
				for (std::size_t i = 0; i < count; ++i) {
					indices.emplace_back(random_int(id_range));
				}
				std::sort(indices.begin(), indices.end());
				indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

				// erase from ranges
				auto it = _ranges.begin();
				auto erase_index = indices.begin();
				for (std::size_t i = 0; it != _ranges.end() && erase_index != indices.end(); ++i) {
					auto current_it = it;
					++it;
					if (i == *erase_index) {
						_ranges.erase(current_it);
						++erase_index;
					}
				}

				// erase from reference
				for (std::size_t i = 0; i < indices.size(); ++i) {
					_reference.erase(_reference.begin() + (indices[i] - i));
				}

				is_modification = true;
				break;
			}
		case test_op::on_modification:
			{
				std::size_t pos = random_int(modification_position_range);
				std::size_t erase_len = random_int(modification_length_range);
				std::size_t insert_len = random_int(modification_length_range);

				_ranges.on_modification(pos, erase_len, insert_len);

				// update reference
				std::size_t
					erase_end = pos + erase_len,
					insert_end = pos + insert_len,
					insert_diff = insert_len - erase_len;
				for (auto iter = _reference.begin(); iter != _reference.end(); ) {
					std::size_t iter_end = iter->begin + iter->length;
					if (iter->begin < pos) {
						if (iter_end > pos) {
							if (iter_end > erase_end) {
								iter->length += insert_len - erase_len;
							} else {
								iter->length -= iter_end - pos;
							}
						}
					} else {
						if (iter_end <= erase_end) {
							iter = _reference.erase(iter);
							continue;
						} else if (iter->begin < erase_end) {
							iter->length -= erase_end - iter->begin;
							iter->begin = insert_end;
						} else {
							iter->begin += insert_diff;
						}
					}
					++iter;
				}

				is_modification = true;
				break;
			}

		case test_op::query_first_ending_after:
			{
				std::size_t position = random_int(point_query_position_range);
				auto result = _ranges.find_first_range_ending_after(position);
				range insert_rng;
				if (result.get_iterator() != _ranges.end()) {
					insert_rng = range(
						result.get_range_start(),
						result.get_iterator()->length,
						result.get_iterator()->value
					);
				}

				bool found = false;
				for (auto ref_iter = _reference.begin(); ref_iter != _reference.end(); ++ref_iter) {
					if (ref_iter->begin + ref_iter->length > position) {
						if (*ref_iter != insert_rng) {
							error() << "incorrect \"first after \" query result";
						}
						found = true;
						break;
					}
				}
				if (found == (result.get_iterator() == _ranges.end())) {
					error() << "incorrect \"first after \" query result";
				}

				break;
			}
		case test_op::query_point:
			{
				std::size_t position = random_int(point_query_position_range);
				std::vector<range> found_ranges;
				auto result = _ranges.find_intersecting_ranges(position);
				while (result.begin.get_iterator() != result.end.get_iterator()) {
					found_ranges.emplace_back(
						result.begin.get_range_start(),
						result.begin.get_iterator()->length,
						result.begin.get_iterator()->value
					);
					result.begin = _ranges.find_next_range_ending_at_or_after(position, result.begin);
				}

				// check the results
				auto iter = found_ranges.begin();
				for (auto ref_iter = _reference.begin(); ref_iter != _reference.end(); ++ref_iter) {
					if (ref_iter->begin <= position && ref_iter->begin + ref_iter->length >= position) {
						if (iter == found_ranges.end()) {
							error() << "ranges missed by a point query";
							break;
						}
						if (*iter != *ref_iter) {
							error() << "incorrect point query result entry";
						}
						++iter;
					}
				}
				if (iter != found_ranges.end()) {
					error() << "too many ranges from a point query";
				}

				break;
			}
		case test_op::query_range:
			{
				std::size_t position = random_int(range_query_position_range);
				std::size_t length = random_int(range_query_length_range);
				std::size_t end = position + length;
				std::vector<range> found_ranges;
				auto result = _ranges.find_intersecting_ranges(position, end);
				while (result.before_begin.get_iterator() != result.begin.get_iterator()) {
					found_ranges.emplace_back(
						result.before_begin.get_range_start(),
						result.before_begin.get_iterator()->length,
						result.before_begin.get_iterator()->value
					);
					result.before_begin = _ranges.find_next_range_ending_at_or_after(position, result.before_begin);
				}
				while (result.begin.get_iterator() != result.end.get_iterator()) {
					found_ranges.emplace_back(
						result.begin.get_range_start(),
						result.begin.get_iterator()->length,
						result.begin.get_iterator()->value
					);
					result.begin.move_next();
				}

				// check the results
				auto iter = found_ranges.begin();
				for (auto ref_iter = _reference.begin(); ref_iter != _reference.end(); ++ref_iter) {
					if (ref_iter->begin <= end && ref_iter->begin + ref_iter->length >= position) {
						if (iter == found_ranges.end()) {
							error() << "ranges missed by a range query";
							break;
						}
						if (*iter != *ref_iter) {
							error() << "incorrect range query result entry";
						}
						++iter;
					}
				}
				if (iter != found_ranges.end()) {
					error() << "too many ranges from a range query";
				}

				break;
			}
		}

		// validate
		if (is_modification) {
			auto iter = _ranges.begin_position();
			auto ref_iter = _reference.begin();
			while (iter.get_iterator() != _ranges.end() && ref_iter != _reference.end()) {
				if (iter.get_range_start() != ref_iter->begin) {
					error() << "position " << ref_iter - _reference.begin() << ": incorrect begin position";
				}
				if (iter.get_iterator()->length != ref_iter->length) {
					error() << "position " << ref_iter - _reference.begin() << ": incorrect length";
				}
				if (iter.get_iterator()->value != ref_iter->value) {
					error() << "position " << ref_iter - _reference.begin() << ": incorrect length";
				}
				iter.move_next();
				++ref_iter;
			}
			if (iter.get_iterator() != _ranges.end()) {
				error() << "more ranges than reference";
			} else if (ref_iter != _reference.end()) {
				error() << "less ranges than reference";
			}
		}

		cp::assert_true_logical(!has_error, "errorneous range implementation");
	}
protected:
	cp::editors::overlapping_range_registry<test_t> _ranges; ///< Ranges.
	std::vector<range> _reference; ///< Reference ranges.
};

/// Entry point of the test.
int main(int argc, char **argv) {
	return cp::fuzz_test::main(argc, argv, std::make_unique<overlapping_range_test>());
}
