// Copyright (c) the Codepad contributors. All rights reserved.
// Licensed under the Apache License, Version 2.0. See LICENSE.txt in the project root for license information.

/// \file
/// Stress-test for the buffer. This file randomly inserts and removes bytes and characters from the buffer and then
/// decodes it to check that the contents haven't been corrupted.

#include <random>
#include <atomic>
#include <csignal>

#include <codepad/core/logger_sinks.h>
#include <codepad/editors/manager.h>
#include <codepad/editors/code/interpretation.h>

namespace cp = codepad;

std::atomic_bool keep_running; ///< This is set to \p false if SIGINT is encountered.

std::uniform_real_distribution<double> prob_dist(0.0, 1.0);

/// Generates and returns a series of codepoints and encodes them as UTF8.
template <typename Rnd> cp::editors::byte_string generate_random_utf8_string(size_t length, Rnd &random) {
	std::uniform_int_distribution<cp::codepoint> dist(0, 0x10FFFF - 0x800);
	std::basic_string<cp::codepoint> str;
	for (size_t i = 0; i < length; ++i) {
		// 2% possibility to generate a line break character
		if (prob_dist(random) < 0.02) {
			double x = prob_dist(random);
			if (x < 0.3) {
				str.push_back(U'\r');
			} else if (x < 0.6) {
				str.push_back(U'\n');
			} else {
				// this means that `length` is just a suggestion, but it doesn't break anything
				str.push_back(U'\r');
				str.push_back(U'\n');
			}
		} else {
			cp::codepoint cp = dist(random);
			if (cp >= 0xD800) {
				cp += 0x800;
			}
			str.push_back(cp);
		}
	}
	cp::editors::byte_string res;
	for (cp::codepoint cp : str) {
		res.append(cp::encodings::utf8::encode_codepoint(cp));
	}
	return res;
}
/// Generates a random series of bytes.
template <typename Rnd> cp::editors::byte_string generate_random_string(size_t length, Rnd &random) {
	cp::editors::byte_string res;
	std::uniform_int_distribution<int> dist(0, 255);
	for (size_t i = 0; i < length; ++i) {
		res.push_back(static_cast<std::byte>(dist(random)));
	}
	return res;
}

/// Generates a series of positions in the text used for modifications. All positions are guaranteed to be at
/// character boundaries.
template <typename Rnd> std::vector<std::pair<size_t, size_t>> get_modify_positions_boundary(
	size_t count, const cp::editors::buffer&, const cp::editors::code::interpretation &interp, Rnd &random
) {
	std::uniform_int_distribution<size_t> caretpos_dist(0, interp.get_linebreaks().num_chars());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	cp::editors::code::caret_set cset;
	for (size_t i = 0; i < carets.size(); i += 2) {
		// 10% chance: don't erase anything
		if (prob_dist(random) < 0.1) {
			carets[i + 1] = carets[i];
		}
		cset.add(cp::editors::code::caret_set::entry(
			{carets[i], carets[i + 1]}, cp::editors::code::caret_data()
		));
	}

	std::vector<std::pair<size_t, size_t>> cs;
	cp::editors::code::interpretation::character_position_converter cvt(interp);
	for (const auto &pair : cset.carets) {
		cp::assert_true_logical(pair.first.caret <= pair.first.selection);
		size_t p1 = cvt.character_to_byte(pair.first.caret), p2 = cvt.character_to_byte(pair.first.selection);
		cs.emplace_back(p1, p2);
	}
	return cs;
}
/// Generates a series of completely random positions in the text used for modifications.
template <typename Rnd> std::vector<std::pair<size_t, size_t>> get_modify_positions_random(
	size_t count, const cp::editors::buffer &buf, const cp::editors::code::interpretation&, Rnd &random
) {
	std::uniform_int_distribution<size_t> caretpos_dist(0, buf.length());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	std::vector<std::pair<size_t, size_t>> cs;
	for (size_t i = 0; i < carets.size(); i += 2) {
		// 10% chance: don't erase anything
		if (prob_dist(random) < 0.1) {
			carets[i + 1] = carets[i];
		}
		cs.emplace_back(carets[i], carets[i + 1]);
	}
	return cs;
}

