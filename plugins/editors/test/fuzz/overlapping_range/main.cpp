// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Fuzz-test for the range registry.

#include <csignal>
#include <algorithm>
#include <random>
#include <atomic>

#include <codepad/core/logger_sinks.h>
#include <codepad/editors/overlapping_range_registry.h>

using namespace codepad;
using namespace codepad::editors;

std::atomic_bool keep_running; ///< This is set to \p false if SIGINT is encountered.

/// Uniform real distribution between 0 and 1 used for operations with specific probabilities.
std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

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

/// Entry point of the test.
int main(int argc, char **argv) {
	// this enables us to terminate normally and check if there are any memory leaks
	keep_running = true;
	std::signal(SIGINT, [](int) {
		keep_running = false;
		});

	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("overlapping_range_test.log"));
	logger::set_current(std::move(global_log));

	initialize(argc, argv);

	std::default_random_engine eng(123456);
	std::uniform_int_distribution<std::size_t>
		insert_count_dist(500, 2000),
		erase_count_dist(100, 1000),

		position_dist(0, 10000),
		length_dist(0, 3000),

		point_query_position_dist(0, 15000),
		range_query_position_dist(0, 15000),
		range_query_length_dist(0, 5000),
		modification_position_dist(0, 15000),
		modification_length_dist(0, 5000),

		op_dist(0, 5);
	std::uniform_int_distribution<int> bool_dist(0, 1);
	std::uniform_int_distribution<test_t> value_dist(
		std::numeric_limits<test_t>::min(), std::numeric_limits<test_t>::max()
	);

	overlapping_range_registry<test_t> ranges;
	std::vector<range> reference;

	for (std::size_t idx = 0; keep_running; ++idx) {
		logger::get().log_info(CP_HERE) << "iteration " << idx << ", num ranges: " << reference.size();

		bool has_error = false;
		auto error = [&]() {
			has_error = true;
			return logger::get().log_error(CP_HERE);
		};

		bool is_modification = false;
		std::size_t op = op_dist(eng);
		switch (op) {
		case 0: // insert ranges
			if (reference.size() < 100000) {
				std::size_t count = insert_count_dist(eng);
				logger::get().log_debug(CP_HERE) << "inserting " << count << " ranges";
				for (std::size_t i = 0; i < count; ++i) {
					range rng;
					rng.begin = position_dist(eng);
					rng.length = length_dist(eng);
					rng.value = value_dist(eng);

					ranges.insert_range(rng.begin, rng.length, rng.value);
					bool inserted = false;
					for (auto iter = reference.begin(); iter != reference.end(); ++iter) {
						if (iter->begin >= rng.begin) {
							inserted = true;
							reference.insert(iter, rng);
							break;
						}
					}
					if (!inserted) {
						reference.emplace_back(rng);
					}
				}
				is_modification = true;
				break;
			}
			[[fallthrough]]; // too many ranges; erase some instead
		case 1: // erase ranges
			{
				// generate indices
				std::size_t count = std::min(erase_count_dist(eng), reference.size());
				std::uniform_int_distribution<std::size_t> id_dist(0, reference.size() - 1);
				std::vector<std::size_t> indices;
				indices.reserve(count);
				for (std::size_t i = 0; i < count; ++i) {
					indices.emplace_back(id_dist(eng));
				}
				std::sort(indices.begin(), indices.end());
				indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
				logger::get().log_debug(CP_HERE) << "erasing " << indices.size() << " ranges";
				
				// erase from ranges
				auto it = ranges.begin();
				auto erase_index = indices.begin();
				for (std::size_t i = 0; it != ranges.end() && erase_index != indices.end(); ++i) {
					auto current_it = it;
					++it;
					if (i == *erase_index) {
						ranges.erase(current_it);
						++erase_index;
					}
				}

				// erase from reference
				for (std::size_t i = 0; i < indices.size(); ++i) {
					reference.erase(reference.begin() + (indices[i] - i));
				}

				is_modification = true;
				break;
			}
		case 2: // on_modification
			{
				std::size_t pos = modification_position_dist(eng);
				std::size_t erase_len = modification_length_dist(eng);
				std::size_t insert_len = modification_length_dist(eng);
				logger::get().log_debug(CP_HERE) <<
					"modification at " << pos << " -" << erase_len << " +" << insert_len;

				ranges.on_modification(pos, erase_len, insert_len);

				// update reference
				std::size_t
					erase_end = pos + erase_len,
					insert_end = pos + insert_len,
					insert_diff = insert_len - erase_len;
				for (auto iter = reference.begin(); iter != reference.end(); ) {
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
							iter = reference.erase(iter);
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

		case 3: // "first ending after" query
			{
				std::size_t position = point_query_position_dist(eng);
				logger::get().log_debug(CP_HERE) << "\"first ending after\" query at " << position;

				auto result = ranges.find_first_range_ending_after(position);
				range rng;
				if (result.get_iterator() != ranges.end()) {
					rng = range(
						result.get_range_start(),
						result.get_iterator()->length,
						result.get_iterator()->value
					);
				}

				bool found = false;
				for (auto ref_iter = reference.begin(); ref_iter != reference.end(); ++ref_iter) {
					if (ref_iter->begin + ref_iter->length > position) {
						if (*ref_iter != rng) {
							error() << "incorrect \"first after \" query result";
						}
						found = true;
						break;
					}
				}
				if (found == (result.get_iterator() == ranges.end())) {
					error() << "incorrect \"first after \" query result";
				}

				break;
			}
		case 4: // point query
			{
				std::size_t position = point_query_position_dist(eng);
				logger::get().log_debug(CP_HERE) << "point query at " << position;

				std::vector<range> found_ranges;
				auto result = ranges.find_intersecting_ranges(position);
				while (result.begin.get_iterator() != result.end.get_iterator()) {
					found_ranges.emplace_back(
						result.begin.get_range_start(),
						result.begin.get_iterator()->length,
						result.begin.get_iterator()->value
					);
					result.begin = ranges.find_next_range_ending_at_or_after(position, result.begin);
				}

				// check the results
				auto iter = found_ranges.begin();
				for (auto ref_iter = reference.begin(); ref_iter != reference.end(); ++ref_iter) {
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
		case 5: // range query
			{
				std::size_t position = range_query_position_dist(eng);
				std::size_t length = range_query_length_dist(eng);
				std::size_t end = position + length;
				logger::get().log_debug(CP_HERE) << "range query [" << position << ", " << end << "]";

				std::vector<range> found_ranges;
				auto result = ranges.find_intersecting_ranges(position, end);
				while (result.before_begin.get_iterator() != result.begin.get_iterator()) {
					found_ranges.emplace_back(
						result.before_begin.get_range_start(),
						result.before_begin.get_iterator()->length,
						result.before_begin.get_iterator()->value
					);
					result.before_begin = ranges.find_next_range_ending_at_or_after(position, result.before_begin);
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
				for (auto ref_iter = reference.begin(); ref_iter != reference.end(); ++ref_iter) {
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
			auto iter = ranges.begin_position();
			auto ref_iter = reference.begin();
			while (iter.get_iterator() != ranges.end() && ref_iter != reference.end()) {
				if (iter.get_range_start() != ref_iter->begin) {
					error() << "position " << ref_iter - reference.begin() << ": incorrect begin position";
				}
				if (iter.get_iterator()->length != ref_iter->length) {
					error() << "position " << ref_iter - reference.begin() << ": incorrect length";
				}
				if (iter.get_iterator()->value != ref_iter->value) {
					error() << "position " << ref_iter - reference.begin() << ": incorrect length";
				}
				iter.move_next();
				++ref_iter;
			}
			if (iter.get_iterator() != ranges.end()) {
				error() << "more ranges than reference";
			} else if (ref_iter != reference.end()) {
				error() << "less ranges than reference";
			}

			/*auto entry = std::move(logger::get().log_debug(CP_HERE) << "ranges:\n");
			for (const auto &rng : reference) {
				entry << "  [" << rng.begin << ", " << rng.begin + rng.length << ") - <" << rng.value << ">\n";
			}*/
		}

		assert_true_logical(!has_error, "errorneous range implementation");
	}

	logger::get().log_info(CP_HERE) << "exiting normally";
	return 0;
}
