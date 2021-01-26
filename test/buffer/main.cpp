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

using namespace std;

using namespace codepad;
using namespace codepad::editors;
using namespace codepad::editors::code;

std::atomic_bool keep_running; ///< This is set to \p false if SIGINT is encountered.

uniform_real_distribution<double> prob_dist(0.0, 1.0);

/// Generates and returns a series of codepoints and encodes them as UTF8.
template <typename Rnd> byte_string generate_random_utf8_string(size_t length, Rnd &random) {
	uniform_int_distribution<codepoint> dist(0, 0x10FFFF - 0x800);
	basic_string<codepoint> str;
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
			codepoint cp = dist(random);
			if (cp >= 0xD800) {
				cp += 0x800;
			}
			str.push_back(cp);
		}
	}
	byte_string res;
	for (codepoint cp : str) {
		res.append(encodings::utf8::encode_codepoint(cp));
	}
	return res;
}

/// Generates a random series of bytes.
template <typename Rnd> byte_string generate_random_string(size_t length, Rnd &random) {
	byte_string res;
	uniform_int_distribution<int> dist(0, 255);
	for (size_t i = 0; i < length; ++i) {
		res.push_back(static_cast<std::byte>(dist(random)));
	}
	return res;
}

/// Generates a series of positions in the text used for modifications. All positions are guaranteed to be at
/// character boundaries.
template <typename Rnd> vector<pair<size_t, size_t>> get_modify_positions_boundary(
	size_t count, const buffer&, const interpretation &interp, Rnd &random
) {
	uniform_int_distribution<size_t> caretpos_dist(0, interp.get_linebreaks().num_chars());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	caret_set cset;
	for (size_t i = 0; i < carets.size(); i += 2) {
		// 10% chance: don't erase anything
		if (prob_dist(random) < 0.1) {
			carets[i + 1] = carets[i];
		}
		cset.add(caret_set::entry({carets[i], carets[i + 1]}, caret_data()));
	}

	vector<pair<size_t, size_t>> cs;
	interpretation::character_position_converter cvt(interp);
	for (const auto &pair : cset.carets) {
		assert_true_logical(pair.first.caret <= pair.first.selection);
		size_t p1 = cvt.character_to_byte(pair.first.caret), p2 = cvt.character_to_byte(pair.first.selection);
		cs.emplace_back(p1, p2);
	}
	return cs;
}
/// Generates a series of completely random positions in the text used for modifications.
template <typename Rnd> vector<pair<size_t, size_t>> get_modify_positions_random(
	size_t count, const buffer &buf, const interpretation&, Rnd &random
) {
	uniform_int_distribution<size_t> caretpos_dist(0, buf.length());
	// create carets
	std::vector<size_t> carets;
	for (size_t i = 0; i < count; ++i) {
		carets.push_back(caretpos_dist(random));
		carets.push_back(caretpos_dist(random));
	}
	sort(carets.begin(), carets.end());
	vector<pair<size_t, size_t>> cs;
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

	auto global_log = std::make_unique<logger>();
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::console_sink>());
	global_log->sinks.emplace_back(std::make_unique<logger_sinks::file_sink>("buffer_test.log"));
	logger::set_current(std::move(global_log));

	initialize(argc, argv);

	manager man;

	default_random_engine eng(123456);
	auto buf = man.buffers.new_file();
	std::shared_ptr<interpretation> interp = man.buffers.open_interpretation(buf, man.encodings.get_default());

	caret_set cset;
	cset.reset();
	interp->on_insert(cset, generate_random_string(1000000, eng), nullptr);
	assert_true_logical(interp->check_integrity());

	uniform_int_distribution<size_t>
		ncarets_dist(1, 100), insertlen_dist(0, 3000);
	uniform_int_distribution<int> bool_dist(0, 1);
	size_t idx = 0;
	while (keep_running) {
		logger::get().log_info(CP_HERE) <<
			"document length: " << buf->length() << " bytes, " << interp->get_linebreaks().num_chars() << " chars";

		vector<pair<size_t, size_t>> positions;
		if (bool_dist(eng)) {
			positions = get_modify_positions_random(ncarets_dist(eng), *buf, *interp, eng);
		} else {
			positions = get_modify_positions_boundary(ncarets_dist(eng), *buf, *interp, eng);
		}
		vector<byte_string> inserts;
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
		logger::get().log_info(CP_HERE) << "modifications: " << positions.size();
		{
			buffer::modifier mod;
			mod.begin(*buf, nullptr);
			for (size_t i = 0; i < positions.size(); ++i) {
				mod.modify(positions[i].first, positions[i].second - positions[i].first, inserts[i]);
			}
			buffer::edit dummy;
			mod.end_custom(dummy); // no history; otherwise all memory will be eaten
		}
		logger::get().log_info(CP_HERE) << "checking edit " << idx;
		assert_true_logical(interp->check_integrity());
		++idx;
	}

	logger::get().log_info(CP_HERE) << "exiting normally";
	return 0;
}