/// Entry point of the test.
int main(int argc, char **argv) {
	// this enables us to terminate normally and check if there are any memory leaks
	keep_running = true;
	std::signal(SIGINT, [](int) {
		keep_running = false;
	});

	auto global_log = std::make_unique<cp::logger>();
	global_log->sinks.emplace_back(std::make_unique<cp::logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<cp::logger_sinks::file_sink>("buffer_test.log"));
	cp::logger::set_current(std::move(global_log));

	cp::initialize(argc, argv);

	cp::editors::buffer_manager buf_man;
	cp::editors::code::encoding_registry encodings;

	std::default_random_engine eng(123456);
	auto buf = buf_man.new_file();
	auto interp = buf_man.open_interpretation(*buf, encodings.get_default());

	// stored values from `modification_decoded`
	std::size_t start_char_beforemod, past_end_char_beforemod, num_chars_beforemod;
	interp->modification_decoded +=
		[&](cp::editors::code::interpretation::modification_decoded_info &info) {
			start_char_beforemod = info.start_character;
			past_end_char_beforemod = info.past_end_character;
			if (
				(info.buffer_info.bytes_erased.length() > 0 || info.buffer_info.bytes_inserted.length() > 0) &&
				info.past_end_line_column.position_in_line > info.past_end_line_column.line_iterator->nonbreak_chars
			) {
				++past_end_char_beforemod;
			}
			num_chars_beforemod = interp->get_linebreaks().num_chars();
		};
	interp->end_modification +=
		[&](cp::editors::code::interpretation::end_modification_info &info) {
			// validate the character indices in `info`
			auto [begin_line_col, begin_char] =
				interp->get_linebreaks().get_line_and_column_and_char_of_codepoint(info.start_codepoint);
			auto [end_line_col, end_char] =
				interp->get_linebreaks().get_line_and_column_and_char_of_codepoint(info.past_end_codepoint);
			if (
				(info.buffer_info.bytes_erased.length() > 0 || info.buffer_info.bytes_inserted.length() > 0) &&
				end_line_col.position_in_line > end_line_col.line_iterator->nonbreak_chars
			) {
				++end_char;
			}
			std::size_t num_chars = interp->get_linebreaks().num_chars();
			std::size_t
				after_end_beforemod = num_chars_beforemod - past_end_char_beforemod,
				after_end = num_chars - end_char;
			std::size_t
				expected_beg = std::min(begin_char, start_char_beforemod),
				expected_after_end = std::min(after_end_beforemod, after_end);
			std::size_t
				expected_removed_chars = num_chars_beforemod - (expected_after_end + expected_beg),
				expected_inserted_chars = num_chars - (expected_after_end + expected_beg);

			cp::assert_true_logical(info.start_character == expected_beg, "incorrect start character position");
			cp::assert_true_logical(
				info.removed_characters == expected_removed_chars, "incorrect erased character count"
			);
			cp::assert_true_logical(
				info.inserted_characters == expected_inserted_chars, "incorrect inserted character count"
			);
		};

	cp::editors::code::caret_set cset;
	cset.reset();
	interp->on_insert(cset, generate_random_string(1000000, eng), nullptr);
	cp::assert_true_logical(interp->check_integrity());

	std::uniform_int_distribution<size_t>
		ncarets_dist(1, 100), insertlen_dist(0, 3000);
	std::uniform_int_distribution<int> bool_dist(0, 1);
	for (std::size_t idx = 0; keep_running; ++idx) {
		cp::logger::get().log_info(CP_HERE) <<
			"document length: " << buf->length() << " bytes, " << interp->get_linebreaks().num_chars() << " chars";

		// generate random positions and strings for the edit
		std::vector<std::pair<size_t, size_t>> positions;
		if (bool_dist(eng)) {
			positions = get_modify_positions_random(ncarets_dist(eng), *buf, *interp, eng);
		} else {
			positions = get_modify_positions_boundary(ncarets_dist(eng), *buf, *interp, eng);
		}
		std::vector<cp::editors::byte_string> inserts;
		for (size_t i = 0; i < positions.size(); ++i) {
			double r = prob_dist(eng);
			if (r < 0.1) { // 10% chance: don't insert anything
				inserts.emplace_back();
			} else if (r < 0.55) { // 45% chance: insert ranodm string
				inserts.emplace_back(generate_random_string(insertlen_dist(eng), eng));
			} else { // insert utf-8 string
				inserts.emplace_back(generate_random_utf8_string(insertlen_dist(eng), eng));
			}
		}
		cp::logger::get().log_info(CP_HERE) << "modifications: " << positions.size();

		// perform the edit
		{
			cp::editors::buffer::modifier mod(*buf, nullptr);
			mod.begin();
			for (size_t i = 0; i < positions.size(); ++i) {
				mod.modify(positions[i].first, positions[i].second - positions[i].first, inserts[i]);
			}
			cp::editors::buffer::edit dummy;
			mod.end_custom(dummy); // no history; otherwise all memory will be eaten
		}

		// validate everything
		cp::logger::get().log_info(CP_HERE) << "checking edit " << idx;
		cp::assert_true_logical(interp->check_integrity());
	}

	cp::logger::get().log_info(CP_HERE) << "exiting normally";
	return 0;
}
