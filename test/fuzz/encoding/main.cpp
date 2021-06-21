// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Stress-test for encodings. This test decodes randomly-generated strings forwards and backwards and checks that
/// the results are consistent.

#include <random>
#include <atomic>
#include <csignal>
#include <chrono>

#include <codepad/core/fuzz_test.h>
#include <codepad/core/assert.h>
#include <codepad/core/encodings.h>

namespace cp = codepad;

using encoding = cp::encodings::utf32<>; ///< The encoding to test.

std::pair<std::size_t, std::size_t>
	clip_count_range{ 0, 20 },
	clip_length_range{ 0,3000 }; ///< Possible range of the length of inserted clips.

/// Encoding fuzz test.
class encoding_test : public cp::fuzz_test {
public:
	/// Returns the name of this test.
	std::u8string_view get_name() const override {
		return u8"encoding_test";
	}

	/// Performs one random operation.
	void iterate() override {
		// generate random string for testing
		std::size_t num_strings = random_int(clip_count_range);
		cp::byte_string str;
		for (std::size_t i = 0; i < num_strings; ++i) {
			std::size_t length = random_int(clip_length_range);
			if (random_bool()) {
				str.append(generate_random_encoded_string(length));
			} else {
				str.append(generate_random_string(length));
			}
		}
		// decode forward & backward
		auto it = str.begin();
		while (it != str.end()) {
			// test forward
			auto next1 = it, next2 = it;
			cp::codepoint forward, backward;
			bool valid1 = encoding::next_codepoint(next1, str.end(), forward);
			bool valid2 = encoding::next_codepoint(next2, str.end());
			cp::assert_true_logical(
				next1 == next2, "inconsistent results between the two next_codepoint() overloads"
			);
			if (valid1) {
				cp::assert_true_logical(valid2, "next_codepoint() without a codepoint incorrectly reports invalid");
			}
			// test backward
			if ((next1 - str.begin()) % encoding::get_word_length() == 0) {
				auto prev1 = next1, prev2 = next1;
				bool valid_back1 = encoding::previous_codepoint(prev1, str.begin(), backward);
				bool valid_back2 = encoding::previous_codepoint(prev2, str.begin());
				cp::assert_true_logical(
					prev1 == it, "incorrect result from previous_codepoint() with a codepoint"
				);
				cp::assert_true_logical(
					prev2 == it, "incorrect result from previous_codepoint() without a codepoint"
				);
				cp::assert_true_logical(
					valid_back1 == valid1, "incorrect validity result from previous_codepoint() with a codepoint"
				);
				cp::assert_true_logical(
					valid_back2 == valid2, "incorrect validity result from previous_codepoint() without a codepoint"
				);
				cp::assert_true_logical(
					forward == backward, "inconsistent results from forward and backward decoding"
				);
			}
			it = next1;
		}
	}


	/// Generates and returns a series of codepoints and encodes them using the given encoding.
	[[nodiscard]] cp::byte_string generate_random_encoded_string(std::size_t length) {
		constexpr std::pair<cp::codepoint, cp::codepoint> _codepoint_dist(
			0,
			cp::unicode::codepoint_max -
			(cp::unicode::codepoint_invalid_max + 1 - cp::unicode::codepoint_invalid_min)
		);

		cp::byte_string res;
		for (std::size_t i = 0; i < length; ++i) {
			cp::codepoint c = random_int(_codepoint_dist);
			if (c >= cp::unicode::codepoint_invalid_min) {
				c += cp::unicode::codepoint_invalid_max + 1 - cp::unicode::codepoint_invalid_min;
			}
			res.append(encoding::encode_codepoint(c));
		}
		return res;
	}
	/// Generates a random series of bytes.
	cp::byte_string generate_random_string(std::size_t length) {
		constexpr std::pair<int, int> _byte_dist{ 0, 255 };

		cp::byte_string res;
		for (std::size_t i = 0; i < length; ++i) {
			res.push_back(static_cast<std::byte>(random_int(_byte_dist)));
		}
		return res;
	}
};

/// Entry point of the test.
int main(int argc, char **argv) {
	return cp::fuzz_test::main(argc, argv, std::make_unique<encoding_test>());
}
